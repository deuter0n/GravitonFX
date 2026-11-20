// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "dsp/InputStage.h"

#include <algorithm>
#include <cmath>

namespace gfx {

namespace {

constexpr double kFadeSeconds = 0.0025;    // each side of a catch-up
constexpr double kMaxExcessSeconds = 0.040; // beyond this, catch up at once

}  // namespace

void InputStage::prepare(double captureRate, double outputRate, size_t packetFrames, size_t maxBlockFrames)
{
    captureRate_ = captureRate;
    outputRate_ = outputRate;
    packetFrames_ = packetFrames;
    fadeStep_ = static_cast<float>(1.0 / (kFadeSeconds * outputRate));

    // Room for one block at the fastest possible ratio.
    const double maxRatio = captureRate / outputRate * (1.0 + DriftController::kMaxTrim);
    scratch_.assign(static_cast<size_t>(std::ceil(maxRatio * static_cast<double>(maxBlockFrames))) + 4, 0.0f);
    start();
}

void InputStage::start()
{
    reserve_.reset();
    drift_.reset();
    resampler_.reset();
    state_ = State::Priming;
    catchUpPending_ = false;
    overrunPending_ = false;
    fadeGain_ = 0.0f;
    fadeTarget_ = 1.0f;
    stats_ = InputStats{};
    stats_.reserveMs = reserve_.ms();
    stats_.targetMs = targetSeconds() * 1000.0;
}

size_t InputStage::primeFrames() const
{
    return static_cast<size_t>(reserve_.ms() / 1000.0 * captureRate_) + packetFrames_;
}

// Measured after each read, the fill steps down from reserve + packet - block
// (just after a packet arrived) to reserve (just before the next one), so its
// average is reserve + (packet - block) / 2.
double InputStage::targetSeconds() const
{
    const double sawtooth = static_cast<double>(packetFrames_) - blockInputFrames_;
    return reserve_.ms() / 1000.0 + std::max(0.0, sawtooth) / 2.0 / captureRate_;
}

void InputStage::playSilence(float* out, size_t frames)
{
    std::fill(out, out + frames, 0.0f);
    stats_.playing = false;
}

void InputStage::applyFade(float* out, size_t frames)
{
    for (size_t i = 0; i < frames; ++i) {
        if (fadeGain_ < fadeTarget_) {
            fadeGain_ = std::min(fadeTarget_, fadeGain_ + fadeStep_);
        } else if (fadeGain_ > fadeTarget_) {
            fadeGain_ = std::max(fadeTarget_, fadeGain_ - fadeStep_);
        }
        out[i] *= fadeGain_;
    }
}

void InputStage::pull(SpscRing& ring, const CaptureSignals& signals, float* out, size_t frames)
{
    // New events from the capture side.
    if (signals.discontinuities != seen_.discontinuities) {
        stats_.discontinuities += signals.discontinuities - seen_.discontinuities;
    }
    if (signals.overruns != seen_.overruns && !overrunPending_) {
        // One stall = one dropout, however many writes failed. Audio was lost
        // inside the ring, so the catch-up clears it and primes from fresh,
        // continuous audio.
        stats_.overruns += 1;
        overrunPending_ = true;
        catchUpPending_ = state_ == State::Playing;
        if (state_ != State::Playing) {
            ring.clear();
            overrunPending_ = false;
        }
    }
    seen_ = signals;
    blockInputFrames_ = static_cast<double>(frames) * captureRate_ / outputRate_;

    switch (state_) {
    case State::Priming: {
        const size_t prime = primeFrames();
        if (ring.size() < prime) {
            playSilence(out, frames);
            return;
        }
        // Start playing in this same block, so the fill follows its planned
        // sawtooth from the first read.
        ring.skip(ring.size() - prime);  // planned catch-up, not a dropout
        resampler_.reset();
        drift_.resume(targetSeconds());
        fadeGain_ = 0.0f;
        fadeTarget_ = 1.0f;
        state_ = State::Playing;
        break;
    }

    case State::Starved: {
        playSilence(out, frames);
        idleSeen_ = idleSeen_ || signals.idle;
        if (ring.size() > starvedFill_) {  // audio is arriving again
            if (!idleSeen_) {
                stats_.underruns += 1;
                reserve_.grow();
                stats_.reserveMs = reserve_.ms();
            }
            state_ = State::Priming;
        }
        return;
    }

    case State::Playing:
        break;
    }

    // A catch-up fades out first, then drops audio, then fades back in.
    if (catchUpPending_ && fadeGain_ == 0.0f) {
        catchUpPending_ = false;
        if (overrunPending_) {
            ring.clear();
            overrunPending_ = false;
            state_ = State::Priming;
            playSilence(out, frames);
            return;
        }
        const size_t prime = primeFrames();
        if (ring.size() > prime) {
            ring.skip(ring.size() - prime);
        }
        drift_.resume(targetSeconds());
    }
    if (!catchUpPending_ && ring.size() > primeFrames() + static_cast<size_t>(kMaxExcessSeconds * captureRate_)) {
        catchUpPending_ = true;  // far too much buffered (e.g. a burst after a stall)
    }
    fadeTarget_ = catchUpPending_ ? 0.0f : 1.0f;

    const double ratio = captureRate_ / outputRate_ * (1.0 + drift_.trim());
    const size_t need = resampler_.inputNeeded(frames, ratio);
    if (need > scratch_.size() || ring.size() < need) {
        state_ = State::Starved;
        idleSeen_ = signals.idle;
        starvedFill_ = ring.size();
        catchUpPending_ = false;
        overrunPending_ = false;
        playSilence(out, frames);
        return;
    }

    ring.peek(scratch_.data(), need);
    ring.skip(resampler_.process(scratch_.data(), out, frames, ratio));
    applyFade(out, frames);
    stats_.playing = true;

    if (!catchUpPending_) {
        drift_.update(static_cast<double>(ring.size()) / captureRate_, targetSeconds(),
                      static_cast<double>(frames) / outputRate_);
    }
    stats_.fillMs = drift_.smoothedFillSeconds() * 1000.0;
    stats_.targetMs = targetSeconds() * 1000.0;
    stats_.reserveMs = reserve_.ms();
    stats_.driftPpm = drift_.trim() * 1e6;
    stats_.locked = drift_.locked();
}

}  // namespace gfx

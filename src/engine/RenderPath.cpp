// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "engine/RenderPath.h"

#include <algorithm>

namespace gfx {

namespace {
constexpr double kSourceFadeSeconds = 0.0025;  // around a source swap
constexpr double kOutputFadeSeconds = 0.010;   // before the device closes
// Many USB DACs mute themselves while they receive exact digital silence and
// un-mute with a click when sound starts. A one-step-of-16-bit square wave at
// half the sample rate (about -90 dBFS, far above hearing) keeps them awake.
constexpr float kKeepAliveLevel = 1.0f / 32768.0f;
}  // namespace

void RenderPath::prepare(double outputRate, int maxBlockFrames, const EngineSettings& settings)
{
    fs_ = outputRate;
    in_.assign(static_cast<size_t>(maxBlockFrames), 0.0f);
    test_.assign(static_cast<size_t>(maxBlockFrames), 0.0f);
    chain_.prepare(outputRate, settings.filter);
    limiter_.prepare(outputRate);
    tests_.prepare(outputRate);
    setParams(settings.filter, settings.limiter);
    fadeStep_ = static_cast<float>(1.0 / (kOutputFadeSeconds * outputRate));
    outputGain_ = 1.0f;
    sourceGain_ = 1.0f;
    fadeOutRequested_ = false;
    fadedOut_ = false;
    keepAlive_ = kKeepAliveLevel;
    testSeen_ = testSeq_.load();
    testKind_ = 0;
}

void RenderPath::setParams(const FilterParams& filter, bool limiter)
{
    cutoff_ = filter.cutoffHz;
    slope_ = filter.slope;
    subsonicHz_ = filter.subsonicHz;
    subsonicSlope_ = filter.subsonicSlope;
    gainDb_ = filter.gainDb;
    limiterOn_ = limiter;
}

void RenderPath::setSource(SourceSlot* slot)
{
    pending_ = slot;
    if (!current_.load()) {
        current_ = slot;  // nothing playing yet: no fade needed
    }
}

void RenderPath::render(float* mono, size_t frames)
{
    frames = std::min(frames, in_.size());

    // Settings.
    FilterParams p;
    p.cutoffHz = cutoff_.load(std::memory_order_relaxed);
    p.slope = slope_.load(std::memory_order_relaxed);
    p.subsonicHz = subsonicHz_.load(std::memory_order_relaxed);
    p.subsonicSlope = subsonicSlope_.load(std::memory_order_relaxed);
    p.gainDb = gainDb_.load(std::memory_order_relaxed);
    chain_.setTarget(p);
    limiter_.setEnabled(limiterOn_.load(std::memory_order_relaxed));

    // Test signal commands.
    const unsigned seq = testSeq_.load(std::memory_order_acquire);
    if (seq != testSeen_) {
        testSeen_ = seq;
        switch (testCommand_.load(std::memory_order_relaxed)) {
        case kPulseStart: tests_.startPulse(); break;
        case kSweepStart: tests_.startSweep(); break;
        case kStop: tests_.stop(); break;
        default: break;
        }
    }

    // Input: the current source (silence if none), faded around a swap.
    SourceSlot* slot = current_.load(std::memory_order_acquire);
    if (slot && slot->capture) {
        slot->stage.pull(slot->ring, slot->capture->signals(), in_.data(), frames);
    } else {
        std::fill(in_.begin(), in_.begin() + frames, 0.0f);
    }
    const bool swapping = pending_.load(std::memory_order_acquire) != slot;
    const float sourceStep = static_cast<float>(1.0 / (kSourceFadeSeconds * fs_));
    for (size_t i = 0; i < frames; ++i) {
        sourceGain_ = swapping ? std::max(0.0f, sourceGain_ - sourceStep) : std::min(1.0f, sourceGain_ + sourceStep);
        in_[i] *= sourceGain_;
    }
    if (swapping && sourceGain_ == 0.0f) {
        current_.store(pending_.load(), std::memory_order_release);  // the new source primes and fades in
    }
    if (inTap_) {
        inTap_->write(in_.data(), frames);
    }

    // Test signal, filters, limiter.
    tests_.process(test_.data(), frames);
    chain_.process(in_.data(), test_.data(), mono, frames);
    limiter_.process(mono, frames);

    // Output fade before the device is released.
    const bool fadingOut = fadeOutRequested_.load(std::memory_order_relaxed);
    for (size_t i = 0; i < frames; ++i) {
        outputGain_ = fadingOut ? std::max(0.0f, outputGain_ - fadeStep_) : std::min(1.0f, outputGain_ + fadeStep_);
        mono[i] *= outputGain_;
    }
    if (fadingOut && outputGain_ == 0.0f) {
        fadedOut_.store(true, std::memory_order_release);
    }
    if (outTap_) {
        outTap_->write(mono, frames);
    }

    // Keep the DAC from seeing exact silence (after the analysis tap, so the
    // meters never show it).
    for (size_t i = 0; i < frames; ++i) {
        mono[i] += keepAlive_;
        keepAlive_ = -keepAlive_;
    }

    // Telemetry.
    testKind_.store(tests_.kind() == TestSignals::Kind::Pulse ? 1 : (tests_.kind() == TestSignals::Kind::Sweep ? 2 : 0),
                    std::memory_order_relaxed);
    sweepHz_.store(tests_.sweepHz(), std::memory_order_relaxed);
    limiterHit_.store(limiter_.engaged(), std::memory_order_relaxed);
    clipped_.store(limiter_.clipped(), std::memory_order_relaxed);
    if (slot && slot->capture) {
        const InputStats& s = slot->stage.stats();
        fillMs_.store(s.fillMs, std::memory_order_relaxed);
        targetMs_.store(s.targetMs, std::memory_order_relaxed);
        driftPpm_.store(s.driftPpm, std::memory_order_relaxed);
        reserveMs_.store(s.reserveMs, std::memory_order_relaxed);
        locked_.store(s.locked, std::memory_order_relaxed);
        playing_.store(s.playing, std::memory_order_relaxed);
        dropouts_.store(s.dropouts(), std::memory_order_relaxed);
    }
}

Telemetry RenderPath::telemetry(double outputBufferMs, double capturePacketMs) const
{
    Telemetry t;
    const double fill = fillMs_.load(std::memory_order_relaxed);
    const double target = targetMs_.load(std::memory_order_relaxed);
    // End to end: one capture packet + the buffered audio + the output buffer
    // (WASAPI exclusive is double-buffered).
    t.latencyMs = capturePacketMs + fill + 2.0 * outputBufferMs;
    t.driftPpm = driftPpm_.load(std::memory_order_relaxed);
    t.fill = target > 0.0 ? std::clamp(fill / (2.0 * target), 0.0, 1.0) : 0.0;
    t.locked = locked_.load(std::memory_order_relaxed);
    t.dropouts = dropouts_.load(std::memory_order_relaxed);
    t.reserveMs = reserveMs_.load(std::memory_order_relaxed);
    t.limiterHit = limiterHit_.load(std::memory_order_relaxed);
    t.clipped = clipped_.load(std::memory_order_relaxed);
    t.sweepHz = sweepHz_.load(std::memory_order_relaxed);
    return t;
}

}  // namespace gfx

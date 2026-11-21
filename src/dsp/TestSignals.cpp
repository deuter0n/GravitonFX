// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "dsp/TestSignals.h"

#include <algorithm>
#include <cmath>

namespace gfx {

void TestSignals::prepare(double sampleRate)
{
    fs_ = sampleRate;
    fadeStep_ = 1.0 / (kFadeSeconds * sampleRate);
    kind_ = Kind::None;
    stopping_ = false;
    gain_ = 0.0;
    phase_ = 0.0;
}

void TestSignals::start(Kind kind)
{
    // Starting from silence fades in from zero. Switching from one test to
    // the other keeps the phase and the envelope, so it doesn't click.
    if (kind_ == Kind::None) {
        gain_ = 0.0;
    }
    kind_ = kind;
    stopping_ = false;
    elapsed_ = 0.0;
}

void TestSignals::process(float* out, size_t frames)
{
    if (kind_ == Kind::None) {
        std::fill(out, out + frames, 0.0f);
        return;
    }

    const double sweepSamples = kSweepSteps * kSweepStepSeconds * fs_;
    const double stepSamples = kSweepStepSeconds * fs_;
    const double fadeSamples = kFadeSeconds * fs_;

    for (size_t i = 0; i < frames; ++i) {
        // Frequency and automatic end for this sample.
        if (kind_ == Kind::Pulse) {
            frequency_ = kPulseHz;
            if (elapsed_ >= kPulseLimitSeconds * fs_) {
                stopping_ = true;
            }
        } else if (kind_ == Kind::Sweep) {
            const int step = std::min(kSweepSteps - 1, static_cast<int>(elapsed_ / stepSamples));
            frequency_ = kSweepStartHz + kSweepStepHz * step;
            if (elapsed_ >= sweepSamples - fadeSamples) {
                stopping_ = true;  // fade out within the last step
            }
        }

        // Fade envelope.
        gain_ = stopping_ ? std::max(0.0, gain_ - fadeStep_) : std::min(1.0, gain_ + fadeStep_);

        const double envelope = 0.5 - 0.5 * std::cos(M_PI * gain_);  // S-curve: smooth at both ends
        out[i] = kind_ == Kind::None ? 0.0f : static_cast<float>(kLevel * envelope * std::sin(phase_));

        phase_ += 2.0 * M_PI * frequency_ / fs_;
        if (phase_ >= 2.0 * M_PI) {
            phase_ -= 2.0 * M_PI;
        }
        elapsed_ += 1.0;

        if (stopping_ && gain_ < 1e-9) {  // faded out (allowing for rounding)
            kind_ = Kind::None;
            stopping_ = false;
            frequency_ = 0.0;
            gain_ = 0.0;
            // The rest of this block is silence. (Running on through the loop
            // used to raise the fade again, so the next test started with a jump.)
            std::fill(out + i + 1, out + frames, 0.0f);
            return;
        }
    }
}

}  // namespace gfx

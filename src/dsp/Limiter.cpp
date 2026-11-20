// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "dsp/Limiter.h"

#include <algorithm>
#include <cmath>

namespace gfx {

void Limiter::prepare(double sampleRate)
{
    constexpr double kReleaseSeconds = 0.050;
    release_ = static_cast<float>(1.0 - std::exp(-1.0 / (kReleaseSeconds * sampleRate)));
    gain_ = 1.0f;
}

void Limiter::process(float* samples, size_t frames)
{
    engaged_ = false;
    clipped_ = false;
    for (size_t i = 0; i < frames; ++i) {
        float x = samples[i];
        const float level = std::abs(x);

        // Gain needed to keep this sample under the ceiling (1 when off).
        const float needed = (enabled_ && level > kCeiling) ? kCeiling / level : 1.0f;
        if (needed < gain_) {
            gain_ = needed;  // instant attack
        } else {
            gain_ += (1.0f - gain_) * release_;  // smooth recovery
        }
        x *= gain_;

        if (enabled_) {
            if (gain_ < 0.999f) {
                engaged_ = true;
            }
            x = std::clamp(x, -kCeiling, kCeiling);  // guarantees the ceiling
        } else if (std::abs(x) > 1.0f) {
            clipped_ = true;
        }
        samples[i] = x;
    }
}

}  // namespace gfx

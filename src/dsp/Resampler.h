// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include <cstddef>

namespace gfx {

// Variable-ratio resampler with linear interpolation.
//
// `ratio` is input samples per output sample: captureRate / outputRate,
// times (1 + drift trim). Linear interpolation is enough here because the
// signal is low-passed to 200 Hz or less afterwards, which removes the
// interpolation images.
//
// Usage per output block:
//   size_t need = r.inputNeeded(frames, ratio);   // peek this many from the ring
//   size_t used = r.process(in, out, frames, ratio);
//   ring.skip(used);                              // remove only what was consumed
class Resampler {
public:
    void reset() { phase_ = 0.0; last_ = 0.0f; }

    // Input samples that `process` may read or consume for `frames` outputs.
    // Covers the whole advance (which can exceed the last sample read when
    // ratio > 1) plus one sample of margin for floating-point rounding.
    size_t inputNeeded(size_t frames, double ratio) const
    {
        return static_cast<size_t>(phase_ + ratio * static_cast<double>(frames)) + 2;
    }

    // Writes `frames` outputs from `in` (which must hold inputNeeded(...)
    // samples) and returns how many input samples were consumed.
    size_t process(const float* in, float* out, size_t frames, double ratio)
    {
        // Output k lies `phase_` of the way from the previous input sample
        // (in[i - 1], or last_ for i = 0) to in[i].
        size_t i = 0;
        for (size_t k = 0; k < frames; ++k) {
            const float prev = i == 0 ? last_ : in[i - 1];
            out[k] = prev + static_cast<float>(phase_) * (in[i] - prev);
            phase_ += ratio;
            while (phase_ >= 1.0) {
                phase_ -= 1.0;
                ++i;
            }
        }
        if (i > 0) {
            last_ = in[i - 1];
        }
        return i;
    }

private:
    double phase_ = 0.0;
    float last_ = 0.0f;
};

}  // namespace gfx

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include "dsp/Butterworth.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace gfx {

// User-facing filter settings.
struct FilterParams {
    double cutoffHz = 80.0;    // low-pass cutoff, 20-200 Hz
    int slope = 24;            // low-pass slope: 12, 24, 36 or 48 dB/oct
    double subsonicHz = 20.0;  // subsonic high-pass: 0 (off), 10, 15, 20 or 30 Hz
    int subsonicSlope = 12;    // subsonic slope: 12 (gentle) or 24 (steep) dB/oct
    double gainDb = 0.0;       // -12 to +12 dB
};

// Fades between a fixed set of signal paths. Changing the target while a fade
// runs is deferred until it finishes, so a fade always goes from one path to
// another and the weights always add up to 1.
class PathFade {
public:
    void snap(int path) { from_ = to_ = pending_ = path; t_ = 1.0; }
    void setTarget(int path) { pending_ = path; }
    void setLength(double samples) { step_ = 1.0 / samples; }

    // Advances one sample and returns the weight of `path`.
    void tick()
    {
        if (t_ >= 1.0 && pending_ != to_) {
            from_ = to_;
            to_ = pending_;
            t_ = 0.0;
        }
        if (t_ < 1.0) {
            t_ = std::min(1.0, t_ + step_);
        }
    }
    double weight(int path) const
    {
        return (path == to_ ? t_ : 0.0) + (path == from_ ? 1.0 - t_ : 0.0);
    }

private:
    int from_ = 0, to_ = 0, pending_ = 0;
    double t_ = 1.0;
    double step_ = 0.001;
};

// The shaker signal path after resampling:
//   low-pass -> (+ test signal) -> subsonic high-pass -> gain
//
// Parameter changes never click: cutoff and subsonic frequency glide in the
// log domain (time constant 10 ms, recomputed once per block), gain glides per
// sample, and slope changes crossfade over 20 ms between filters that are
// always running. Real-time safe: no allocation after construction.
class FilterChain {
public:
    FilterChain();

    // Sets the sample rate and jumps straight to `params` (no glide).
    void prepare(double sampleRate, const FilterParams& params);

    // New target settings; takes effect smoothly over the next blocks.
    void setTarget(const FilterParams& params) { target_ = params; }

    // `test` may be null; it is added after the low-pass.
    void process(const float* in, const float* test, float* out, size_t frames);

    // Current (smoothed) values, for tests.
    double cutoffHz() const { return cutoff_; }
    double gainLinear() const { return gain_; }

private:
    void updateCoefficients();

    static int lowPassPath(int slope);
    static int subsonicPath(const FilterParams& p);

    double fs_ = 48000.0;
    FilterParams target_;
    double cutoff_ = 80.0;
    double subsonic_ = 20.0;
    double gain_ = 1.0;
    double glideCoeff_ = 0.0;  // per-sample one-pole coefficient for gain

    // Low-pass orders 2, 4, 6, 8 (paths 0-3).
    std::array<Butterworth, 4> lowPass_;
    PathFade lowPassFade_;

    // Subsonic paths: 0 = off (bypass), 1 = gentle (order 2), 2 = steep (order 4).
    Butterworth subsonicGentle_;
    Butterworth subsonicSteep_;
    PathFade subsonicFade_;
};

}  // namespace gfx

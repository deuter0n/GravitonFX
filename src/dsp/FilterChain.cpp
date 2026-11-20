// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "dsp/FilterChain.h"

#include <algorithm>
#include <cmath>

namespace gfx {

namespace {

constexpr double kGlideSeconds = 0.010;  // cutoff, subsonic and gain glide
constexpr double kFadeSeconds = 0.020;   // slope crossfades

double dbToLinear(double db) { return std::pow(10.0, db / 20.0); }

// Moves `current` toward `target` in the log domain; snaps when close.
double glideLog(double current, double target, double amount)
{
    const double next = current * std::pow(target / current, amount);
    return std::abs(next / target - 1.0) < 1e-4 ? target : next;
}

}  // namespace

FilterChain::FilterChain()
    : lowPass_{Butterworth(FilterType::LowPass, 2), Butterworth(FilterType::LowPass, 4),
               Butterworth(FilterType::LowPass, 6), Butterworth(FilterType::LowPass, 8)},
      subsonicGentle_(FilterType::HighPass, 2),
      subsonicSteep_(FilterType::HighPass, 4)
{
}

int FilterChain::lowPassPath(int slope) { return std::clamp(slope / 12 - 1, 0, 3); }

int FilterChain::subsonicPath(const FilterParams& p)
{
    if (p.subsonicHz <= 0.0) {
        return 0;
    }
    return p.subsonicSlope >= 24 ? 2 : 1;
}

void FilterChain::prepare(double sampleRate, const FilterParams& params)
{
    fs_ = sampleRate;
    target_ = params;
    cutoff_ = params.cutoffHz;
    if (params.subsonicHz > 0.0) {
        subsonic_ = params.subsonicHz;
    }
    gain_ = dbToLinear(params.gainDb);
    glideCoeff_ = 1.0 - std::exp(-1.0 / (kGlideSeconds * fs_));

    for (auto& f : lowPass_) {
        f.reset();
    }
    subsonicGentle_.reset();
    subsonicSteep_.reset();
    updateCoefficients();

    lowPassFade_.setLength(kFadeSeconds * fs_);
    subsonicFade_.setLength(kFadeSeconds * fs_);
    lowPassFade_.snap(lowPassPath(params.slope));
    subsonicFade_.snap(subsonicPath(params));
}

void FilterChain::updateCoefficients()
{
    for (auto& f : lowPass_) {
        f.design(cutoff_, fs_);
    }
    subsonicGentle_.design(subsonic_, fs_);
    subsonicSteep_.design(subsonic_, fs_);
}

void FilterChain::process(const float* in, const float* test, float* out, size_t frames)
{
    // Once per block: glide the filter frequencies and pick the fade targets.
    // While the subsonic filter is off its frequency stays where it was, so
    // turning it back on doesn't sweep.
    const double amount = 1.0 - std::exp(-static_cast<double>(frames) / (kGlideSeconds * fs_));
    const double newCutoff = glideLog(cutoff_, target_.cutoffHz, amount);
    const double newSubsonic = target_.subsonicHz > 0.0 ? glideLog(subsonic_, target_.subsonicHz, amount) : subsonic_;
    if (newCutoff != cutoff_ || newSubsonic != subsonic_) {
        cutoff_ = newCutoff;
        subsonic_ = newSubsonic;
        updateCoefficients();
    }
    lowPassFade_.setTarget(lowPassPath(target_.slope));
    subsonicFade_.setTarget(subsonicPath(target_));
    const double targetGain = dbToLinear(target_.gainDb);

    for (size_t i = 0; i < frames; ++i) {
        // Low-pass: all orders run, the fade picks (or blends) the active one.
        lowPassFade_.tick();
        double x = 0.0;
        for (int p = 0; p < 4; ++p) {
            const double y = lowPass_[p].process(in[i]);
            x += lowPassFade_.weight(p) * y;
        }

        // Test signals join here, so they skip the low-pass but still pass
        // the subsonic filter, gain and limiter.
        if (test) {
            x += test[i];
        }

        // Subsonic high-pass.
        subsonicFade_.tick();
        const double gentle = subsonicGentle_.process(x);
        const double steep = subsonicSteep_.process(x);
        x = subsonicFade_.weight(0) * x + subsonicFade_.weight(1) * gentle + subsonicFade_.weight(2) * steep;

        // Gain.
        gain_ += (targetGain - gain_) * glideCoeff_;
        out[i] = static_cast<float>(x * gain_);
    }
}

}  // namespace gfx

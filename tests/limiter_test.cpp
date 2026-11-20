// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "dsp/Limiter.h"

#include <doctest/doctest.h>

#include <cmath>
#include <vector>

using gfx::Limiter;

namespace {

constexpr double kFs = 48000.0;

std::vector<float> tone(double hz, double amplitude, double seconds)
{
    std::vector<float> v(static_cast<size_t>(seconds * kFs));
    for (size_t i = 0; i < v.size(); ++i) {
        v[i] = static_cast<float>(amplitude * std::sin(2 * M_PI * hz * i / kFs));
    }
    return v;
}

float peak(const std::vector<float>& v)
{
    float p = 0.0f;
    for (float x : v) {
        p = std::max(p, std::abs(x));
    }
    return p;
}

}  // namespace

TEST_CASE("Limiter on: a +3 dBFS peak never exceeds -1 dBFS")
{
    Limiter limiter;
    limiter.prepare(kFs);
    auto signal = tone(40.0, std::pow(10.0, 3.0 / 20.0), 1.0);
    bool engaged = false;
    for (size_t i = 0; i < signal.size(); i += 64) {
        limiter.process(signal.data() + i, std::min<size_t>(64, signal.size() - i));
        engaged = engaged || limiter.engaged();
        CHECK_FALSE(limiter.clipped());
    }
    CHECK(engaged);
    CHECK(peak(signal) <= Limiter::kCeiling);
}

TEST_CASE("Limiter on: quiet signals pass untouched and aren't flagged")
{
    Limiter limiter;
    limiter.prepare(kFs);
    const auto original = tone(40.0, 0.5, 0.5);
    auto signal = original;
    limiter.process(signal.data(), signal.size());
    CHECK_FALSE(limiter.engaged());
    CHECK_FALSE(limiter.clipped());
    CHECK(signal == original);
}

TEST_CASE("Limiter off: loud signals pass and are reported as clipped")
{
    Limiter limiter;
    limiter.prepare(kFs);
    limiter.setEnabled(false);
    const auto original = tone(40.0, 1.5, 0.5);
    auto signal = original;
    limiter.process(signal.data(), signal.size());
    CHECK(limiter.clipped());
    CHECK_FALSE(limiter.engaged());
    CHECK(signal == original);
}

TEST_CASE("Limiter recovers to unity gain after a peak")
{
    Limiter limiter;
    limiter.prepare(kFs);
    auto loud = tone(40.0, 2.0, 0.2);
    limiter.process(loud.data(), loud.size());
    auto quiet = tone(40.0, 0.5, 1.0);  // 20 release time constants
    limiter.process(quiet.data(), quiet.size());
    std::vector<float> lastBlock(quiet.end() - 4800, quiet.end());
    CHECK(peak(lastBlock) == doctest::Approx(0.5).epsilon(0.002));

    // The flags describe one block: a later quiet block isn't flagged.
    auto later = tone(40.0, 0.5, 0.1);
    limiter.process(later.data(), later.size());
    CHECK_FALSE(limiter.engaged());
}

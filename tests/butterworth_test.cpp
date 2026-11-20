// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "dsp/Butterworth.h"

#include <doctest/doctest.h>

#include <cmath>

using namespace gfx;

namespace {

constexpr double kFs = 48000.0;

// Response of a whole design in dB at `hz`.
double responseDb(FilterType type, int slope, double cutoff, double hz, double fs = kFs)
{
    std::array<BiquadCoeffs, kMaxSections> c;
    const int sections = designButterworth(type, orderForSlope(slope), cutoff, fs, c);
    double mag = 1.0;
    for (int i = 0; i < sections; ++i) {
        mag *= c[i].magnitudeAt(hz, fs);
    }
    return 20.0 * std::log10(mag);
}

// Measures the steady-state gain of a running filter with a sine at `hz`.
double measuredDb(Butterworth& f, double hz, double fs = kFs)
{
    double peak = 0.0;
    const int total = static_cast<int>(fs * 2);
    for (int i = 0; i < total; ++i) {
        const double y = f.process(std::sin(2.0 * M_PI * hz * i / fs));
        if (i > total / 2) {
            peak = std::max(peak, std::abs(y));
        }
    }
    return 20.0 * std::log10(peak);
}

}  // namespace

TEST_CASE("Low-pass is -3 dB at the cutoff for every slope")
{
    for (int slope : {12, 24, 36, 48}) {
        for (double cutoff : {20.0, 80.0, 200.0}) {
            CAPTURE(slope);
            CAPTURE(cutoff);
            CHECK(std::abs(responseDb(FilterType::LowPass, slope, cutoff, cutoff) + 3.01) <= 0.5);
            CHECK(std::abs(responseDb(FilterType::LowPass, slope, cutoff, 1.0)) < 0.01);  // flat passband
        }
    }
}

TEST_CASE("Low-pass at 80 Hz, 24 dB/oct attenuates 320 Hz by at least 45 dB")
{
    CHECK(responseDb(FilterType::LowPass, 24, 80.0, 320.0) <= -45.0);
    CHECK(responseDb(FilterType::LowPass, 24, 80.0, 320.0, 44100.0) <= -45.0);
}

TEST_CASE("Low-pass section counts follow the slope")
{
    std::array<BiquadCoeffs, kMaxSections> c;
    CHECK(designButterworth(FilterType::LowPass, 2, 80, kFs, c) == 1);
    CHECK(designButterworth(FilterType::LowPass, 4, 80, kFs, c) == 2);
    CHECK(designButterworth(FilterType::LowPass, 6, 80, kFs, c) == 3);
    CHECK(designButterworth(FilterType::LowPass, 8, 80, kFs, c) == 4);
}

TEST_CASE("A running low-pass matches its designed response")
{
    Butterworth lp(FilterType::LowPass, 4);
    lp.design(80.0, kFs);
    CHECK(measuredDb(lp, 320.0) <= -45.0);
    lp.reset();
    CHECK(measuredDb(lp, 45.0) > -1.0);
}

TEST_CASE("Subsonic high-pass at 20 Hz protects the shaker")
{
    // Gentle: at least 11 dB down at 10 Hz. Steep: at least 23 dB.
    CHECK(responseDb(FilterType::HighPass, 12, 20.0, 10.0) <= -11.0);
    CHECK(responseDb(FilterType::HighPass, 24, 20.0, 10.0) <= -23.0);
    // Both lose less than 1 dB at 45 Hz.
    CHECK(responseDb(FilterType::HighPass, 12, 20.0, 45.0) > -1.0);
    CHECK(responseDb(FilterType::HighPass, 24, 20.0, 45.0) > -1.0);
}

TEST_CASE("Subsonic high-pass is -3 dB at each selectable frequency")
{
    for (double hz : {10.0, 15.0, 20.0, 30.0}) {
        for (int slope : {12, 24}) {
            CAPTURE(hz);
            CAPTURE(slope);
            CHECK(std::abs(responseDb(FilterType::HighPass, slope, hz, hz) + 3.01) <= 0.5);
        }
    }
}

TEST_CASE("80 Hz low-pass plus 20 Hz subsonic passes 45 Hz within 1 dB")
{
    Butterworth lp(FilterType::LowPass, 4);
    Butterworth hp(FilterType::HighPass, 2);
    lp.design(80.0, kFs);
    hp.design(20.0, kFs);
    double peak = 0.0;
    const int total = static_cast<int>(kFs * 2);
    for (int i = 0; i < total; ++i) {
        const double y = hp.process(lp.process(std::sin(2.0 * M_PI * 45.0 * i / kFs)));
        if (i > total / 2) {
            peak = std::max(peak, std::abs(y));
        }
    }
    CHECK(20.0 * std::log10(peak) > -1.0);
}

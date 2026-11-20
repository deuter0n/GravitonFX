// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "dsp/Butterworth.h"
#include "dsp/Resampler.h"
#include "dsp/SpscRing.h"

#include <doctest/doctest.h>

#include <cmath>
#include <vector>

using namespace gfx;

namespace {

// Feeds `seconds` of a tone at `inRate` through a ring and the resampler at
// `ratio`, in output blocks of 64, and returns the output.
std::vector<float> resampleTone(double hz, double inRate, double outRate, double seconds, double trim = 0.0)
{
    const double ratio = inRate / outRate * (1.0 + trim);
    SpscRing ring(1 << 16);
    Resampler rs;
    std::vector<float> out;
    std::vector<float> scratch(1024), block(64);
    size_t produced = 0;
    const size_t total = static_cast<size_t>(seconds * outRate);
    while (out.size() < total) {
        // Keep the ring topped up like a capture thread would.
        while (ring.size() < 2048) {
            const float x = static_cast<float>(std::sin(2 * M_PI * hz * static_cast<double>(produced++) / inRate));
            ring.write(&x, 1);
        }
        const size_t need = rs.inputNeeded(64, ratio);
        REQUIRE(ring.peek(scratch.data(), need) == need);
        const size_t used = rs.process(scratch.data(), block.data(), 64, ratio);
        REQUIRE(used <= need);
        ring.skip(used);
        out.insert(out.end(), block.begin(), block.end());
    }
    return out;
}

// Removes the best-fitting sine at `hz` (least squares on sin and cos) and
// returns the residual RMS relative to the tone's RMS, in dB.
double residualDb(const std::vector<float>& y, size_t from, double hz, double fs)
{
    double ss = 0, cc = 0, sc = 0, ys = 0, yc = 0;
    for (size_t i = from; i < y.size(); ++i) {
        const double s = std::sin(2 * M_PI * hz * i / fs), c = std::cos(2 * M_PI * hz * i / fs);
        ss += s * s; cc += c * c; sc += s * c; ys += y[i] * s; yc += y[i] * c;
    }
    const double det = ss * cc - sc * sc;
    const double a = (ys * cc - yc * sc) / det, b = (yc * ss - ys * sc) / det;
    double err = 0, sig = 0;
    for (size_t i = from; i < y.size(); ++i) {
        const double fit = a * std::sin(2 * M_PI * hz * i / fs) + b * std::cos(2 * M_PI * hz * i / fs);
        err += (y[i] - fit) * (y[i] - fit);
        sig += fit * fit;
    }
    return 10.0 * std::log10(err / sig);
}

}  // namespace

TEST_CASE("Resampler at ratio 1 passes samples through with one sample of delay")
{
    Resampler rs;
    const float in[6] = {1, 2, 3, 4, 5, 6};
    float out[4];
    CHECK(rs.inputNeeded(4, 1.0) >= 4);
    CHECK(rs.process(in, out, 4, 1.0) == 4);
    CHECK(out[0] == 0.0f);  // previous sample (silence at start)
    CHECK(out[1] == 1.0f);
    CHECK(out[3] == 3.0f);
}

TEST_CASE("Resampler consumes input at the requested ratio")
{
    Resampler rs;
    std::vector<float> in(4096, 0.0f), out(64);
    size_t consumed = 0;
    for (int b = 0; b < 1000; ++b) {
        REQUIRE(rs.inputNeeded(64, 48000.0 / 44100.0) <= in.size());
        consumed += rs.process(in.data(), out.data(), 64, 48000.0 / 44100.0);
    }
    CHECK(static_cast<double>(consumed) == doctest::Approx(64000.0 * 48000.0 / 44100.0).epsilon(1e-4));
}

TEST_CASE("Resampler never reads or consumes beyond inputNeeded, even at large ratios")
{
    for (double ratio : {0.23, 0.92, 1.0, 1.0884, 2.5, 4.354}) {
        CAPTURE(ratio);
        Resampler rs;
        std::vector<float> in(2048, 1.0f), out(64);
        for (int b = 0; b < 500; ++b) {
            const size_t need = rs.inputNeeded(64, ratio);
            REQUIRE(need <= in.size());
            CHECK(rs.process(in.data(), out.data(), 64, ratio) <= need);
        }
    }
}

TEST_CASE("Resampling 60 Hz from 48 to 44.1 kHz, then low-passing, leaves images below -60 dB")
{
    auto y = resampleTone(60.0, 48000.0, 44100.0, 3.0, 500e-6);
    Butterworth lp(FilterType::LowPass, 4);
    lp.design(80.0, 44100.0);
    for (float& v : y) {
        v = static_cast<float>(lp.process(v));
    }
    // The trim makes the output tone 60 * (1 + trim) Hz in output time.
    CHECK(residualDb(y, 44100, 60.0 * (1.0 + 500e-6), 44100.0) < -60.0);
}

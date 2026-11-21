// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "dsp/FilterChain.h"

#include <doctest/doctest.h>

#include <cmath>
#include <functional>
#include <vector>

using namespace gfx;

namespace {

constexpr double kFs = 48000.0;
constexpr size_t kBlock = 64;

// A bass test input: 30 Hz and 70 Hz at 0.4 each. Its own largest
// sample-to-sample step is about 0.0052, so a jump above 0.01 means the
// filter itself made a click.
float bass(size_t i)
{
    const double t = static_cast<double>(i) / kFs;
    return static_cast<float>(0.4 * std::sin(2 * M_PI * 30 * t) + 0.4 * std::sin(2 * M_PI * 70 * t));
}
constexpr double kClickThreshold = 0.01;

struct RunResult {
    double maxStep = 0.0;  // largest |y[n] - y[n-1]| after the first 0.5 s
    double peakLast = 0.0; // peak over the last 0.25 s
};

// Runs `seconds` of input through `chain` in blocks; `beforeBlock(t)` may
// change the target at time t.
RunResult run(FilterChain& chain, double seconds, const std::function<float(size_t)>& input,
              const std::function<void(double)>& beforeBlock = {}, const std::function<float(size_t)>& test = {})
{
    RunResult r;
    const size_t total = static_cast<size_t>(seconds * kFs);
    std::vector<float> in(kBlock), tst(kBlock), out(kBlock);
    float prev = 0.0f;
    for (size_t start = 0; start < total; start += kBlock) {
        if (beforeBlock) {
            beforeBlock(start / kFs);
        }
        for (size_t i = 0; i < kBlock; ++i) {
            in[i] = input(start + i);
            tst[i] = test ? test(start + i) : 0.0f;
        }
        chain.process(in.data(), test ? tst.data() : nullptr, out.data(), kBlock);
        for (size_t i = 0; i < kBlock; ++i) {
            const size_t n = start + i;
            if (n > kFs / 2) {
                r.maxStep = std::max(r.maxStep, static_cast<double>(std::abs(out[i] - prev)));
            }
            if (n > total - kFs / 4) {
                r.peakLast = std::max(r.peakLast, static_cast<double>(std::abs(out[i])));
            }
            prev = out[i];
        }
    }
    return r;
}

double toDb(double x) { return 20.0 * std::log10(x); }

float sine(size_t i, double hz, double amp = 0.5)
{
    return static_cast<float>(amp * std::sin(2 * M_PI * hz * static_cast<double>(i) / kFs));
}

}  // namespace

TEST_CASE("FilterChain passes bass and removes content above the cutoff")
{
    FilterChain chain;
    chain.prepare(kFs, FilterParams{});  // 80 Hz, 24 dB/oct, subsonic 20 Hz gentle, 0 dB

    CHECK(toDb(run(chain, 2.0, [](size_t i) { return sine(i, 45.0, 1.0); }).peakLast) > -1.0);
    chain.prepare(kFs, FilterParams{});
    CHECK(toDb(run(chain, 2.0, [](size_t i) { return sine(i, 320.0, 1.0); }).peakLast) <= -45.0);
}

TEST_CASE("FilterChain gain: +6 dB doubles the level")
{
    FilterParams p;
    FilterChain a, b;
    a.prepare(kFs, p);
    p.gainDb = 6.0;
    b.prepare(kFs, p);
    const auto tone = [](size_t i) { return sine(i, 50.0); };
    const double diff = toDb(run(b, 1.0, tone).peakLast) - toDb(run(a, 1.0, tone).peakLast);
    CHECK(std::abs(diff - 6.0) < 0.05);
}

TEST_CASE("FilterChain: dragging the cutoff 20 -> 200 Hz in half a second doesn't click")
{
    FilterParams p;
    p.cutoffHz = 20.0;
    FilterChain chain;
    chain.prepare(kFs, p);
    const auto r = run(chain, 2.0, bass, [&](double t) {
        // Hold for 0.5 s, sweep up over 0.5 s, then back down instantly.
        if (t >= 0.5 && t < 1.0) {
            p.cutoffHz = 20.0 + 180.0 * (t - 0.5) / 0.5;
        } else if (t >= 1.5) {
            p.cutoffHz = 20.0;
        }
        chain.setTarget(p);
    });
    CHECK(r.maxStep < kClickThreshold);
}

TEST_CASE("FilterChain: switching slope 12 -> 48 dB/oct doesn't click and settles within 50 ms")
{
    FilterParams p;
    p.slope = 12;
    FilterChain chain;
    chain.prepare(kFs, p);
    const auto r = run(chain, 1.0, bass, [&](double t) {
        p.slope = t >= 0.6 ? 48 : 12;
        chain.setTarget(p);
    });
    CHECK(r.maxStep < kClickThreshold);

    // After 50 ms the output equals a chain that was always at 48 dB/oct.
    FilterParams p48;
    p48.slope = 48;
    FilterChain reference;
    reference.prepare(kFs, p48);
    const auto ref = run(reference, 1.0, bass);
    CHECK(std::abs(r.peakLast - ref.peakLast) < 1e-3);
}

TEST_CASE("FilterChain: subsonic on/off, slope and frequency changes don't click")
{
    FilterParams p;
    p.subsonicHz = 0.0;
    FilterChain chain;
    chain.prepare(kFs, p);
    const auto r = run(chain, 2.0, bass, [&](double t) {
        p.subsonicHz = t < 0.6 ? 0.0 : (t < 1.2 ? 20.0 : 30.0);
        p.subsonicSlope = t < 0.9 ? 12 : 24;
        chain.setTarget(p);
    });
    CHECK(r.maxStep < kClickThreshold);
}

TEST_CASE("FilterChain: gain jumps are ramped")
{
    FilterParams p;
    FilterChain chain;
    chain.prepare(kFs, p);
    const auto r = run(chain, 1.0, bass, [&](double t) {
        p.gainDb = t < 0.6 ? -12.0 : 12.0;
        chain.setTarget(p);
    });
    // +12 dB scales the signal's own steps by 4; the jump itself adds little.
    CHECK(r.maxStep < 4 * kClickThreshold);
    CHECK(std::abs(chain.gainLinear() - std::pow(10.0, 12.0 / 20.0)) < 1e-3);
}

TEST_CASE("FilterChain: test signals skip the low-pass but not the subsonic filter")
{
    FilterParams p;
    p.cutoffHz = 40.0;
    FilterChain chain;
    chain.prepare(kFs, p);
    const auto silence = [](size_t) { return 0.0f; };

    // A 150 Hz test tone passes even with a 40 Hz cutoff...
    const auto high = run(chain, 1.0, silence, {}, [](size_t i) { return sine(i, 150.0); });
    CHECK(toDb(high.peakLast / 0.5) > -0.5);

    // ...but a 10 Hz test tone is cut by the 20 Hz subsonic filter.
    chain.prepare(kFs, p);
    const auto low = run(chain, 2.0, silence, {}, [](size_t i) { return sine(i, 10.0); });
    CHECK(toDb(low.peakLast / 0.5) <= -11.0);
}

// The Settings > Engine options really change the sound: each subsonic
// frequency and slope cuts what a Butterworth high-pass of that order should.
// (Measured on the test-signal input, which skips the low-pass.)
TEST_CASE("Subsonic filter: every frequency and slope cuts as specified; Off passes")
{
    const auto silence = [](size_t) { return 0.0f; };
    const auto levelDb = [&](FilterParams p, double hz) {
        FilterChain chain;
        chain.prepare(kFs, p);
        const auto r = run(chain, 3.0, silence, {}, [hz](size_t i) { return sine(i, hz); });
        return toDb(r.peakLast / 0.5);
    };

    FilterParams off;
    off.subsonicHz = 0.0;
    CHECK(std::abs(levelDb(off, 8.0)) < 0.5);  // Off: even 8 Hz passes

    for (const double fc : {10.0, 15.0, 20.0, 30.0}) {
        for (const int slope : {12, 24}) {
            CAPTURE(fc);
            CAPTURE(slope);
            FilterParams p;
            p.subsonicHz = fc;
            p.subsonicSlope = slope;
            // -3 dB at the chosen frequency for both slopes.
            CHECK(std::abs(levelDb(p, fc) + 3.0) < 1.0);
            // An octave below: Gentle (2nd order) about -12 dB, Steep (4th order) about -24 dB.
            const double expected = slope == 12 ? -12.3 : -24.1;
            CHECK(std::abs(levelDb(p, fc / 2) - expected) < 1.5);
            // Two octaves above: practically untouched.
            CHECK(levelDb(p, fc * 4) > -0.5);
        }
    }
}

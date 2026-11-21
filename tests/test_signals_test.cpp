// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "dsp/TestSignals.h"

#include <doctest/doctest.h>

#include <cmath>
#include <vector>

using gfx::TestSignals;

namespace {

constexpr double kFs = 48000.0;

std::vector<float> render(TestSignals& t, double seconds)
{
    std::vector<float> out(static_cast<size_t>(seconds * kFs));
    for (size_t i = 0; i < out.size(); i += 64) {
        t.process(out.data() + i, std::min<size_t>(64, out.size() - i));
    }
    return out;
}

float peak(const float* begin, const float* end)
{
    float p = 0.0f;
    for (const float* x = begin; x < end; ++x) {
        p = std::max(p, std::abs(*x));
    }
    return p;
}

int zeroCrossings(const float* begin, const float* end)
{
    int count = 0;
    for (const float* x = begin + 1; x < end; ++x) {
        count += (x[-1] < 0.0f) != (x[0] < 0.0f);
    }
    return count;
}

double maxStep(const std::vector<float>& v)
{
    double m = 0.0;
    for (size_t i = 1; i < v.size(); ++i) {
        m = std::max(m, static_cast<double>(std::abs(v[i] - v[i - 1])));
    }
    return m;
}

// Steepest step a full-level 200 Hz sine can make.
const double kSmoothLimit = 2 * M_PI * 200.0 / kFs * TestSignals::kLevel * 1.05;

}  // namespace

TEST_CASE("Nothing plays until a test starts")
{
    TestSignals t;
    t.prepare(kFs);
    const auto out = render(t, 0.1);
    CHECK(peak(out.data(), out.data() + out.size()) == 0.0f);
    CHECK_FALSE(t.active());
}

TEST_CASE("Pulse: 40 Hz, fades in over 30 ms, plays until stopped, fades out over 30 ms")
{
    TestSignals t;
    t.prepare(kFs);
    t.startPulse();
    const auto held = render(t, 2.0);
    CHECK(t.kind() == TestSignals::Kind::Pulse);

    // Fade in: quiet in the first 2 ms, full level after 30 ms.
    CHECK(peak(held.data(), held.data() + 96) < 0.1f);
    CHECK(peak(held.data() + 1440, held.data() + 4800) == doctest::Approx(TestSignals::kLevel).epsilon(0.01));
    // 40 Hz: 80 zero crossings per second.
    CHECK(std::abs(zeroCrossings(held.data() + 48000, held.data() + 96000) - 80) <= 1);

    t.stop();
    const auto released = render(t, 0.1);
    CHECK(peak(released.data() + 1440, released.data() + released.size()) == 0.0f);  // silent after 30 ms
    CHECK_FALSE(t.active());
    CHECK(maxStep(held) < kSmoothLimit);
    CHECK(maxStep(released) < kSmoothLimit);
}

// The largest second difference: a corner in the envelope (where a straight-
// line fade starts or ends) shows up here as a spike that a sample-to-sample
// step check misses. A steady 40 Hz tone at this level stays near 1.5e-5.
static float maxCurvature(const std::vector<float>& s)
{
    float m = 0.0f;
    for (size_t i = 2; i < s.size(); ++i) {
        m = std::max(m, std::abs(s[i] - 2.0f * s[i - 1] + s[i - 2]));
    }
    return m;
}

TEST_CASE("Pulse fades have no corners (no click on earphones)")
{
    TestSignals t;
    t.prepare(kFs);
    t.startPulse();
    std::vector<float> all = render(t, 0.5);
    t.stop();
    const auto tail = render(t, 0.1);
    all.insert(all.end(), tail.begin(), tail.end());
    CHECK(maxCurvature(all) < 5e-5f);
}

TEST_CASE("Pulse stops by itself after 10 seconds")
{
    TestSignals t;
    t.prepare(kFs);
    t.startPulse();
    render(t, 9.9);
    CHECK(t.active());
    render(t, 0.2);
    CHECK_FALSE(t.active());
}

TEST_CASE("Sweep: 37 steps of 0.5 s from 20 to 200 Hz, 18.5 s in total")
{
    TestSignals t;
    t.prepare(kFs);
    t.startSweep();

    std::vector<double> reported;
    std::vector<float> all;
    for (int step = 0; step < 37; ++step) {
        const auto part = render(t, 0.5);
        all.insert(all.end(), part.begin(), part.end());
        // The output frequency during this step, from zero crossings in its middle 0.4 s.
        const double hz = zeroCrossings(part.data() + 2400, part.data() + 21600) / 2.0 / 0.4;
        CHECK(std::abs(hz - (20.0 + 5.0 * step)) <= 2.5);
        reported.push_back(t.sweepHz());
    }
    // Reported frequency at the end of each step (the last step has faded out).
    for (int step = 0; step < 36; ++step) {
        CHECK(reported[step] == 20.0 + 5.0 * step);
    }
    CHECK(reported[36] == 0.0);
    CHECK_FALSE(t.active());

    // Phase-continuous between steps and faded at both ends: no clicks.
    CHECK(maxStep(all) < kSmoothLimit);
    CHECK(std::abs(all.front()) < 0.01f);
    CHECK(std::abs(all.back()) < 0.01f);
}

TEST_CASE("Sweep can be stopped early with a fade")
{
    TestSignals t;
    t.prepare(kFs);
    t.startSweep();
    render(t, 3.0);
    t.stop();
    const auto tail = render(t, 0.05);
    CHECK_FALSE(t.active());
    CHECK(t.sweepHz() == 0.0);
    CHECK(maxStep(tail) < kSmoothLimit);
}

TEST_CASE("Starting one test while the other plays switches without a click")
{
    TestSignals t;
    t.prepare(kFs);
    t.startPulse();
    auto a = render(t, 1.0);
    t.startSweep();
    auto b = render(t, 1.0);
    CHECK(t.kind() == TestSignals::Kind::Sweep);
    a.insert(a.end(), b.begin(), b.end());
    CHECK(maxStep(a) < kSmoothLimit);
}

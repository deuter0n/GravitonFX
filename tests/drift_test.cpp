// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "dsp/DriftController.h"
#include "dsp/InputStage.h"
#include "dsp/Reserve.h"

#include <doctest/doctest.h>

#include <cmath>
#include <functional>
#include <random>
#include <vector>

using namespace gfx;

// ---------------------------------------------------------------------------
// Reserve

TEST_CASE("Reserve starts at 3 ms, grows 1 ms per step up to 6 ms, and resets")
{
    Reserve r;
    CHECK(r.ms() == 3.0);
    r.grow();
    CHECK(r.ms() == 4.0);
    r.grow();
    r.grow();
    r.grow();
    CHECK(r.ms() == 6.0);
    r.reset();
    CHECK(r.ms() == 3.0);
}

// ---------------------------------------------------------------------------
// DriftController over 4 hours, with only the fill level simulated (no
// samples), so the test stays fast.

TEST_CASE("Drift control: clocks 50 ppm apart for 4 hours keep latency within +-1 ms")
{
    constexpr double kOutRate = 48000.0;
    constexpr double kCaptureSpeed = 1.0 + 50e-6;  // capture clock runs fast
    constexpr double kPacket = 480.0;
    constexpr double kBlock = 64.0;
    constexpr double kTarget = 0.003 + (kPacket - kBlock) / 2.0 / kOutRate;  // see InputStage
    constexpr double kHours = 4.0;

    std::mt19937 rng(1);
    std::uniform_real_distribution<double> jitter(-0.001, 0.001);  // packet arrival jitter

    DriftController drift;
    drift.resume(kTarget);
    double fill = 0.003 * kOutRate;  // the reserve; the first packet arrives at t = 0
    double t = 0.0;
    double packetClock = 0.0;  // nominal arrival time on the capture clock
    double nextPacket = 0.0;
    double lockTime = -1.0;
    double settled = 0.0, lo = 1e9, hi = -1e9;
    double windowSum = 0.0;
    int windowCount = 0;
    double minFill = 1e9;
    double trimSum = 0.0;
    long trimCount = 0;

    const double blockSeconds = kBlock / kOutRate;
    while (t < kHours * 3600.0) {
        t += blockSeconds;
        while (nextPacket <= t) {  // packets arrive on the capture clock, +-1 ms jitter
            fill += kPacket;
            packetClock += kPacket / (kOutRate * kCaptureSpeed);
            nextPacket = std::max(nextPacket, packetClock + jitter(rng));
        }
        fill -= kBlock * (1.0 + drift.trim());  // output consumes at the trimmed ratio
        minFill = std::min(minFill, fill);
        if (t > (kHours - 1.0) * 3600.0) {
            trimSum += drift.trim();
            ++trimCount;
        }
        drift.update(fill / kOutRate, kTarget, blockSeconds);

        if (lockTime < 0.0 && drift.locked()) {
            lockTime = t;
        }
        // Latency = average fill over each second.
        windowSum += fill / kOutRate;
        if (++windowCount == 750) {
            const double avg = windowSum / windowCount;
            if (t > 30.0 && settled == 0.0) {
                settled = avg;
            }
            if (settled != 0.0) {
                lo = std::min(lo, avg);
                hi = std::max(hi, avg);
            }
            windowSum = 0.0;
            windowCount = 0;
        }
    }
    CHECK(lockTime >= 0.0);
    CHECK(lockTime < 10.0);
    CHECK((hi - settled) * 1000.0 < 1.0);
    CHECK((settled - lo) * 1000.0 < 1.0);
    // Over the last hour the trim averages out to the clock difference.
    CHECK(trimSum / trimCount * 1e6 == doctest::Approx(50.0).epsilon(0.02));
    CHECK(minFill > 0.0);  // never ran dry, despite +-1 ms packet jitter
}

TEST_CASE("Drift control clamps the trim to +-1000 ppm")
{
    DriftController drift;
    for (int i = 0; i < 10000; ++i) {
        drift.update(1.0, 0.0, 0.001);  // absurdly overfull
    }
    CHECK(drift.trim() == doctest::Approx(DriftController::kMaxTrim));
}

// ---------------------------------------------------------------------------
// InputStage with real samples: capture packets go into a ring on a
// (slightly fast) capture clock, the stage pulls 64-sample blocks.

namespace {

struct Sim {
    static constexpr double kOutRate = 48000.0;
    static constexpr double kCaptureRate = 48000.0;  // nominal
    static constexpr size_t kPacket = 480;
    static constexpr size_t kBlock = 64;
    static constexpr double kIdleAfter = 0.030;      // capture reports silence after 30 ms

    double captureSpeed = 1.0 + 50e-6;
    SpscRing ring{static_cast<size_t>(0.2 * kCaptureRate)};
    InputStage stage;
    CaptureSignals signals;

    double t = 0.0;
    double nextPacket = 0.0;
    double lastPacket = 0.0;
    uint64_t captured = 0;
    bool sourcePlaying = true;   // false = silence (loopback sends no packets)
    double packetDelay = 0.0;    // one-off delay for the next packet (a capture hiccup)
    bool outputStalled = false;  // output thread not running

    std::vector<float> out;      // everything the stage produced
    std::vector<double> fillLog; // ring fill (ms) after each pull
    std::vector<double> ppmLog;  // drift trim (ppm) after each pull

    Sim() { stage.prepare(kCaptureRate, kOutRate, kPacket, kBlock); }

    static float signal(uint64_t n) { return static_cast<float>(0.5 * std::sin(2 * M_PI * 50.0 * n / kCaptureRate)); }

    void run(double seconds)
    {
        const double end = t + seconds;
        std::vector<float> packet(kPacket), block(kBlock);
        while (t < end) {
            t += kBlock / kOutRate;
            while (nextPacket + packetDelay <= t) {
                if (sourcePlaying) {
                    for (size_t i = 0; i < kPacket; ++i) {
                        packet[i] = signal(captured++);
                    }
                    if (ring.write(packet.data(), kPacket) < kPacket) {
                        signals.overruns += 1;
                    }
                    lastPacket = t;
                    signals.idle = false;
                }
                nextPacket += packetDelay + kPacket / (kCaptureRate * captureSpeed);
                packetDelay = 0.0;
            }
            if (!sourcePlaying && t - lastPacket > kIdleAfter) {
                signals.idle = true;
            }
            if (outputStalled) {
                continue;
            }
            stage.pull(ring, signals, block.data(), kBlock);
            out.insert(out.end(), block.begin(), block.end());
            fillLog.push_back(ring.size() / kCaptureRate * 1000.0);
            ppmLog.push_back(stage.stats().driftPpm);
        }
    }

    static double recentAverage(const std::vector<double>& log, double seconds)
    {
        const size_t n = static_cast<size_t>(seconds * kOutRate / kBlock);
        double sum = 0.0;
        for (size_t i = log.size() - n; i < log.size(); ++i) {
            sum += log[i];
        }
        return sum / static_cast<double>(n);
    }

    // Average ring fill (ms) and drift trim (ppm) over the last `seconds`.
    double recentFillMs(double seconds) const { return recentAverage(fillLog, seconds); }
    double recentPpm(double seconds) const { return recentAverage(ppmLog, seconds); }

    // Largest sample-to-sample step in the output from sample `from` on.
    double maxStep(size_t from = 0) const
    {
        double m = 0.0;
        for (size_t i = std::max<size_t>(from, 1); i < out.size(); ++i) {
            m = std::max(m, static_cast<double>(std::abs(out[i] - out[i - 1])));
        }
        return m;
    }

    // Number of times the output fell silent for more than 1 ms.
    int gaps(size_t from = 0) const
    {
        int count = 0, run = 0;
        for (size_t i = from; i < out.size(); ++i) {
            run = out[i] == 0.0f ? run + 1 : 0;
            if (run == 48) {
                ++count;
            }
        }
        return count;
    }
};

// The 50 Hz test tone's own largest step is about 0.0033.
constexpr double kClickThreshold = 0.01;

}  // namespace

TEST_CASE("InputStage starts, locks within 10 s and streams without glitches")
{
    Sim sim;
    sim.run(10.0);
    const auto& s = sim.stage.stats();
    CHECK(s.playing);
    CHECK(s.locked);
    CHECK(s.dropouts() == 0);
    CHECK(sim.gaps(4800) == 0);
    CHECK(sim.maxStep() < kClickThreshold);
    CHECK(std::abs(sim.recentFillMs(2.0) - s.targetMs) < 0.5);

    // Averaged over a minute, the trim (shown to the user as "clock drift")
    // follows the 50 ppm clock difference. Measuring the fill at block
    // boundaries adds a slow wobble that repeats every ~26 s as packet and
    // block timing slide past each other, hence the 20% tolerance.
    sim.run(90.0);
    CHECK(sim.recentPpm(60.0) == doctest::Approx(50.0).epsilon(0.2));
}

TEST_CASE("InputStage: latency is the same before and after a 60 s silence, with no dropout")
{
    Sim sim;
    sim.run(10.0);
    const double before = sim.recentFillMs(3.0);

    sim.sourcePlaying = false;
    sim.run(60.0);
    CHECK_FALSE(sim.stage.stats().playing);

    sim.sourcePlaying = true;
    sim.run(10.0);
    const double after = sim.recentFillMs(3.0);

    CHECK(sim.stage.stats().dropouts() == 0);
    CHECK(sim.stage.stats().reserveMs == 3.0);
    CHECK(std::abs(after - before) < 0.5);
}

TEST_CASE("InputStage: repeated underruns grow the reserve 3 -> 6 ms, one dropout each")
{
    Sim sim;
    sim.run(5.0);
    const size_t start = sim.out.size();
    for (int i = 0; i < 5; ++i) {
        sim.packetDelay = 0.015;  // capture hiccup: one packet 15 ms late
        sim.run(3.0);
    }
    const auto& s = sim.stage.stats();
    CHECK(s.underruns == 5);
    CHECK(s.dropouts() == 5);
    CHECK(s.reserveMs == 6.0);
    CHECK(sim.gaps(start) == 5);  // growing the reserve added no extra gaps
    CHECK(std::abs(sim.recentFillMs(1.0) - s.targetMs) < 0.5);
}

TEST_CASE("InputStage: a small capture delay is absorbed by the reserve")
{
    Sim sim;
    sim.run(5.0);
    sim.packetDelay = 0.0015;
    sim.run(3.0);
    CHECK(sim.stage.stats().dropouts() == 0);
}

TEST_CASE("InputStage: an overrun drops audio with a fade and doesn't grow the reserve")
{
    Sim sim;
    sim.run(5.0);
    const size_t start = sim.out.size();
    sim.outputStalled = true;  // the ring (200 ms, rounded up to 341 ms) fills up
    sim.run(0.5);
    sim.outputStalled = false;
    sim.run(5.0);

    const auto& s = sim.stage.stats();
    CHECK(s.overruns == 1);
    CHECK(s.underruns == 0);
    CHECK(s.dropouts() == 1);
    CHECK(s.reserveMs == 3.0);
    CHECK(sim.maxStep(start) < kClickThreshold);  // faded, no click
    CHECK(std::abs(sim.recentFillMs(1.0) - s.targetMs) < 0.5);
}

TEST_CASE("InputStage counts capture discontinuities as dropouts")
{
    Sim sim;
    sim.run(2.0);
    sim.signals.discontinuities += 1;
    sim.run(1.0);
    CHECK(sim.stage.stats().discontinuities == 1);
    CHECK(sim.stage.stats().dropouts() == 1);
}

TEST_CASE("InputStage: Start resets the reserve and the counters")
{
    Sim sim;
    sim.run(2.0);
    sim.packetDelay = 0.015;
    sim.run(2.0);
    REQUIRE(sim.stage.stats().reserveMs == 4.0);
    sim.stage.start();
    CHECK(sim.stage.stats().reserveMs == 3.0);
    CHECK(sim.stage.stats().dropouts() == 0);
}

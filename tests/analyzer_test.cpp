// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "analysis/Analyzer.h"
#include "dsp/SpscRing.h"
#include "engine/RenderPath.h"

#include <doctest/doctest.h>

#include <cmath>
#include <vector>

using namespace gfx;

namespace {

constexpr double kFs = 48000.0;

std::vector<float> sine(double hz, double amplitude, double seconds)
{
    std::vector<float> v(static_cast<size_t>(seconds * kFs));
    for (size_t i = 0; i < v.size(); ++i) {
        v[i] = static_cast<float>(amplitude * std::sin(2 * M_PI * hz * i / kFs));
    }
    return v;
}

int bandOf(double hz)
{
    for (int b = 0; b < Analyzer::kBands; ++b) {
        if (hz >= Analyzer::edge(b) && hz < Analyzer::edge(b + 1)) {
            return b;
        }
    }
    return -1;
}

int loudestBand(const std::array<float, Analyzer::kBands>& bands)
{
    return static_cast<int>(std::max_element(bands.begin(), bands.end()) - bands.begin());
}

}  // namespace

TEST_CASE("Bands are log-spaced from 10 Hz to 2 kHz")
{
    CHECK(Analyzer::edge(0) == doctest::Approx(10.0));
    CHECK(Analyzer::edge(Analyzer::kBands) == doctest::Approx(2000.0));
    CHECK(Analyzer::edge(32) == doctest::Approx(std::sqrt(10.0 * 2000.0)));
}

TEST_CASE("A 50 Hz tone peaks in the band containing 50 Hz, at about its level")
{
    Analyzer a;
    a.prepare(kFs);
    const auto tone = sine(50.0, 0.5, 1.0);
    a.pushInput(tone.data(), tone.size());
    const auto s = a.snapshot(1.0 / 30.0);
    CHECK(loudestBand(s.in) == bandOf(50.0));
    CHECK(s.in[bandOf(50.0)] == doctest::Approx(-6.0).epsilon(0.1));  // 0.5 = -6 dBFS
    CHECK(s.inDb == doctest::Approx(-6.0).epsilon(0.01));
    CHECK(s.outDb == Analyzer::kFloorDb);
}

TEST_CASE("Low tones land in or right next to their own band")
{
    // Below ~20 Hz a band (under 1 Hz wide) is narrower than the FFT's main
    // lobe (~4 bins of 2.9 Hz), so allow two bands of spread there.
    for (double hz : {12.0, 15.0, 25.0, 40.0}) {
        Analyzer a;
        a.prepare(kFs);
        const auto tone = sine(hz, 0.5, 1.0);
        a.pushInput(tone.data(), tone.size());
        CAPTURE(hz);
        CHECK(std::abs(loudestBand(a.snapshot(0.03).in) - bandOf(hz)) <= (hz < 20.0 ? 2 : 1));
    }
}

TEST_CASE("Peaks reset after each snapshot")
{
    Analyzer a;
    a.prepare(kFs);
    const auto tone = sine(50.0, 0.5, 0.1);
    a.pushInput(tone.data(), tone.size());
    a.snapshot(0.03);
    CHECK(a.snapshot(0.03).inDb == Analyzer::kFloorDb);
}

TEST_CASE("No signal after 2 s below -60 dBFS, cleared as soon as input returns")
{
    Analyzer a;
    a.prepare(kFs);
    const auto quiet = sine(50.0, 0.0005, 0.04);  // -66 dBFS
    bool noSignal = false;
    for (int i = 0; i < 59; ++i) {  // 1.97 s
        a.pushInput(quiet.data(), quiet.size());
        noSignal = a.snapshot(1.0 / 30.0).noSignal;
    }
    CHECK_FALSE(noSignal);
    a.pushInput(quiet.data(), quiet.size());
    a.snapshot(1.0 / 30.0);
    a.pushInput(quiet.data(), quiet.size());
    CHECK(a.snapshot(1.0 / 30.0).noSignal);

    const auto loud = sine(50.0, 0.1, 0.04);
    a.pushInput(loud.data(), loud.size());
    CHECK_FALSE(a.snapshot(1.0 / 30.0).noSignal);
}

TEST_CASE("Through the render path: 1 kHz is gone from the output at an 80 Hz cutoff, test signals only show in the output")
{
    // Render path with taps, fed by a capture-less source (silence) plus a
    // 1 kHz input injected through a fake ring source.
    EngineSettings settings;  // 80 Hz, 24 dB/oct
    RenderPath path;
    SpscRing inTap(1 << 16), outTap(1 << 16);
    path.prepare(kFs, 64, settings);
    path.setTaps(&inTap, &outTap);

    struct ToneCapture final : CaptureDevice {
        OpenResult open(const SourceSpec&, SpscRing*) override { return {OpenStatus::Ok, {}}; }
        void setTap(SpscRing*) override {}
        bool start() override { return true; }
        void close() override {}
        const CaptureFormat& format() const override { return f; }
        CaptureSignals signals() const override { return {}; }
        bool failed() const override { return false; }
        CaptureFormat f{kFs, 2, 32, true, 480};
    } capture;
    SourceSlot slot;
    slot.stage.prepare(kFs, kFs, 480, 64);
    slot.capture = &capture;
    path.setSource(&slot);

    Analyzer a;
    a.prepare(kFs);
    std::vector<float> block(64), tap(64);
    const auto input = sine(1000.0, 0.5, 1.5);
    size_t written = 0;
    for (int b = 0; b < static_cast<int>(1.0 * kFs / 64); ++b) {
        while (slot.ring.size() < 2000 && written < input.size()) {
            written += slot.ring.write(input.data() + written, 480);
        }
        path.render(block.data(), 64);
        size_t n;
        while ((n = inTap.read(tap.data(), tap.size())) > 0) a.pushInput(tap.data(), n);
        while ((n = outTap.read(tap.data(), tap.size())) > 0) a.pushOutput(tap.data(), n);
    }
    auto s = a.snapshot(0.03);
    CHECK(s.in[bandOf(1000.0)] > -10.0);   // the input shows 1 kHz
    CHECK(s.out[bandOf(1000.0)] < -80.0);  // the output doesn't

    // Now a pulse with the input silent: only the output shows 40 Hz.
    slot.capture = nullptr;
    path.pulseStart();
    for (int b = 0; b < static_cast<int>(0.5 * kFs / 64); ++b) {
        path.render(block.data(), 64);
        size_t n;
        while ((n = inTap.read(tap.data(), tap.size())) > 0) a.pushInput(tap.data(), n);
        while ((n = outTap.read(tap.data(), tap.size())) > 0) a.pushOutput(tap.data(), n);
    }
    s = a.snapshot(0.03);
    CHECK(s.out[bandOf(40.0)] > -20.0);
    CHECK(s.inDb == Analyzer::kFloorDb);
}

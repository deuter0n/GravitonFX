// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include "dsp/FilterChain.h"
#include "dsp/InputStage.h"
#include "dsp/Limiter.h"
#include "dsp/SpscRing.h"
#include "dsp/TestSignals.h"
#include "engine/Backend.h"
#include "engine/EngineTypes.h"
#include "output/OutputDevice.h"

#include <atomic>
#include <vector>

namespace gfx {

// One capture source with its own ring buffer and input stage. The control
// thread fills it in (allocations happen there) and hands it to the render
// path; two of these let a source change open the new one before the old one
// closes.
struct SourceSlot {
    SpscRing ring{static_cast<size_t>(0.2 * 192000)};  // 200 ms at up to 192 kHz
    InputStage stage;
    CaptureDevice* capture = nullptr;  // null = silence
};

// Everything the output thread does for one block:
//   input stage -> filter chain (+ test signal) -> limiter -> fades -> mono out
// Settings and commands arrive through atomics; telemetry leaves the same
// way. render() never allocates, locks or waits.
class RenderPath final : public Renderer {
public:
    // Control thread, before the output starts.
    void prepare(double outputRate, int maxBlockFrames, const EngineSettings& settings);

    // Control thread, any time.
    void setParams(const FilterParams& filter, bool limiter);
    void setSource(SourceSlot* slot);       // swaps with a short fade
    bool sourceSwapped() const { return current_.load() == pending_.load(); }
    void clearSource() { current_ = nullptr; pending_ = nullptr; }  // only while the output is stopped
    void resetTests()  // only while the output is stopped: no test plays any more
    {
        tests_.prepare(fs_);
        testKind_ = 0;
        sweepHz_ = 0.0;
    }
    void fadeOut() { fadeOutRequested_ = true; }
    bool fadedOut() const { return fadedOut_.load(); }
    void pulseStart() { testCommand_ = kPulseStart; ++testSeq_; }
    void pulseStop() { testCommand_ = kStop; ++testSeq_; }
    void sweepStart() { testCommand_ = kSweepStart; ++testSeq_; }
    void testStop() { testCommand_ = kStop; ++testSeq_; }
    void setTaps(SpscRing* input, SpscRing* output) { inTap_ = input; outTap_ = output; }

    bool testActive() const { return testKind_.load() != 0; }
    bool pulsing() const { return testKind_.load() == 1; }
    bool sweeping() const { return testKind_.load() == 2; }
    Telemetry telemetry(double outputBufferMs, double capturePacketMs) const;

    // Output thread.
    void render(float* mono, size_t frames) override;

private:
    enum : int { kNone, kPulseStart, kSweepStart, kStop };

    double fs_ = 48000.0;
    FilterChain chain_;
    Limiter limiter_;
    TestSignals tests_;
    std::vector<float> in_, test_;

    // Settings from the control thread.
    std::atomic<double> cutoff_{80.0}, subsonicHz_{20.0}, gainDb_{0.0};
    std::atomic<int> slope_{24}, subsonicSlope_{12};
    std::atomic<bool> limiterOn_{true};

    // Source swap.
    std::atomic<SourceSlot*> current_{nullptr};
    std::atomic<SourceSlot*> pending_{nullptr};
    float sourceGain_ = 1.0f;

    // Output fade before the device closes.
    std::atomic<bool> fadeOutRequested_{false};
    std::atomic<bool> fadedOut_{false};
    float outputGain_ = 1.0f;
    float fadeStep_ = 0.0f;

    // Test signal commands (latest wins).
    std::atomic<int> testCommand_{kNone};
    std::atomic<unsigned> testSeq_{0};
    unsigned testSeen_ = 0;
    std::atomic<int> testKind_{0};  // 0 none, 1 pulse, 2 sweep

    SpscRing* inTap_ = nullptr;
    SpscRing* outTap_ = nullptr;

    // Telemetry.
    std::atomic<double> fillMs_{0.0}, targetMs_{0.0}, driftPpm_{0.0}, reserveMs_{0.0}, sweepHz_{0.0};
    std::atomic<bool> locked_{false}, limiterHit_{false}, clipped_{false}, playing_{false};
    std::atomic<uint32_t> dropouts_{0}, underruns_{0}, overruns_{0}, discontinuities_{0};
};

}  // namespace gfx

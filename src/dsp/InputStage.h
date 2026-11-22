// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include "dsp/DriftController.h"
#include "dsp/Reserve.h"
#include "dsp/Resampler.h"
#include "dsp/SpscRing.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace gfx {

// What the capture thread reports (through atomics) to the output thread.
struct CaptureSignals {
    bool idle = false;              // no packets for a while: the source is silent
    uint32_t overruns = 0;          // running count: the ring was full and audio was dropped
    uint32_t discontinuities = 0;   // running count: Windows flagged a capture glitch
};

struct InputStats {
    uint32_t underruns = 0;
    uint32_t overruns = 0;
    uint32_t discontinuities = 0;
    double fillMs = 0.0;     // smoothed ring fill
    double targetMs = 0.0;   // where the fill is steered to
    double reserveMs = 0.0;  // automatic safety reserve
    double driftPpm = 0.0;
    bool locked = false;
    bool playing = false;    // false while priming, silent or starved

    // The user-facing "Dropouts" counter.
    uint32_t dropouts() const { return underruns + overruns + discontinuities; }
};

// The output side of the ring buffer: reads captured mono audio at the
// capture rate and delivers it at the output rate, keeping latency low and
// constant.
//
//  - Priming: plays silence until the ring holds reserve + one capture
//    packet + one block, drops any excess (the planned catch-up after silence, not a
//    dropout), then fades in.
//  - Playing: resamples with the drift-corrected ratio.
//  - Starved: the ring ran dry. If the capture reported silence meanwhile it
//    was a pause; otherwise it was an underrun: count it and grow the reserve,
//    then prime again at the new level (so growing adds no further dropout).
//  - An overrun (or a large excess) fades out, drops audio back to the prime
//    level and fades in; it counts as a dropout but doesn't grow the reserve.
//
// Real-time safe after prepare().
class InputStage {
public:
    void prepare(double captureRate, double outputRate, size_t packetFrames, size_t maxBlockFrames);

    // Engine start: reserve back to 3 ms, drift and counters reset.
    void start();

    void pull(SpscRing& ring, const CaptureSignals& signals, float* out, size_t frames);

    const InputStats& stats() const { return stats_; }

private:
    enum class State { Priming, Playing, Starved };

    size_t primeFrames() const;
    double targetSeconds() const;
    void playSilence(float* out, size_t frames);
    void applyFade(float* out, size_t frames);

    double captureRate_ = 48000.0;
    double outputRate_ = 48000.0;
    size_t packetFrames_ = 480;

    State state_ = State::Priming;
    Reserve reserve_;
    DriftController drift_;
    Resampler resampler_;
    std::vector<float> scratch_;

    CaptureSignals seen_;
    bool idleSeen_ = false;     // capture reported silence while starved
    size_t starvedFill_ = 0;    // ring fill when starvation began
    bool catchUpPending_ = false;
    bool overrunPending_ = false;   // the pending catch-up clears the ring
    double blockInputFrames_ = 64.0; // capture frames read per block
    float fadeGain_ = 0.0f;
    float fadeTarget_ = 1.0f;
    float fadeStep_ = 0.0f;

    InputStats stats_;
};

}  // namespace gfx

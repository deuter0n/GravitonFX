// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include <cstddef>

namespace gfx {

// The pulse and sweep test signals, generated on the output thread.
//
//  - Pulse: 40 Hz while held, with 10 ms fades; stops by itself after 10 s
//    in case the release never arrives.
//  - Sweep: 20 -> 200 Hz in 5 Hz steps of 0.5 s (37 steps, 18.5 s), with
//    10 ms fades at the ends. The oscillator phase carries over between
//    steps, so step changes don't click.
//
// Only one test plays at a time: starting one while another plays switches
// over without a fade gap. Real-time safe.
class TestSignals {
public:
    static constexpr double kLevel = 0.5;  // -6 dBFS before gain
    static constexpr double kPulseHz = 40.0;
    static constexpr double kPulseLimitSeconds = 10.0;
    static constexpr double kSweepStartHz = 20.0;
    static constexpr double kSweepStepHz = 5.0;
    static constexpr int kSweepSteps = 37;  // 20, 25, ... 200 Hz
    static constexpr double kSweepStepSeconds = 0.5;
    static constexpr double kFadeSeconds = 0.010;

    enum class Kind { None, Pulse, Sweep };

    void prepare(double sampleRate);

    void startPulse() { start(Kind::Pulse); }
    void startSweep() { start(Kind::Sweep); }
    void stop() { stopping_ = kind_ != Kind::None; }  // fades out

    // Writes the test signal (silence when none plays) to `out`.
    void process(float* out, size_t frames);

    Kind kind() const { return kind_; }
    bool active() const { return kind_ != Kind::None; }

    // Frequency of the current sweep step, or 0 when no sweep plays.
    double sweepHz() const { return kind_ == Kind::Sweep ? frequency_ : 0.0; }

private:
    void start(Kind kind);

    double fs_ = 48000.0;
    Kind kind_ = Kind::None;
    bool stopping_ = false;
    double phase_ = 0.0;      // radians
    double frequency_ = 0.0;
    double gain_ = 0.0;       // fade envelope
    double fadeStep_ = 0.0;
    double elapsed_ = 0.0;    // samples since start
};

}  // namespace gfx

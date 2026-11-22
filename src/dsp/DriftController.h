// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

namespace gfx {

// Keeps the ring buffer fill at its target despite the capture and output
// clocks running at slightly different speeds.
//
// Once per output block it gets the ring fill (seconds of audio left after
// the block was read). The fill swings by about one packet (capture adds
// ~10 ms packets, output drains in blocks), so it is smoothed first. A PI controller on
// the smoothed error produces `trim`, which speeds up or slows down how fast
// the output consumes input:  ratio = captureRate / outputRate * (1 + trim).
// The trim is clamped to +-1000 ppm and reported as "clock drift".
//
// "Locked" means the smoothed error stayed within +-0.25 ms for 2 s.
class DriftController {
public:
    static constexpr double kMaxTrim = 1000e-6;

    // Forgets everything (engine start or device change).
    void reset();

    // Restarts the fill smoothing at the target but keeps the learned trim,
    // so latency after a pause equals latency before it.
    void resume(double targetSeconds);

    void update(double fillSeconds, double targetSeconds, double blockSeconds);

    double trim() const { return trim_; }
    double smoothedFillSeconds() const { return smoothedFill_; }
    bool locked() const { return lockedFor_ >= kLockSeconds; }

private:
    static constexpr double kSmoothingSeconds = 0.5;
    static constexpr double kKp = 0.8;   // 1/s
    static constexpr double kKi = 0.16;  // 1/s^2
    static constexpr double kLockTolerance = 0.25e-3;
    static constexpr double kLockSeconds = 2.0;

    double smoothedFill_ = 0.0;
    double integral_ = 0.0;
    double trim_ = 0.0;
    double lockedFor_ = 0.0;
};

}  // namespace gfx

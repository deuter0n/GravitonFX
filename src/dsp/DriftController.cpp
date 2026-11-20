// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "dsp/DriftController.h"

#include <algorithm>
#include <cmath>

namespace gfx {

void DriftController::reset()
{
    smoothedFill_ = 0.0;
    integral_ = 0.0;
    trim_ = 0.0;
    lockedFor_ = 0.0;
}

void DriftController::resume(double targetSeconds)
{
    smoothedFill_ = targetSeconds;
    lockedFor_ = 0.0;
}

void DriftController::update(double fillSeconds, double targetSeconds, double blockSeconds)
{
    smoothedFill_ += (fillSeconds - smoothedFill_) * (1.0 - std::exp(-blockSeconds / kSmoothingSeconds));
    const double error = smoothedFill_ - targetSeconds;

    // More audio buffered than wanted -> positive trim -> consume faster.
    // The integral stops growing while the trim is clamped (anti-windup).
    const double unclamped = kKp * error + kKi * (integral_ + error * blockSeconds);
    if (std::abs(unclamped) < kMaxTrim) {
        integral_ += error * blockSeconds;
    }
    trim_ = std::clamp(kKp * error + kKi * integral_, -kMaxTrim, kMaxTrim);

    lockedFor_ = std::abs(error) <= kLockTolerance ? lockedFor_ + blockSeconds : 0.0;
}

}  // namespace gfx

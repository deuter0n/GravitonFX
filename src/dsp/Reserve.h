// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include <algorithm>

namespace gfx {

// The automatic safety reserve: how much audio is kept buffered at the low
// point of the capture packet cycle. Starts at 3 ms on every engine start and
// grows by 1 ms per underrun, up to 6 ms. Not saved, not user-configurable.
class Reserve {
public:
    static constexpr double kStartMs = 3.0;
    static constexpr double kStepMs = 1.0;
    static constexpr double kMaxMs = 6.0;

    void reset() { ms_ = kStartMs; }
    void grow() { ms_ = std::min(kMaxMs, ms_ + kStepMs); }
    double ms() const { return ms_; }

private:
    double ms_ = kStartMs;
};

}  // namespace gfx

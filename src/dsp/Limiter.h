// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include <cstddef>

namespace gfx {

// Peak limiter with no lookahead (so it adds no latency): instant attack,
// about 50 ms release, then a hard clamp at -1 dBFS that guarantees the
// ceiling. When switched off, the gain glides back to unity and the signal
// passes untouched; anything above 0 dBFS then clips in the device and is
// reported as clipped. Real-time safe.
class Limiter {
public:
    static constexpr float kCeiling = 0.891250938f;  // -1 dBFS

    void prepare(double sampleRate);
    void setEnabled(bool enabled) { enabled_ = enabled; }

    // Processes in place.
    void process(float* samples, size_t frames);

    // Flags for the block just processed.
    bool engaged() const { return engaged_; }  // limiter reduced the level
    bool clipped() const { return clipped_; }  // output went above 0 dBFS

private:
    bool enabled_ = true;
    float gain_ = 1.0f;
    float release_ = 0.0f;  // per-sample recovery coefficient
    bool engaged_ = false;
    bool clipped_ = false;
};

}  // namespace gfx

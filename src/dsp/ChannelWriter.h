// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include <cstddef>

namespace gfx {

// Device sample formats the output drivers can open.
enum class SampleFormat {
    Float32,    // 32-bit float
    Int16,      // 16-bit integer
    Int24,      // 24-bit integer, packed in 3 bytes
    Int24In32,  // 24-bit integer in the top bits of a 32-bit container
    Int32,      // 32-bit integer
};

size_t bytesPerSample(SampleFormat format);

// Writes the mono shaker signal to channels 1 and 2 of an interleaved device
// buffer, converted to the device format, and zero-fills every other
// channel. Values are clamped to the format's range. Real-time safe.
void writeInterleaved(const float* mono, void* device, size_t frames, int channels, SampleFormat format);

// Converts mono samples to one non-interleaved channel buffer (ASIO).
void writeChannel(const float* mono, void* channel, size_t frames, SampleFormat format);

}  // namespace gfx

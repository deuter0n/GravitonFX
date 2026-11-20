// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "dsp/ChannelWriter.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace gfx {

namespace {

// Stores one sample in `format` at `dst` and returns the next position.
uint8_t* store(float x, uint8_t* dst, SampleFormat format)
{
    const double v = std::clamp(static_cast<double>(x), -1.0, 1.0);
    switch (format) {
    case SampleFormat::Float32: {
        const float f = static_cast<float>(v);
        std::memcpy(dst, &f, 4);
        return dst + 4;
    }
    case SampleFormat::Int16: {
        const int16_t s = static_cast<int16_t>(std::lround(v * 32767.0));
        std::memcpy(dst, &s, 2);
        return dst + 2;
    }
    case SampleFormat::Int24: {
        const int32_t s = static_cast<int32_t>(std::lround(v * 8388607.0));
        dst[0] = static_cast<uint8_t>(s & 0xff);  // little endian, 3 bytes
        dst[1] = static_cast<uint8_t>((s >> 8) & 0xff);
        dst[2] = static_cast<uint8_t>((s >> 16) & 0xff);
        return dst + 3;
    }
    case SampleFormat::Int24In32: {
        const int32_t s = static_cast<int32_t>(std::lround(v * 8388607.0)) * 256;  // top 24 bits
        std::memcpy(dst, &s, 4);
        return dst + 4;
    }
    case SampleFormat::Int32: {
        const int32_t s = static_cast<int32_t>(std::llround(v * 2147483647.0));
        std::memcpy(dst, &s, 4);
        return dst + 4;
    }
    }
    return dst;
}

}  // namespace

size_t bytesPerSample(SampleFormat format)
{
    switch (format) {
    case SampleFormat::Int16:
        return 2;
    case SampleFormat::Int24:
        return 3;
    default:
        return 4;
    }
}

void writeInterleaved(const float* mono, void* device, size_t frames, int channels, SampleFormat format)
{
    auto* dst = static_cast<uint8_t*>(device);
    const size_t silentBytes = static_cast<size_t>(std::max(0, channels - 2)) * bytesPerSample(format);
    for (size_t i = 0; i < frames; ++i) {
        dst = store(mono[i], dst, format);  // channel 1
        if (channels >= 2) {
            dst = store(mono[i], dst, format);  // channel 2
        }
        std::memset(dst, 0, silentBytes);  // channels 3..N (zero is silence in every format)
        dst += silentBytes;
    }
}

void writeChannel(const float* mono, void* channel, size_t frames, SampleFormat format)
{
    auto* dst = static_cast<uint8_t*>(channel);
    for (size_t i = 0; i < frames; ++i) {
        dst = store(mono[i], dst, format);
    }
}

}  // namespace gfx

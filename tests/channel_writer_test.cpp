// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "dsp/ChannelWriter.h"

#include <doctest/doctest.h>

#include <cstdint>
#include <cstring>
#include <vector>

using namespace gfx;

TEST_CASE("Mono goes to channels 1 and 2 of a stereo float buffer")
{
    const float mono[3] = {0.25f, -0.5f, 1.0f};
    std::vector<float> dev(6, 9.0f);
    writeInterleaved(mono, dev.data(), 3, 2, SampleFormat::Float32);
    CHECK(dev == std::vector<float>{0.25f, 0.25f, -0.5f, -0.5f, 1.0f, 1.0f});
}

TEST_CASE("On an 8-channel buffer, channels 3-8 are silent")
{
    const float mono[2] = {0.5f, -0.25f};
    std::vector<float> dev(16, 9.0f);
    writeInterleaved(mono, dev.data(), 2, 8, SampleFormat::Float32);
    for (int frame = 0; frame < 2; ++frame) {
        CHECK(dev[frame * 8 + 0] == mono[frame]);
        CHECK(dev[frame * 8 + 1] == mono[frame]);
        for (int ch = 2; ch < 8; ++ch) {
            CHECK(dev[frame * 8 + ch] == 0.0f);
        }
    }
}

TEST_CASE("16-bit output on 8 channels: conversion, clamping and silent extra channels")
{
    const float mono[3] = {0.5f, -1.0f, 2.0f};  // 2.0 is clamped
    std::vector<int16_t> dev(24, 99);
    writeInterleaved(mono, dev.data(), 3, 8, SampleFormat::Int16);
    CHECK(dev[0] == 16384);
    CHECK(dev[1] == 16384);
    CHECK(dev[8] == -32767);
    CHECK(dev[16] == 32767);
    CHECK(dev[17] == 32767);
    for (int ch = 2; ch < 8; ++ch) {
        CHECK(dev[ch] == 0);
        CHECK(dev[16 + ch] == 0);
    }
}

TEST_CASE("24-bit packed and 24-in-32 formats")
{
    const float mono[1] = {0.5f};

    std::vector<uint8_t> packed(3 * 4, 0xAA);
    writeInterleaved(mono, packed.data(), 1, 4, SampleFormat::Int24);
    const int32_t left = packed[0] | (packed[1] << 8) | (static_cast<int8_t>(packed[2]) << 16);
    CHECK(left == 4194304);  // 0.5 * 2^23, rounded
    CHECK(std::memcmp(packed.data(), packed.data() + 3, 3) == 0);
    CHECK(packed[6] == 0);
    CHECK(packed[11] == 0);

    std::vector<int32_t> in32(2);
    writeInterleaved(mono, in32.data(), 1, 2, SampleFormat::Int24In32);
    CHECK(in32[0] == 4194304 * 256);
    CHECK((in32[0] & 0xff) == 0);
}

TEST_CASE("32-bit integer full scale")
{
    const float mono[2] = {1.0f, -1.0f};
    std::vector<int32_t> dev(4);
    writeInterleaved(mono, dev.data(), 2, 2, SampleFormat::Int32);
    CHECK(dev[0] == 2147483647);
    CHECK(dev[2] == -2147483647);
}

TEST_CASE("Non-interleaved channel writer for ASIO")
{
    const float mono[3] = {0.5f, 0.0f, -0.5f};
    std::vector<int16_t> ch(3);
    writeChannel(mono, ch.data(), 3, SampleFormat::Int16);
    CHECK(ch == std::vector<int16_t>{16384, 0, -16384});
}

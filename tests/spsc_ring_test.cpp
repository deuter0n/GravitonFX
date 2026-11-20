// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "dsp/SpscRing.h"

#include <doctest/doctest.h>

#include <thread>
#include <vector>

using gfx::SpscRing;

TEST_CASE("SpscRing rounds its capacity up to a power of two")
{
    CHECK(SpscRing(1000).capacity() == 1024);
    CHECK(SpscRing(1024).capacity() == 1024);
}

TEST_CASE("SpscRing reports empty and full")
{
    SpscRing ring(8);
    float out[8]{};
    CHECK(ring.size() == 0);
    CHECK(ring.read(out, 8) == 0);

    const float in[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    CHECK(ring.write(in, 10) == 8);  // only 8 fit
    CHECK(ring.size() == 8);
    CHECK(ring.write(in, 1) == 0);   // full
}

TEST_CASE("SpscRing keeps order across the wraparound")
{
    SpscRing ring(8);
    float value = 0.0f;
    float expected = 0.0f;
    for (int round = 0; round < 100; ++round) {
        float in[5];
        for (float& v : in) {
            v = value++;
        }
        REQUIRE(ring.write(in, 5) == 5);
        float out[5];
        REQUIRE(ring.read(out, 5) == 5);
        for (float v : out) {
            CHECK(v == expected++);
        }
    }
}

TEST_CASE("SpscRing peek leaves data in place and skip removes it")
{
    SpscRing ring(8);
    const float in[4] = {1, 2, 3, 4};
    ring.write(in, 4);
    float out[4]{};
    CHECK(ring.peek(out, 4) == 4);
    CHECK(ring.size() == 4);
    CHECK(ring.skip(3) == 3);
    CHECK(ring.read(out, 4) == 1);
    CHECK(out[0] == 4);
    ring.writeSilence(2);
    ring.read(out, 2);
    CHECK(out[0] == 0.0f);
    CHECK(out[1] == 0.0f);
}

TEST_CASE("SpscRing passes a counting sequence between two threads")
{
    SpscRing ring(256);
    constexpr int kTotal = 2'000'000;

    std::thread producer([&] {
        float block[37];
        int next = 0;
        while (next < kTotal) {
            const int n = std::min(37, kTotal - next);
            for (int i = 0; i < n; ++i) {
                block[i] = static_cast<float>(next + i);
            }
            int written = 0;
            while (written < n) {
                written += static_cast<int>(ring.write(block + written, n - written));
            }
            next += n;
        }
    });

    int expected = 0;
    bool inOrder = true;
    float block[53];
    while (expected < kTotal) {
        const size_t n = ring.read(block, 53);
        for (size_t i = 0; i < n; ++i) {
            inOrder = inOrder && (block[i] == static_cast<float>(expected++));
        }
    }
    producer.join();
    CHECK(inOrder);
    CHECK(ring.size() == 0);
}

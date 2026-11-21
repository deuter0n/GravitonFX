// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "engine/RenderPath.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace gfx;

namespace {

// Peak of the last 0.25 s of a 1 s pulse through the whole render path.
float pulsePeak(const EngineSettings& s)
{
    RenderPath r;
    r.prepare(48000.0, 64, s);
    r.pulseStart();
    std::vector<float> block(64);
    float peak = 0.0f;
    for (int b = 0; b < 750; ++b) {  // 1 s
        r.render(block.data(), block.size());
        if (b >= 562) {
            for (float v : block) {
                peak = std::max(peak, std::abs(v));
            }
        }
    }
    return peak;
}

}  // namespace

TEST_CASE("Output limiter setting: on keeps the shaker signal under -1 dBFS, off lets it through")
{
    EngineSettings s;
    s.filter.gainDb = 12.0;  // the -6 dBFS test pulse becomes about +6 dBFS
    s.limiter = true;
    CHECK(pulsePeak(s) <= std::pow(10.0f, -1.0f / 20.0f) + 1e-6f);
    s.limiter = false;
    CHECK(pulsePeak(s) > 1.5f);
}

TEST_CASE("Subsonic setting reaches the render path")
{
    // A 40 Hz pulse barely changes with a 20 Hz subsonic filter, but a 30 Hz
    // steep filter takes a measurable bit off it.
    EngineSettings s;
    s.filter.subsonicHz = 0.0;
    const float off = pulsePeak(s);
    s.filter.subsonicHz = 30.0;
    s.filter.subsonicSlope = 24;
    const float steep30 = pulsePeak(s);
    CHECK(off == doctest::Approx(0.5).epsilon(0.02));
    CHECK(steep30 < off * 0.97f);
}

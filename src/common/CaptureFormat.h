// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include <cstddef>

namespace gfx {

struct CaptureFormat {
    double sampleRate = 0.0;
    int channels = 0;
    int bits = 0;             // as shown to the user
    bool isFloat = false;
    size_t packetFrames = 0;  // typical frames per capture packet (~10 ms)

    bool operator==(const CaptureFormat&) const = default;
};

}  // namespace gfx

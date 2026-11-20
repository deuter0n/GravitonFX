// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "dsp/Butterworth.h"

#include <algorithm>
#include <cmath>

namespace gfx {

int designButterworth(FilterType type, int order, double hz, double fs,
                      std::array<BiquadCoeffs, kMaxSections>& out)
{
    const int sections = std::clamp(order / 2, 1, kMaxSections);
    const int n = sections * 2;

    // Bilinear transform with prewarping, so the -3 dB point lands exactly on
    // `hz`. Each section gets the Q of one Butterworth pole pair.
    const double w0 = 2.0 * M_PI * std::clamp(hz, 1.0, 0.45 * fs) / fs;
    const double cosW = std::cos(w0);
    const double sinW = std::sin(w0);

    for (int k = 0; k < sections; ++k) {
        const double q = 1.0 / (2.0 * std::cos(M_PI * (2 * k + 1) / (2.0 * n)));
        const double alpha = sinW / (2.0 * q);
        const double a0 = 1.0 + alpha;

        BiquadCoeffs& c = out[k];
        if (type == FilterType::LowPass) {
            c.b0 = (1.0 - cosW) / 2.0 / a0;
            c.b1 = (1.0 - cosW) / a0;
        } else {
            c.b0 = (1.0 + cosW) / 2.0 / a0;
            c.b1 = -(1.0 + cosW) / a0;
        }
        c.b2 = c.b0;
        c.a1 = -2.0 * cosW / a0;
        c.a2 = (1.0 - alpha) / a0;
    }
    return sections;
}

}  // namespace gfx

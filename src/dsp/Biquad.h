// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include <cmath>
#include <complex>

namespace gfx {

// Coefficients of one second-order section, normalized so a0 = 1:
//   H(z) = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2)
struct BiquadCoeffs {
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0;

    // Magnitude of the response at `hz` for sample rate `fs` (used by tests).
    double magnitudeAt(double hz, double fs) const
    {
        const std::complex<double> z1 = std::polar(1.0, -2.0 * M_PI * hz / fs);  // z^-1
        const std::complex<double> z2 = z1 * z1;
        return std::abs((b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2));
    }
};

// One second-order section in transposed direct form II with double state.
// Coefficients can change between samples without resetting the state.
class Biquad {
public:
    void setCoeffs(const BiquadCoeffs& c) { c_ = c; }
    void reset() { s1_ = s2_ = 0.0; }

    double process(double x)
    {
        const double y = c_.b0 * x + s1_;
        s1_ = c_.b1 * x - c_.a1 * y + s2_;
        s2_ = c_.b2 * x - c_.a2 * y;
        return y;
    }

private:
    BiquadCoeffs c_;
    double s1_ = 0.0;
    double s2_ = 0.0;
};

}  // namespace gfx

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include "dsp/Biquad.h"

#include <array>

namespace gfx {

// Butterworth filters built from cascaded biquads. The order is the slope in
// dB/oct divided by 6 (12 -> 2, 24 -> 4, 36 -> 6, 48 -> 8), so each design
// has 1 to 4 sections.
constexpr int kMaxSections = 4;

enum class FilterType { LowPass, HighPass };

// Designs the sections for a Butterworth filter of even `order` (2 to 8) at
// cutoff `hz` and sample rate `fs`. Returns the number of sections used.
int designButterworth(FilterType type, int order, double hz, double fs,
                      std::array<BiquadCoeffs, kMaxSections>& out);

inline int orderForSlope(int dbPerOct) { return dbPerOct / 6; }

// A cascade of up to four biquads. Real-time safe: no allocation.
class Butterworth {
public:
    Butterworth(FilterType type, int order) : type_(type), order_(order) {}

    // Recomputes the coefficients; the filter state is kept, so this can be
    // called once per block while audio flows.
    void design(double hz, double fs)
    {
        std::array<BiquadCoeffs, kMaxSections> c;
        sections_ = designButterworth(type_, order_, hz, fs, c);
        for (int i = 0; i < sections_; ++i) {
            stages_[i].setCoeffs(c[i]);
        }
    }

    void reset()
    {
        for (auto& s : stages_) {
            s.reset();
        }
    }

    double process(double x)
    {
        for (int i = 0; i < sections_; ++i) {
            x = stages_[i].process(x);
        }
        return x;
    }

private:
    FilterType type_;
    int order_;
    int sections_ = 0;
    std::array<Biquad, kMaxSections> stages_;
};

}  // namespace gfx

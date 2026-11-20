// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "analysis/Analyzer.h"

#include <algorithm>
#include <cmath>

namespace gfx {

void fft(std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    // Bit-reversal permutation.
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(a[i], a[j]);
        }
    }
    // Butterflies.
    for (size_t len = 2; len <= n; len <<= 1) {
        const std::complex<double> step = std::polar(1.0, -2.0 * M_PI / static_cast<double>(len));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w = 1.0;
            for (size_t k = 0; k < len / 2; ++k) {
                const auto u = a[i + k];
                const auto v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= step;
            }
        }
    }
}

double Analyzer::edge(int band)
{
    return kLowHz * std::pow(kHighHz / kLowHz, static_cast<double>(band) / kBands);
}

void Analyzer::prepare(double sampleRate)
{
    fs_ = sampleRate;
    // About 0.34 s of audio: fine enough for the 10-20 Hz bands.
    n_ = sampleRate > 96000.0 ? 65536 : (sampleRate > 48000.0 ? 32768 : 16384);
    for (History* h : {&in_, &out_}) {
        h->samples.assign(n_, 0.0f);
        h->pos = 0;
        h->peak = 0.0f;
    }
    window_.resize(n_);
    for (size_t i = 0; i < n_; ++i) {
        window_[i] = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * M_PI * static_cast<double>(i) / static_cast<double>(n_)));
    }
    fft_.resize(n_);
    quietFor_ = 0.0;
}

void Analyzer::push(History& h, const float* samples, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        h.samples[h.pos] = samples[i];
        h.pos = (h.pos + 1) % n_;
        h.peak = std::max(h.peak, std::abs(samples[i]));
    }
}

void Analyzer::spectrum(const History& h, std::array<float, kBands>& bands)
{
    for (size_t i = 0; i < n_; ++i) {
        fft_[i] = h.samples[(h.pos + i) % n_] * window_[i];  // oldest first
    }
    fft(fft_);

    // A full-scale sine gives a peak of N/4 with a Hann window.
    const double scale = 4.0 / static_cast<double>(n_);
    const double binHz = fs_ / static_cast<double>(n_);
    for (int b = 0; b < kBands; ++b) {
        const double lo = edge(b), hi = edge(b + 1);
        size_t first = static_cast<size_t>(std::ceil(lo / binHz));
        size_t last = static_cast<size_t>(std::floor(hi / binHz));
        if (first > last) {  // band narrower than a bin: use the nearest bin
            first = last = static_cast<size_t>(std::lround(std::sqrt(lo * hi) / binHz));
        }
        double peak = 0.0;
        for (size_t k = first; k <= last && k < n_ / 2; ++k) {
            peak = std::max(peak, std::abs(fft_[k]) * scale);
        }
        bands[b] = static_cast<float>(peak > 0.0 ? std::max(kFloorDb, 20.0 * std::log10(peak)) : kFloorDb);
    }
}

Analyzer::Snapshot Analyzer::snapshot(double elapsedSeconds)
{
    Snapshot s;
    auto toDb = [](float peak) { return peak > 0.0f ? std::max(kFloorDb, 20.0 * std::log10(peak)) : kFloorDb; };
    s.inDb = toDb(in_.peak);
    s.outDb = toDb(out_.peak);
    in_.peak = 0.0f;
    out_.peak = 0.0f;
    spectrum(in_, s.in);
    spectrum(out_, s.out);

    quietFor_ = s.inDb < kNoSignalDb ? quietFor_ + elapsedSeconds : 0.0;
    s.noSignal = quietFor_ >= kNoSignalSeconds;
    return s;
}

}  // namespace gfx

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include <array>
#include <complex>
#include <cstddef>
#include <vector>

namespace gfx {

// Levels and spectra for the dashboard, computed off the audio threads from
// copies of the input (captured, before filtering) and output (shaker)
// signals.
//
//  - Peak level of each signal since the previous snapshot, in dBFS.
//  - 64 log-spaced bands from 10 Hz to 2 kHz, in dBFS (a full-scale sine
//    reads 0), from a Hann-windowed FFT of the most recent samples.
//  - "No signal": input below -60 dBFS for 2 s.
class Analyzer {
public:
    static constexpr int kBands = 64;
    static constexpr double kLowHz = 10.0;
    static constexpr double kHighHz = 2000.0;
    static constexpr double kFloorDb = -120.0;
    static constexpr double kNoSignalDb = -60.0;
    static constexpr double kNoSignalSeconds = 2.0;

    struct Snapshot {
        double inDb = kFloorDb;
        double outDb = kFloorDb;
        std::array<float, kBands> in{};
        std::array<float, kBands> out{};
        bool noSignal = false;
    };

    void prepare(double sampleRate);
    double sampleRate() const { return fs_; }

    void pushInput(const float* samples, size_t count) { push(in_, samples, count); }
    void pushOutput(const float* samples, size_t count) { push(out_, samples, count); }

    // Computes a snapshot; `elapsedSeconds` since the previous one drives the
    // no-signal timer. Resets the peaks.
    Snapshot snapshot(double elapsedSeconds);

    // Forgets the no-signal timer (engine start).
    void resetNoSignal() { quietFor_ = 0.0; }

    // Band edges, for tests: band b covers [edge(b), edge(b + 1)).
    static double edge(int band);

private:
    struct History {
        std::vector<float> samples;  // circular, the most recent N samples
        size_t pos = 0;
        float peak = 0.0f;
    };

    void push(History& h, const float* samples, size_t count);
    void spectrum(const History& h, std::array<float, kBands>& bands);

    double fs_ = 48000.0;
    size_t n_ = 16384;
    History in_, out_;
    std::vector<float> window_;
    std::vector<std::complex<double>> fft_;
    double quietFor_ = 0.0;
};

// In-place radix-2 FFT; the size must be a power of two.
void fft(std::vector<std::complex<double>>& data);

}  // namespace gfx

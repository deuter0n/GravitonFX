// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include "common/CaptureFormat.h"
#include "dsp/FilterChain.h"
#include "engine/Backend.h"
#include "output/OutputDevice.h"

#include <array>
#include <cstdint>
#include <string>

namespace gfx {

// The settings the engine acts on (a subset of the app settings).
struct EngineSettings {
    enum class SourceMode { SystemMix, App };

    SourceMode sourceMode = SourceMode::SystemMix;
    bool captureFollowsDefault = true;  // System mix on "Windows default"
    std::wstring captureDeviceId;       // fixed System mix device
    std::wstring captureDeviceName;     // shown while it's missing
    std::wstring appExe;                // App mode target, e.g. "RaceSim.exe"

    OutputDriver driver = OutputDriver::WasapiExclusive;
    std::wstring shakerDeviceId;        // endpoint ID or ASIO driver ID
    std::wstring shakerDeviceName;      // shown while it's missing
    int bufferFrames = 64;

    FilterParams filter;
    bool limiter = true;

    bool sameSource(const EngineSettings& o) const
    {
        return sourceMode == o.sourceMode && captureFollowsDefault == o.captureFollowsDefault &&
               captureDeviceId == o.captureDeviceId && appExe == o.appExe;
    }
    bool sameOutput(const EngineSettings& o) const
    {
        return driver == o.driver && shakerDeviceId == o.shakerDeviceId && bufferFrames == o.bufferFrames;
    }
};

enum class Phase { Stopped, Starting, Running, Waiting, Switching, Error };

// Which panel a message belongs to.
enum class Panel { None, Source, Shaker };

struct EngineState {
    Phase phase = Phase::Stopped;
    bool checkOutput = false;  // System mix captures the shaker device: shown as "Check output"

    Panel messagePanel = Panel::None;
    bool messageIsError = false;  // false: a "waiting for" message (blue dot)
    bool retrying = false;        // Error that retries by itself (the UI adds "Retrying…")
    std::wstring message;

    std::wstring testMessage;  // short-lived, Shaker panel (test couldn't open the output)
    bool pulsing = false;
    bool sweeping = false;

    bool captureOpen = false;
    CaptureFormat captureFormat;
    bool outputOpen = false;
    OutputFormat outputFormat;

    bool operator==(const EngineState&) const = default;
};

// Live values for the dashboard, read about 30 times a second.
struct Telemetry {
    double latencyMs = 0.0;
    double driftPpm = 0.0;
    double fill = 0.0;  // 0..1, 0.5 = on target
    bool locked = false;
    uint32_t dropouts = 0;
    double reserveMs = 0.0;
    bool limiterHit = false;
    bool clipped = false;
    double sweepHz = 0.0;
    bool test = false;  // output-only test telemetry: leave engine health alone

    // From the analysis thread.
    double inDb = -120.0;   // captured input peak
    double outDb = -120.0;  // shaker output peak
    bool noSignal = false;  // input below -60 dBFS for 2 s while Running
    std::array<float, 64> spectrumIn{};   // 10 Hz - 2 kHz, dBFS
    std::array<float, 64> spectrumOut{};
};

}  // namespace gfx

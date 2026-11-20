// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include "common/CaptureFormat.h"
#include "common/OpenResult.h"
#include "dsp/InputStage.h"
#include "dsp/SpscRing.h"
#include "output/OutputDevice.h"

#include <cstdint>
#include <memory>
#include <string>

namespace gfx {

// What to capture: a render device's mix (System mix) or one process tree (App).
struct SourceSpec {
    enum class Kind { Loopback, Process } kind = Kind::Loopback;
    std::wstring deviceId;  // Loopback
    uint32_t processId = 0; // Process
};

class CaptureDevice {
public:
    virtual ~CaptureDevice() = default;
    virtual OpenResult open(const SourceSpec& spec, SpscRing* ring) = 0;
    virtual void setTap(SpscRing* tap) = 0;
    virtual bool start() = 0;
    virtual void close() = 0;
    virtual const CaptureFormat& format() const = 0;
    virtual CaptureSignals signals() const = 0;  // real-time safe
    virtual bool failed() const = 0;
};

enum class OutputDriver { WasapiExclusive, Asio };

// Everything the engine needs from the system. The Windows implementation
// wraps WASAPI; tests use fakes.
class Backend {
public:
    virtual ~Backend() = default;

    virtual std::wstring defaultRenderId() = 0;
    virtual bool isConnected(const std::wstring& deviceId) = 0;  // render device or ASIO driver
    virtual uint32_t findProcess(const std::wstring& exe) = 0;   // 0 = no audio session

    virtual std::unique_ptr<CaptureDevice> makeCapture() = 0;
    virtual std::unique_ptr<OutputDevice> makeOutput(OutputDriver driver) = 0;

    // Monotonic clock in the same units as OutputDevice::lastCallbackTime.
    virtual int64_t now() = 0;
    virtual int64_t ticksPerSecond() = 0;

    // Lets the output thread run while the control thread waits for a fade
    // (a short sleep on Windows, a manual pump in tests).
    virtual void waitBriefly() = 0;
};

}  // namespace gfx

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include "capture/DeviceEnumerator.h"
#include "capture/SessionWatcher.h"
#include "engine/Backend.h"

namespace gfx {

// The real system: WASAPI devices, audio sessions and the performance
// counter. Create and use it on the engine's control thread (COM MTA).
class WindowsBackend final : public Backend {
public:
    WindowsBackend();

    DeviceEnumerator& devices() { return devices_; }
    SessionWatcher& sessions() { return sessions_; }

    std::wstring defaultRenderId() override;
    bool isConnected(const std::wstring& deviceId) override;
    uint32_t findProcess(const std::wstring& exe) override;
    std::unique_ptr<CaptureDevice> makeCapture() override;
    std::unique_ptr<OutputDevice> makeOutput(OutputDriver driver) override;
    int64_t now() override;
    int64_t ticksPerSecond() override { return frequency_; }
    void waitBriefly() override;

private:
    DeviceEnumerator devices_;
    SessionWatcher sessions_;
    int64_t frequency_ = 1;
};

}  // namespace gfx

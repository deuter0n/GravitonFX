// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "engine/WindowsBackend.h"

#include "capture/WasapiCapture.h"
#include "output/Asio.h"
#include "output/WasapiExclusiveOutput.h"

#include <windows.h>

namespace gfx {

namespace {

// Adapts WasapiCapture to the engine's CaptureDevice interface.
class WasapiCaptureDevice final : public CaptureDevice {
public:
    explicit WasapiCaptureDevice(IMMDeviceEnumerator* enumerator) : capture_(enumerator) {}

    OpenResult open(const SourceSpec& spec, SpscRing* ring) override
    {
        return spec.kind == SourceSpec::Kind::Loopback ? capture_.openLoopback(spec.deviceId, ring)
                                                       : capture_.openProcess(spec.processId, ring);
    }
    void setTap(SpscRing* tap) override { capture_.setTap(tap); }
    bool start() override { return capture_.start(); }
    void close() override { capture_.close(); }
    const CaptureFormat& format() const override { return capture_.format(); }
    CaptureSignals signals() const override { return capture_.signals(); }
    bool failed() const override { return capture_.failed(); }

private:
    WasapiCapture capture_;
};

}  // namespace

WindowsBackend::WindowsBackend() : sessions_(devices_.native())
{
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    frequency_ = f.QuadPart;
}

std::wstring WindowsBackend::defaultRenderId() { return devices_.defaultRenderId(); }

bool WindowsBackend::isConnected(const std::wstring& deviceId)
{
    AudioDevice device;
    AsioDriver driver;
    return devices_.find(deviceId, device) || findAsioDriver(deviceId, driver);
}

uint32_t WindowsBackend::findProcess(const std::wstring& exe) { return sessions_.findProcess(exe); }

double WindowsBackend::renderRate(const std::wstring& deviceId)
{
    AudioDevice device;
    return devices_.find(deviceId, device) ? device.sampleRate : 0.0;
}

bool WindowsBackend::processAlive(uint32_t pid)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) {
        return false;
    }
    DWORD code = 0;
    const bool alive = GetExitCodeProcess(process, &code) && code == STILL_ACTIVE;
    CloseHandle(process);
    return alive;
}

std::unique_ptr<CaptureDevice> WindowsBackend::makeCapture()
{
    return std::make_unique<WasapiCaptureDevice>(devices_.native());
}

std::unique_ptr<OutputDevice> WindowsBackend::makeOutput(OutputDriver driver)
{
    if (driver == OutputDriver::Asio) {
        return makeAsioOutput();
    }
    return std::make_unique<WasapiExclusiveOutput>(devices_.native());
}

int64_t WindowsBackend::now()
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

void WindowsBackend::waitBriefly() { Sleep(1); }

}  // namespace gfx

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "engine/WindowsBackend.h"

#include "capture/WasapiCapture.h"
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
    AudioDevice ignored;
    return devices_.find(deviceId, ignored);
}

uint32_t WindowsBackend::findProcess(const std::wstring& exe) { return sessions_.findProcess(exe); }

std::unique_ptr<CaptureDevice> WindowsBackend::makeCapture()
{
    return std::make_unique<WasapiCaptureDevice>(devices_.native());
}

std::unique_ptr<OutputDevice> WindowsBackend::makeOutput(OutputDriver)
{
    // ASIO arrives with task group 10; until then every output is WASAPI.
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

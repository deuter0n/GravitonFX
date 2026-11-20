// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

// gfxprobe: a console tool for checking the audio layers by hand.
//
//   gfxprobe --list-devices          list render devices
//   gfxprobe --list-devices --watch  ...and print changes until Enter
//   gfxprobe --test-tone <n|id>      40 Hz on outputs 1-2 at each buffer size
//   gfxprobe --list-apps             apps with audio sessions
//   gfxprobe --capture <default|n|id> [seconds]   System mix capture levels
//   gfxprobe --capture-app <exe> [seconds]        App capture levels

#include "capture/DeviceEnumerator.h"
#include "capture/SessionWatcher.h"
#include "capture/WasapiCapture.h"
#include "output/WasapiExclusiveOutput.h"

#include <windows.h>
#include <objbase.h>
#include <wil/com.h>

#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace gfx;

namespace {

void printDevices(const DeviceEnumerator& devices)
{
    const auto list = devices.renderDevices();
    std::wprintf(L"%zu render device(s):\n", list.size());
    for (size_t i = 0; i < list.size(); ++i) {
        const auto& d = list[i];
        std::wprintf(L"  [%zu]%s %ls\n       %d Hz, %d ch, id %ls\n", i, d.isDefault ? L" (default)" : L"",
                     d.name.c_str(), d.sampleRate, d.channels, d.id.c_str());
    }
}

const wchar_t* eventName(DeviceEvent e)
{
    switch (e) {
    case DeviceEvent::DefaultChanged: return L"default changed";
    case DeviceEvent::Added: return L"added";
    case DeviceEvent::Removed: return L"removed";
    case DeviceEvent::StateChanged: return L"state changed";
    }
    return L"?";
}

int listDevices(bool watch)
{
    DeviceEnumerator devices;
    printDevices(devices);
    if (!watch) {
        return 0;
    }
    devices.setListener([](DeviceEvent e, const std::wstring& id) {
        std::wprintf(L"\n* %ls: %ls\n", eventName(e), id.c_str());
    });
    std::wprintf(L"\nWatching for changes. Plug or unplug a device; press Enter to stop.\n");
    std::wstring line;
    std::getline(std::wcin, line);
    std::wprintf(L"\nNow:\n");
    printDevices(devices);
    return 0;
}

// A quiet 40 Hz sine (-12 dBFS).
class ToneRenderer final : public Renderer {
public:
    explicit ToneRenderer(double rate) : step_(2.0 * M_PI * 40.0 / rate) {}
    void render(float* mono, size_t frames) override
    {
        for (size_t i = 0; i < frames; ++i) {
            mono[i] = static_cast<float>(0.25 * std::sin(phase_));
            phase_ = std::fmod(phase_ + step_, 2.0 * M_PI);
        }
    }

private:
    double step_;
    double phase_ = 0.0;
};

const wchar_t* formatName(const OutputFormat& f)
{
    return f.isFloat ? L"32-bit float" : (f.bits == 16 ? L"16-bit" : (f.bits == 24 ? L"24-bit" : L"32-bit"));
}

int testTone(const std::wstring& which)
{
    DeviceEnumerator devices;
    const auto list = devices.renderDevices();
    std::wstring id = which;
    if (!which.empty() && which.size() < 4 && iswdigit(which[0])) {
        const size_t index = std::stoul(which);
        if (index >= list.size()) {
            std::wprintf(L"No device [%zu]. Run --list-devices.\n", index);
            return 1;
        }
        id = list[index].id;
    }

    WasapiExclusiveOutput output(devices.native());
    for (int frames : {32, 64, 128, 256}) {
        ToneRenderer tone(48000.0);
        const OpenResult r = output.open(id, frames, &tone);
        if (!r.ok()) {
            std::wprintf(L"%3d samples: %ls\n", frames, r.message.c_str());
            continue;
        }
        const OutputFormat& f = output.format();
        tone = ToneRenderer(f.sampleRate);
        std::wprintf(L"%3d samples: %.1f kHz, %ls, %d ch, buffer %d (%.2f ms) - playing 3 s\n", frames,
                     f.sampleRate / 1000.0, formatName(f), f.channels, f.bufferFrames,
                     1000.0 * f.bufferFrames / f.sampleRate);
        output.start();
        std::this_thread::sleep_for(std::chrono::seconds(3));
        output.close();
        if (output.failed()) {
            std::wprintf(L"     the device failed while playing\n");
        }
    }
    return 0;
}

// Resolves "default", a list index or an endpoint ID to an endpoint ID.
std::wstring resolveDevice(const DeviceEnumerator& devices, const std::wstring& which)
{
    if (which == L"default") {
        return devices.defaultRenderId();
    }
    if (!which.empty() && which.size() < 4 && iswdigit(which[0])) {
        const auto list = devices.renderDevices();
        const size_t index = std::stoul(which);
        return index < list.size() ? list[index].id : std::wstring();
    }
    return which;
}

int listApps()
{
    DeviceEnumerator devices;
    SessionWatcher sessions(devices.native());
    const auto apps = sessions.list();
    std::wprintf(L"%zu app(s) with audio sessions:\n", apps.size());
    for (const auto& a : apps) {
        std::wprintf(L"  %ls \x00b7 %ls  (pid %u)%ls\n", a.name.c_str(), a.exe.c_str(), a.processId,
                     a.playing ? L"" : L"  silent");
    }
    return 0;
}

// Prints the peak level of the captured mono signal four times a second.
int captureLevels(bool app, const std::wstring& which, int seconds)
{
    DeviceEnumerator devices;
    SpscRing ring(1 << 16);
    WasapiCapture capture(devices.native());
    OpenResult r;
    if (app) {
        const uint32_t pid = SessionWatcher(devices.native()).findProcess(which);
        if (pid == 0) {
            std::wprintf(L"%ls has no audio session. Start it and play something.\n", which.c_str());
            return 1;
        }
        std::wprintf(L"Capturing %ls (pid %u)\n", which.c_str(), pid);
        r = capture.openProcess(pid, &ring);
    } else {
        const std::wstring id = resolveDevice(devices, which);
        AudioDevice info;
        devices.find(id, info);
        std::wprintf(L"Capturing %ls\n", info.name.c_str());
        r = capture.openLoopback(id, &ring);
    }
    if (!r.ok()) {
        std::wprintf(L"%ls\n", r.message.c_str());
        return 1;
    }
    const CaptureFormat& f = capture.format();
    std::wprintf(L"%.1f kHz, %d-bit%ls, %d ch, packets of %zu frames\n", f.sampleRate / 1000.0, f.bits,
                 f.isFloat ? L" float" : L"", f.channels, f.packetFrames);
    capture.start();

    std::vector<float> block(4096);
    for (int tick = 0; tick < seconds * 4; ++tick) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        float peak = 0.0f;
        size_t n;
        while ((n = ring.read(block.data(), block.size())) > 0) {
            for (size_t i = 0; i < n; ++i) {
                peak = std::max(peak, std::abs(block[i]));
            }
        }
        const CaptureSignals s = capture.signals();
        if (s.idle) {
            std::wprintf(L"  idle (nothing playing)\n");
        } else {
            std::wprintf(L"  peak %6.1f dBFS   discontinuities %u  overruns %u\n",
                         peak > 0.0f ? 20.0 * std::log10(peak) : -999.0, s.discontinuities, s.overruns);
        }
        if (capture.failed()) {
            std::wprintf(L"  the source went away\n");
            break;
        }
    }
    capture.close();
    return 0;
}

}  // namespace

int wmain(int argc, wchar_t** argv)
{
    const auto com = wil::CoInitializeEx(COINIT_MULTITHREADED);
    std::vector<std::wstring> args(argv + 1, argv + argc);
    auto has = [&](const wchar_t* flag) { return std::find(args.begin(), args.end(), flag) != args.end(); };

    if (has(L"--list-devices")) {
        return listDevices(has(L"--watch"));
    }
    if (args.size() >= 2 && args[0] == L"--test-tone") {
        return testTone(args[1]);
    }
    if (has(L"--list-apps")) {
        return listApps();
    }
    if (args.size() >= 2 && (args[0] == L"--capture" || args[0] == L"--capture-app")) {
        const int seconds = args.size() >= 3 ? std::stoi(args[2]) : 10;
        return captureLevels(args[0] == L"--capture-app", args[1], seconds);
    }
    std::wprintf(L"gfxprobe --list-devices [--watch]\n"
                 L"gfxprobe --test-tone <index|id>\n"
                 L"gfxprobe --list-apps\n"
                 L"gfxprobe --capture <default|index|id> [seconds]\n"
                 L"gfxprobe --capture-app <exe> [seconds]\n");
    return 1;
}

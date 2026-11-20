// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

// gfxprobe: a console tool for checking the audio layers by hand.
//
//   gfxprobe --list-devices          list render devices
//   gfxprobe --list-devices --watch  ...and print changes until Enter
//   gfxprobe --test-tone <n|id>      40 Hz on outputs 1-2 at each buffer size

#include "capture/DeviceEnumerator.h"
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
    std::wprintf(L"gfxprobe --list-devices [--watch]\n"
                 L"gfxprobe --test-tone <index|id>\n");
    return 1;
}

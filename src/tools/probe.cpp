// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

// gfxprobe: a console tool for checking the audio layers by hand.
//
//   gfxprobe --list-devices          list render devices
//   gfxprobe --list-devices --watch  ...and print changes until Enter
//   gfxprobe --test-tone <n|id>      40 Hz on outputs 1-2 at each buffer size
//   gfxprobe --list-apps             apps with audio sessions
//   gfxprobe --list-asio             ASIO drivers and their formats (loads each
//                                    one briefly; plays nothing)
//   gfxprobe --hold-asio <n> [seconds]  keep driver n loaded (like another app
//                                    using it) without playing; default 30 s
//   gfxprobe --capture <default|n|id> [seconds]   System mix capture levels
//   gfxprobe --capture-app <exe> [seconds]        App capture levels
//   gfxprobe --run <shaker n|id> [--asio] [--app <exe>] [--buffer <samples>] [seconds]
//                                    run the engine (System mix on the Windows
//                                    default, or one app) and print telemetry
//                                    (buffer default 64; with --asio the shaker
//                                    is a driver index from --list-asio)

#include "capture/DeviceEnumerator.h"
#include "capture/SessionWatcher.h"
#include "capture/WasapiCapture.h"
#include "engine/Engine.h"
#include "output/Asio.h"
#include "output/WasapiExclusiveOutput.h"

#include <windows.h>
#include <objbase.h>
#include <wil/com.h>

#include <algorithm>
#include <atomic>
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

// A quiet 40 Hz sine (-12 dBFS) with 30 ms S-curve fades in and out, like
// the app's test signals, so starting and stopping it doesn't click.
class ToneRenderer final : public Renderer {
public:
    void setRate(double rate)
    {
        step_ = 2.0 * M_PI * 40.0 / rate;
        fadeStep_ = 1.0 / (0.030 * rate);
    }
    void fadeOut() { stopping_.store(true); }
    void holdSilent(double samples) { silent_ = static_cast<long long>(samples); }
    void render(float* mono, size_t frames) override
    {
        const bool stopping = stopping_.load();
        for (size_t i = 0; i < frames; ++i) {
            if (silent_ > 0) {
                --silent_;
                mono[i] = 0.0f;
                continue;
            }
            fade_ = stopping ? std::max(0.0, fade_ - fadeStep_) : std::min(1.0, fade_ + fadeStep_);
            const double envelope = 0.5 - 0.5 * std::cos(M_PI * fade_);
            mono[i] = static_cast<float>(0.25 * envelope * std::sin(phase_));
            phase_ = std::fmod(phase_ + step_, 2.0 * M_PI);
        }
    }

private:
    double step_ = 0.0;
    double fadeStep_ = 0.0;
    double phase_ = 0.0;
    double fade_ = 0.0;
    long long silent_ = 0;  // samples of silence before the fade-in
    std::atomic<bool> stopping_{false};
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
        ToneRenderer tone;
        tone.setRate(48000.0);
        const OpenResult r = output.open(id, frames, &tone);
        if (!r.ok()) {
            std::wprintf(L"%3d samples: %ls\n", frames, r.message.c_str());
            continue;
        }
        const OutputFormat& f = output.format();
        tone.setRate(f.sampleRate);
        std::wprintf(L"%3d samples: %.1f kHz, %ls, %d ch, buffer %d (%.2f ms) - playing 3 s\n", frames,
                     f.sampleRate / 1000.0, formatName(f), f.channels, f.bufferFrames,
                     1000.0 * f.bufferFrames / f.sampleRate);
        tone.holdSilent(f.sampleRate * 0.5);  // half a second of silence first: tells a DAC's own start-up pop from the tone
        output.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(3500));
        tone.fadeOut();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));  // the fade plus a buffer or two
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

const wchar_t* phaseName(Phase p)
{
    switch (p) {
    case Phase::Stopped: return L"STOPPED";
    case Phase::Starting: return L"STARTING";
    case Phase::Running: return L"RUNNING";
    case Phase::Waiting: return L"WAITING";
    case Phase::Switching: return L"SWITCHING";
    case Phase::Error: return L"ERROR";
    }
    return L"?";
}

// Runs the real engine and prints state changes and telemetry.
int runEngine(const std::wstring& shaker, const std::wstring& appExe, bool asio, int buffer, int seconds)
{
    std::wstring shakerId, shakerName;
    if (asio) {
        // the shaker is an ASIO driver, by its index in --list-asio
        const auto drivers = listAsioDrivers();
        const size_t index = std::stoul(shaker);
        if (index >= drivers.size()) {
            std::wprintf(L"ASIO driver not found. Run --list-asio.\n");
            return 1;
        }
        shakerId = drivers[index].id;
        shakerName = drivers[index].name;
    } else {
        DeviceEnumerator devices;
        shakerId = resolveDevice(devices, shaker);
        AudioDevice info;
        if (!devices.find(shakerId, info)) {
            std::wprintf(L"Shaker device not found. Run --list-devices.\n");
            return 1;
        }
        shakerName = info.name;
    }

    Engine engine;
    engine.setStateListener([](const EngineState& s) {
        std::wprintf(L"[state] %ls%ls%ls%ls\n", s.checkOutput ? L"CHECK OUTPUT" : phaseName(s.phase),
                     s.message.empty() ? L"" : L" - ", s.message.c_str(),
                     s.testMessage.empty() ? L"" : (L" (test: " + s.testMessage + L")").c_str());
    });

    EngineSettings settings;
    settings.shakerDeviceId = shakerId;
    settings.shakerDeviceName = shakerName;
    if (asio) {
        settings.driver = OutputDriver::Asio;
    }
    if (buffer > 0) {
        settings.bufferFrames = buffer;
    }
    if (!appExe.empty()) {
        settings.sourceMode = EngineSettings::SourceMode::App;
        settings.appExe = appExe;
    }
    engine.apply(settings);
    engine.start();

    for (int tick = 0; tick < seconds * 2; ++tick) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        const Telemetry t = engine.telemetry();
        std::wprintf(L"  in %6.1f  out %6.1f dBFS%ls  latency %5.1f ms  drift %+7.1f ppm  fill %3.0f%%  %ls  "
                     L"reserve %.0f ms  dropouts %u (under %u, over %u, gap %u, resync %u)%ls\n",
                     t.inDb, t.outDb, t.noSignal ? L" (no signal)" : L"", t.latencyMs, t.driftPpm, t.fill * 100.0,
                     t.locked ? L"locked  " : L"settling", t.reserveMs, t.dropouts, t.underruns, t.overruns,
                     t.discontinuities, t.outputResyncs, t.limiterHit ? L"  LIMIT" : L"");
    }
    engine.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return 0;
}

}  // namespace

int listAsio()
{
    if (!asioSupported()) {
        std::wprintf(L"This build has no ASIO (GFX_WITH_ASIO is off).\n");
        return 1;
    }
    const auto drivers = listAsioDrivers();
    std::wprintf(L"%zu ASIO driver(s):\n", drivers.size());
    for (size_t i = 0; i < drivers.size(); ++i) {
        OutputFormat f;
        if (describeAsioDriver(drivers[i].id, f)) {
            std::wprintf(L"  [%zu] %ls\n       %.0f Hz, %d-bit%ls, %d outputs, id %ls\n", i, drivers[i].name.c_str(), f.sampleRate,
                         f.bits, f.isFloat ? L" float" : L"", f.channels, drivers[i].id.c_str());
        } else {
            std::wprintf(L"  [%zu] %ls\n       couldn't be read (in use by another app?), id %ls\n", i, drivers[i].name.c_str(),
                         drivers[i].id.c_str());
        }
    }
    return 0;
}

int holdAsio(size_t index, int seconds)
{
    const auto drivers = listAsioDrivers();
    if (index >= drivers.size()) {
        std::wprintf(L"No ASIO driver [%zu]. Run --list-asio.\n", index);
        return 1;
    }
    auto output = makeAsioOutput();
    const OpenResult r = output->open(drivers[index].id, 256, nullptr);  // opened, never started: silent
    if (!r.ok()) {
        std::wprintf(L"Couldn't load %ls: %ls\n", drivers[index].name.c_str(), r.message.c_str());
        return 1;
    }
    std::wprintf(L"Holding %ls for %d s (nothing plays)...\n", drivers[index].name.c_str(), seconds);
    Sleep(static_cast<DWORD>(seconds) * 1000);
    output->close();
    std::wprintf(L"Released.\n");
    return 0;
}

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
    if (has(L"--list-asio")) {
        return listAsio();
    }
    if (args.size() >= 2 && args[0] == L"--hold-asio") {
        return holdAsio(std::stoul(args[1]), args.size() >= 3 ? std::stoi(args[2]) : 30);
    }
    if (args.size() >= 2 && (args[0] == L"--capture" || args[0] == L"--capture-app")) {
        const int seconds = args.size() >= 3 ? std::stoi(args[2]) : 10;
        return captureLevels(args[0] == L"--capture-app", args[1], seconds);
    }
    if (args.size() >= 2 && args[0] == L"--run") {
        std::wstring app;
        int seconds = 30;
        int buffer = 0;  // 0: the engine's default
        bool asio = false;
        for (size_t i = 2; i < args.size(); ++i) {
            if (args[i] == L"--app" && i + 1 < args.size()) {
                app = args[++i];
            } else if (args[i] == L"--asio") {
                asio = true;
            } else if (args[i] == L"--buffer" && i + 1 < args.size()) {
                buffer = std::stoi(args[++i]);
            } else {
                seconds = std::stoi(args[i]);
            }
        }
        return runEngine(args[1], app, asio, buffer, seconds);
    }
    std::wprintf(L"gfxprobe --run <shaker index|id> [--asio] [--app <exe>] [--buffer <samples>] [seconds]\n"
                 L"gfxprobe --list-devices [--watch]\n"
                 L"gfxprobe --test-tone <index|id>\n"
                 L"gfxprobe --list-apps\n"
                 L"gfxprobe --list-asio\n"
                 L"gfxprobe --hold-asio <index> [seconds]\n"
                 L"gfxprobe --capture <default|index|id> [seconds]\n"
                 L"gfxprobe --capture-app <exe> [seconds]\n");
    return 1;
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "engine/EngineCore.h"

#include <doctest/doctest.h>

#include <map>
#include <set>
#include <vector>

using namespace gfx;

namespace {

// ---------------------------------------------------------------------------
// Fakes

struct FakeBackend;

class FakeOutput final : public OutputDevice {
public:
    explicit FakeOutput(FakeBackend& b) : backend_(b) {}
    OpenResult open(const std::wstring& id, int bufferFrames, Renderer* renderer) override;
    bool start() override { started = true; return true; }
    void stop() override { started = false; }
    void close() override;
    const OutputFormat& format() const override { return format_; }
    int64_t lastCallbackTime() const override { return lastCallback; }
    bool failed() const override { return failedFlag; }

    void pump();

    FakeBackend& backend_;
    Renderer* renderer = nullptr;
    OutputFormat format_;
    bool started = false;
    bool failedFlag = false;
    int64_t lastCallback = 0;
    std::wstring deviceId;
};

class FakeCapture final : public CaptureDevice {
public:
    explicit FakeCapture(FakeBackend& b) : backend_(b) {}
    OpenResult open(const SourceSpec& s, SpscRing*) override;
    void setTap(SpscRing*) override {}
    bool start() override { return true; }
    void close() override;
    const CaptureFormat& format() const override { return format_; }
    CaptureSignals signals() const override { CaptureSignals c; c.idle = true; return c; }
    bool failed() const override { return failedFlag; }

    FakeBackend& backend_;
    CaptureFormat format_{48000.0, 2, 32, true, 480};
    SourceSpec spec;
    bool failedFlag = false;
};

struct FakeBackend final : Backend {
    std::wstring defaultId = L"speakers";
    std::set<std::wstring> connected{L"speakers", L"headset", L"dac"};
    std::map<std::wstring, uint32_t> processes;  // exe -> pid
    OpenResult nextOutputResult{OpenStatus::Ok, {}};
    OpenResult nextCaptureResult{OpenStatus::Ok, {}};
    int64_t clock = 0;  // microseconds
    bool outputStalled = false;

    FakeOutput* output = nullptr;           // the open output, if any
    std::vector<FakeCapture*> captures;     // open captures
    int outputsOpened = 0;
    int capturesOpened = 0;

    std::wstring defaultRenderId() override { return defaultId; }
    bool isConnected(const std::wstring& id) override { return connected.count(id) > 0; }
    uint32_t findProcess(const std::wstring& exe) override
    {
        auto it = processes.find(exe);
        return it == processes.end() ? 0 : it->second;
    }
    std::unique_ptr<CaptureDevice> makeCapture() override { return std::make_unique<FakeCapture>(*this); }
    std::unique_ptr<OutputDevice> makeOutput(OutputDriver) override { return std::make_unique<FakeOutput>(*this); }
    int64_t now() override { return clock; }
    int64_t ticksPerSecond() override { return 1'000'000; }
    void waitBriefly() override
    {
        clock += 1000;
        if (output && output->started && !outputStalled) {
            output->pump();
        }
    }

    // Lets time pass with the output calling back every ~1.3 ms, ticking the
    // engine every 100 ms like the control thread does.
    void run(EngineCore& engine, double seconds)
    {
        const int64_t end = clock + static_cast<int64_t>(seconds * 1e6);
        int64_t nextTick = clock;
        while (clock < end) {
            clock += 1333;
            if (output && output->started && !outputStalled) {
                output->pump();
            }
            if (clock >= nextTick) {
                engine.tick();
                nextTick = clock + 100'000;
            }
        }
    }
};

OpenResult FakeOutput::open(const std::wstring& id, int bufferFrames, Renderer* r)
{
    if (!backend_.nextOutputResult.ok()) {
        return backend_.nextOutputResult;
    }
    renderer = r;
    deviceId = id;
    format_ = OutputFormat{48000.0, SampleFormat::Float32, 32, true, 2, bufferFrames};
    backend_.output = this;
    ++backend_.outputsOpened;
    lastCallback = backend_.clock;
    return {OpenStatus::Ok, {}};
}

void FakeOutput::close()
{
    started = false;
    if (backend_.output == this) {
        backend_.output = nullptr;
    }
}

void FakeOutput::pump()
{
    std::vector<float> block(static_cast<size_t>(format_.bufferFrames));
    renderer->render(block.data(), block.size());
    lastCallback = backend_.clock;
}

OpenResult FakeCapture::open(const SourceSpec& s, SpscRing*)
{
    if (!backend_.nextCaptureResult.ok()) {
        return backend_.nextCaptureResult;
    }
    spec = s;
    backend_.captures.push_back(this);
    ++backend_.capturesOpened;
    return {OpenStatus::Ok, {}};
}

void FakeCapture::close()
{
    std::erase(backend_.captures, this);
}

// ---------------------------------------------------------------------------
// Helpers

EngineSettings systemMix()
{
    EngineSettings s;
    s.sourceMode = EngineSettings::SourceMode::SystemMix;
    s.captureFollowsDefault = true;
    s.shakerDeviceId = L"dac";
    s.shakerDeviceName = L"USB DAC";
    return s;
}

EngineSettings appMode(const std::wstring& exe)
{
    EngineSettings s = systemMix();
    s.sourceMode = EngineSettings::SourceMode::App;
    s.appExe = exe;
    return s;
}

struct Rig {
    FakeBackend backend;
    EngineCore engine{backend};
    std::vector<Phase> phases;

    explicit Rig(const EngineSettings& s)
    {
        engine.setStateListener([this](const EngineState& st) {
            if (phases.empty() || phases.back() != st.phase) {
                phases.push_back(st.phase);
            }
        });
        engine.apply(s);
    }
    Phase phase() const { return engine.state().phase; }
};

}  // namespace

// ---------------------------------------------------------------------------

TEST_CASE("Start goes Starting -> Running; Stop releases everything")
{
    Rig rig(systemMix());
    rig.engine.start();
    CHECK(rig.phases == std::vector<Phase>{Phase::Starting, Phase::Running});
    CHECK(rig.backend.output != nullptr);
    CHECK(rig.backend.captures.size() == 1);
    CHECK(rig.backend.captures[0]->spec.deviceId == L"speakers");

    rig.backend.run(rig.engine, 1.0);
    CHECK(rig.phase() == Phase::Running);

    rig.engine.stop();
    CHECK(rig.phase() == Phase::Stopped);
    CHECK(rig.backend.output == nullptr);
    CHECK(rig.backend.captures.empty());
}

TEST_CASE("Missing shaker device: Waiting, then Running when it's plugged in")
{
    Rig rig(systemMix());
    rig.backend.connected.erase(L"dac");
    rig.engine.start();
    CHECK(rig.phase() == Phase::Waiting);
    CHECK(rig.engine.state().messagePanel == Panel::Shaker);
    CHECK_FALSE(rig.engine.state().messageIsError);
    CHECK(rig.engine.state().message == L"Not connected. Output starts when it's back.");

    rig.backend.connected.insert(L"dac");
    rig.engine.onDeviceEvent(DeviceEvent::Added, L"dac");
    CHECK(rig.phase() == Phase::Running);
    CHECK(rig.engine.state().message.empty());
}

TEST_CASE("App not running: Waiting with the spec message, Running once it starts")
{
    Rig rig(appMode(L"RallyGame.exe"));
    rig.engine.start();
    CHECK(rig.phase() == Phase::Waiting);
    CHECK(rig.engine.state().messagePanel == Panel::Source);
    CHECK(rig.engine.state().message == L"Not running. Capture starts when it opens.");
    CHECK(rig.backend.output != nullptr);  // the output already streams silence

    rig.backend.processes[L"RallyGame.exe"] = 4242;
    rig.backend.run(rig.engine, 2.5);  // found by the 2 s poll
    CHECK(rig.phase() == Phase::Running);
    CHECK(rig.backend.captures.size() == 1);
    CHECK(rig.backend.captures[0]->spec.processId == 4242);
}

TEST_CASE("App closed while Running -> Waiting; relaunched with a new PID -> Running")
{
    Rig rig(appMode(L"RaceSim.exe"));
    rig.backend.processes[L"RaceSim.exe"] = 100;
    rig.engine.start();
    REQUIRE(rig.phase() == Phase::Running);

    rig.backend.processes.clear();
    rig.backend.run(rig.engine, 2.5);
    CHECK(rig.phase() == Phase::Waiting);
    CHECK(rig.backend.captures.empty());

    rig.backend.processes[L"RaceSim.exe"] = 200;
    rig.backend.run(rig.engine, 2.5);
    CHECK(rig.phase() == Phase::Running);
    REQUIRE(rig.backend.captures.size() == 1);
    CHECK(rig.backend.captures[0]->spec.processId == 200);
}

TEST_CASE("App restarting quickly is followed to its new process")
{
    Rig rig(appMode(L"RaceSim.exe"));
    rig.backend.processes[L"RaceSim.exe"] = 100;
    rig.engine.start();
    rig.backend.processes[L"RaceSim.exe"] = 300;
    rig.backend.run(rig.engine, 2.5);
    CHECK(rig.phase() == Phase::Running);
    REQUIRE(rig.backend.captures.size() == 1);
    CHECK(rig.backend.captures[0]->spec.processId == 300);
}

TEST_CASE("Fixed capture device unplugged -> Waiting; replugged -> Running")
{
    EngineSettings s = systemMix();
    s.captureFollowsDefault = false;
    s.captureDeviceId = L"headset";
    s.captureDeviceName = L"Headset";
    Rig rig(s);
    rig.engine.start();
    REQUIRE(rig.phase() == Phase::Running);

    rig.backend.connected.erase(L"headset");
    rig.backend.captures[0]->failedFlag = true;
    rig.backend.run(rig.engine, 0.3);
    CHECK(rig.phase() == Phase::Waiting);
    CHECK(rig.engine.state().message == L"Not connected. Capture resumes when it's back.");

    rig.backend.connected.insert(L"headset");
    rig.engine.onDeviceEvent(DeviceEvent::Added, L"headset");
    CHECK(rig.phase() == Phase::Running);
}

TEST_CASE("Shaker in use -> Error with a message; recovers within 3 s once released")
{
    Rig rig(systemMix());
    rig.backend.nextOutputResult = {OpenStatus::InUse, L"In use by another app. Close it or pick another device."};
    rig.engine.start();
    CHECK(rig.phase() == Phase::Error);
    CHECK(rig.engine.state().messagePanel == Panel::Shaker);
    CHECK(rig.engine.state().messageIsError);

    rig.backend.run(rig.engine, 2.0);
    CHECK(rig.phase() == Phase::Error);  // still held by the other app

    rig.backend.nextOutputResult = {OpenStatus::Ok, {}};
    rig.backend.run(rig.engine, 3.0);
    CHECK(rig.phase() == Phase::Running);
}

TEST_CASE("Output stall over 500 ms -> Error, then the output restarts")
{
    Rig rig(systemMix());
    rig.engine.start();
    rig.backend.run(rig.engine, 0.5);
    rig.backend.outputStalled = true;
    rig.backend.run(rig.engine, 0.8);
    CHECK(rig.phase() == Phase::Error);
    CHECK(rig.engine.state().message == L"Stopped responding.");

    rig.backend.outputStalled = false;
    rig.backend.run(rig.engine, 3.5);
    CHECK(rig.phase() == Phase::Running);
    CHECK(rig.backend.outputsOpened == 2);
}

TEST_CASE("Feedback: System mix capturing the shaker device blocks Start")
{
    EngineSettings s = systemMix();
    s.shakerDeviceId = L"speakers";  // the Windows default
    Rig rig(s);
    CHECK(rig.engine.state().checkOutput);
    rig.engine.start();
    CHECK(rig.phase() == Phase::Stopped);
    CHECK(rig.backend.output == nullptr);
}

TEST_CASE("Feedback: App mode never conflicts")
{
    EngineSettings s = appMode(L"Game.exe");
    s.shakerDeviceId = L"speakers";
    Rig rig(s);
    rig.backend.processes[L"Game.exe"] = 7;
    CHECK_FALSE(rig.engine.state().checkOutput);
    rig.engine.start();
    CHECK(rig.phase() == Phase::Running);
}

TEST_CASE("Feedback arising while running (default moves to the shaker) stops the engine")
{
    Rig rig(systemMix());
    rig.engine.start();
    REQUIRE(rig.phase() == Phase::Running);
    rig.backend.defaultId = L"dac";
    rig.engine.onDeviceEvent(DeviceEvent::DefaultChanged, L"dac");
    CHECK(rig.phase() == Phase::Stopped);
    CHECK(rig.engine.state().checkOutput);
}

TEST_CASE("Windows default change moves System mix capture (Switching -> Running)")
{
    Rig rig(systemMix());
    rig.engine.start();
    rig.backend.run(rig.engine, 0.2);
    rig.phases.clear();
    rig.backend.defaultId = L"headset";
    rig.engine.onDeviceEvent(DeviceEvent::DefaultChanged, L"headset");
    CHECK(rig.phases == std::vector<Phase>{Phase::Switching, Phase::Running});
    REQUIRE(rig.backend.captures.size() == 1);
    CHECK(rig.backend.captures[0]->spec.deviceId == L"headset");
}

TEST_CASE("A fixed capture device ignores Windows default changes")
{
    EngineSettings s = systemMix();
    s.captureFollowsDefault = false;
    s.captureDeviceId = L"speakers";
    Rig rig(s);
    rig.engine.start();
    rig.backend.defaultId = L"headset";
    rig.engine.onDeviceEvent(DeviceEvent::DefaultChanged, L"headset");
    CHECK(rig.phase() == Phase::Running);
    CHECK(rig.backend.capturesOpened == 1);
}

TEST_CASE("Changing the shaker device while running: Switching, old released, new opened")
{
    Rig rig(systemMix());
    rig.engine.start();
    rig.phases.clear();
    EngineSettings s = systemMix();
    s.shakerDeviceId = L"headset";
    rig.engine.apply(s);
    CHECK(rig.phases == std::vector<Phase>{Phase::Switching, Phase::Running});
    REQUIRE(rig.backend.output != nullptr);
    CHECK(rig.backend.output->deviceId == L"headset");
    CHECK(rig.backend.outputsOpened == 2);
}

TEST_CASE("Filter changes apply without switching")
{
    Rig rig(systemMix());
    rig.engine.start();
    rig.phases.clear();
    EngineSettings s = systemMix();
    s.filter.cutoffHz = 120.0;
    s.limiter = false;
    rig.engine.apply(s);
    CHECK(rig.phases.empty());
    CHECK(rig.backend.outputsOpened == 1);
    CHECK(rig.backend.capturesOpened == 1);
}

TEST_CASE("Pulse while stopped opens only the output and keeps the status Stopped")
{
    Rig rig(systemMix());
    rig.engine.pulseStart();
    CHECK(rig.phase() == Phase::Stopped);
    CHECK(rig.engine.state().pulsing);
    CHECK(rig.backend.output != nullptr);
    CHECK(rig.backend.captures.empty());
    CHECK(rig.engine.telemetry().test);

    rig.backend.run(rig.engine, 1.0);
    rig.engine.pulseStop();
    rig.backend.run(rig.engine, 0.3);  // fades out, then the device is released
    CHECK(rig.backend.output == nullptr);
    CHECK(rig.phase() == Phase::Stopped);
    CHECK_FALSE(rig.engine.state().pulsing);
}

TEST_CASE("Sweep while stopped with the shaker unplugged shows a message instead of Error")
{
    Rig rig(systemMix());
    rig.backend.connected.erase(L"dac");
    rig.engine.sweep();
    CHECK(rig.phase() == Phase::Stopped);
    CHECK(rig.engine.state().testMessage == L"Not connected.");
    rig.backend.run(rig.engine, 4.5);
    CHECK(rig.engine.state().testMessage.empty());
}

TEST_CASE("Test signals are unavailable while in Error")
{
    Rig rig(systemMix());
    rig.backend.nextOutputResult = {OpenStatus::InUse, L"in use"};
    rig.engine.start();
    REQUIRE(rig.phase() == Phase::Error);
    rig.engine.pulseStart();
    CHECK_FALSE(rig.engine.state().pulsing);
}

TEST_CASE("Errors that retry are flagged so the UI can say so")
{
    Rig rig(systemMix());
    rig.backend.nextOutputResult = {OpenStatus::InUse, L"In use by another app. Close it or pick another device."};
    rig.engine.start();
    CHECK(rig.phase() == Phase::Error);
    CHECK(rig.engine.state().retrying);

    rig.backend.nextOutputResult = {OpenStatus::Ok, {}};
    rig.backend.run(rig.engine, 3.5);
    CHECK(rig.phase() == Phase::Running);
    CHECK_FALSE(rig.engine.state().retrying);
}

TEST_CASE("Per-app capture on old Windows: an error that doesn't retry")
{
    Rig rig(appMode(L"Game.exe"));
    rig.backend.processes[L"Game.exe"] = 9;
    rig.backend.nextCaptureResult = {OpenStatus::Unsupported, L"Needs Windows 10 2004 or later. Use System mix."};
    rig.engine.start();
    CHECK(rig.phase() == Phase::Error);
    CHECK(rig.engine.state().messagePanel == Panel::Source);
    CHECK_FALSE(rig.engine.state().retrying);
    const int opened = rig.backend.outputsOpened;
    rig.backend.run(rig.engine, 10.0);
    CHECK(rig.backend.outputsOpened == opened);  // no retries
}

TEST_CASE("App capture errors go to the source selector")
{
    Rig rig(appMode(L"Game.exe"));
    rig.backend.processes[L"Game.exe"] = 9;
    rig.backend.nextCaptureResult = {OpenStatus::Failed, L"Can't capture this app. Restart it or pick another."};
    rig.engine.start();
    CHECK(rig.engine.state().message == L"Can't capture this app. Restart it or pick another.");
    CHECK(rig.engine.state().messagePanel == Panel::Source);
    CHECK(rig.engine.state().retrying);
}

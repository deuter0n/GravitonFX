// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include "capture/DeviceEvent.h"
#include "engine/Backend.h"
#include "engine/EngineTypes.h"
#include "engine/RenderPath.h"

#include <array>
#include <functional>
#include <memory>

namespace gfx {

// The engine's state machine. Single-threaded: the control thread (or a test)
// calls every method; only RenderPath::render runs elsewhere.
//
//   Stopped -> Starting -> Running            (all good)
//                       -> Waiting            (app, capture device or shaker missing)
//                       -> Error              (in use, format, buffer, stall, unsupported)
//   Running <-> Switching                     (source or output changed while active)
//   Waiting -> Running   on device/session events and a 2 s poll
//   Error   -> retry     every 3 s and on device events
//   Feedback (System mix captures the shaker device) -> Stopped + checkOutput
class EngineCore {
public:
    using StateListener = std::function<void(const EngineState&)>;

    explicit EngineCore(Backend& backend);
    ~EngineCore();

    void setStateListener(StateListener listener) { listener_ = std::move(listener); }
    void setTaps(SpscRing* input, SpscRing* output) { render_.setTaps(input, output); inTap_ = input; }

    // Commands.
    void apply(const EngineSettings& settings);
    void start();
    void stop();
    void pulseStart();
    void pulseStop();
    void sweep();  // starts, or stops a running sweep

    // Events.
    void onDeviceEvent(DeviceEvent event, const std::wstring& deviceId);

    // Called every 100 ms: watchdog, failure checks, retries, polling, tests.
    void tick();

    const EngineState& state() const { return state_; }
    Telemetry telemetry() const;
    const EngineSettings& settings() const { return settings_; }

private:
    enum class Missing { None, App, CaptureDevice, Shaker };

    void tryActivate();
    bool openOutput(bool forTest);
    void closeOutput();
    // Opens the configured source into the free slot and swaps it in.
    // Returns false if it's missing or failed (state already updated).
    bool openSource();
    void switchSource();  // Switching -> open the new source -> Running
    void closeSources();
    void enterWaiting(Missing what);
    void enterError(Panel panel, const std::wstring& message, bool retry = true);
    void setPhase(Phase phase);
    void publish();
    bool feedbackConflict() const;
    std::wstring resolvedCaptureId() const;
    double seconds() const;
    void showTestMessage(const std::wstring& message);

    Backend& backend_;
    EngineSettings settings_;
    EngineState state_;
    EngineState lastPublished_;
    StateListener listener_;

    bool active_ = false;    // the user wants the engine running
    bool testOnly_ = false;  // output open just for a test while stopped
    double testIdleSince_ = -1.0;  // when that test ended (-1: a test is playing)
    RenderPath render_;
    std::unique_ptr<OutputDevice> output_;
    bool outputOpen_ = false;

    std::array<SourceSlot, 2> slots_;
    std::array<std::unique_ptr<CaptureDevice>, 2> captures_;
    int activeSlot_ = -1;  // slot the render path plays, -1 = none
    std::wstring capturedId_;   // what the active slot captures (device ID or exe)
    uint32_t capturedPid_ = 0;
    SpscRing* inTap_ = nullptr;

    double retryAt_ = 0.0;      // Error: next retry time
    double nextPoll_ = 0.0;     // Waiting: next session poll
    double testMessageUntil_ = 0.0;
    Missing missing_ = Missing::None;
};

}  // namespace gfx

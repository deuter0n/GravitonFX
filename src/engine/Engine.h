// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include "capture/DeviceEnumerator.h"
#include "capture/SessionWatcher.h"
#include "engine/EngineTypes.h"

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace gfx {

// The audio engine as the rest of the app sees it. Owns the control thread,
// which owns every device; all methods here are thread-safe and return
// immediately. State changes arrive through the listener (called on the
// control thread); telemetry is a snapshot refreshed ~30 times a second.
class Engine {
public:
    using StateListener = std::function<void(const EngineState&)>;
    using DevicesListener = std::function<void()>;  // device list or sessions changed

    Engine();
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void setStateListener(StateListener listener);
    void setDevicesListener(DevicesListener listener);

    void apply(const EngineSettings& settings);
    void start();
    void stop();
    void pulseStart();
    void pulseStop();
    void sweep();

    EngineState state() const;
    Telemetry telemetry() const;

    // Snapshots for the device pickers (answered on the control thread).
    std::vector<AudioDevice> renderDevices();
    std::vector<AudioApp> audioApps();

private:
    using Command = std::function<void()>;
    void post(Command command);
    template <class T> T ask(std::function<T()> query);
    void run();

    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Command> queue_;
    bool quit_ = false;
    EngineState state_;
    Telemetry telemetry_;
    StateListener stateListener_;
    DevicesListener devicesListener_;

    // Set on the control thread while it runs.
    class EngineCore* core_ = nullptr;
    class WindowsBackend* backend_ = nullptr;

    std::thread thread_;
};

}  // namespace gfx

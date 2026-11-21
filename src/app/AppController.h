// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include "app/MainWindow.h"
#include "engine/Engine.h"
#include "host/Settings.h"

#include <nlohmann/json_fwd.hpp>
#include <windows.h>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace gfx {

// Window messages the controller uses to hop from other threads to the UI thread.
constexpr UINT WM_APP_ENGINE_STATE = WM_APP + 1;
constexpr UINT WM_APP_DEVICES = WM_APP + 2;

// The host side of the dashboard, on the UI thread: owns the settings and the
// engine, turns UI commands into settings changes (validated, saved 300 ms
// later and applied to the engine), and sends the UI its `state`, `devices`
// and `telemetry` messages.
class AppController {
public:
    AppController(MainWindow& window, const std::filesystem::path& settingsFile);
    ~AppController();

    void onPageReady();
    void onUiMessage(const std::string& json);
    void onEngineState();    // WM_APP_ENGINE_STATE
    void onDevicesChanged(); // WM_APP_DEVICES
    void onTimer();          // every 33 ms: telemetry and the save debounce

    Settings& settings() { return settings_; }
    Engine& engine() { return engine_; }
    void saveNow();

    // Window commands from the UI's own title bar (tray and quit come later).
    std::function<void()> onMinimize;
    std::function<void()> onClose;

private:
    void set(const std::string& key, const nlohmann::json& value);
    void changed(bool engineToo = true);
    void applyToEngine();
    void refreshDevices();
    void chooseShakerIfUnset();
    void postState();
    void postDevices();
    void postTelemetry();
    std::string deviceName(const std::string& id) const;
    double seconds() const;
    bool hidden() const;

    MainWindow& window_;
    std::filesystem::path settingsFile_;
    Settings settings_;
    SaveDebouncer saver_;
    Engine engine_;
    std::vector<AudioDevice> endpoints_;
    std::vector<AudioApp> apps_;
    std::wstring defaultId_;
    bool pageReady_ = false;
    bool stale_ = false;  // state or devices changed while hidden
};

}  // namespace gfx

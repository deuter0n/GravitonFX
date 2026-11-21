// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include "app/AppMessages.h"
#include "app/MainWindow.h"
#include "app/Tray.h"
#include "app/UpdateChecker.h"
#include "engine/Engine.h"
#include "host/Settings.h"
#include "output/Asio.h"

#include <nlohmann/json_fwd.hpp>
#include <windows.h>

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace gfx {

// The host side of the dashboard, on the UI thread: owns the settings, the
// engine and the tray icon, turns UI commands into settings changes
// (validated, saved 300 ms later and applied to the engine), and sends the UI
// its `state`, `devices` and `telemetry` messages.
class AppController {
public:
    AppController(MainWindow& window, const std::filesystem::path& settingsFile);
    ~AppController();

    void onPageReady();
    void onUiMessage(const std::string& json);
    void onEngineState();    // WM_APP_ENGINE_STATE
    void onDevicesChanged(); // WM_APP_DEVICES
    void onTimer();          // every 33 ms: telemetry and the save debounce
    void onTrayEvent(LPARAM lParam);  // WM_APP_TRAY
    void onUpdateResult();            // WM_APP_UPDATE

    void showWindow();       // from the tray or a second launch
    void hideToTray();       // minimize
    void closeRequested();   // the close button or Alt+F4: hide or quit, per "Close button hides to tray"

    Settings& settings() { return settings_; }
    Engine& engine() { return engine_; }
    void saveNow();

    Tray& tray() { return tray_; }

    // Quit the app (the caller destroys the window; this object then saves
    // and stops the engine when it's destroyed).
    std::function<void()> onQuit;

private:
    void handleCommand(const nlohmann::json& m);
    void set(const std::string& key, const nlohmann::json& value);
    void changed(bool engineToo = true);
    void applyToEngine();
    void refreshDevices();
    void chooseShakerIfUnset();
    void postState();
    void postDevices();
    void postTelemetry();
    void updateTray();
    void checkForUpdates(bool manual);
    void openReleasePage();
    void syncStartWithWindows();
    std::string deviceName(const std::string& id) const;
    bool asioPresent(const std::wstring& id) const;
    OutputFormat asioFormat(const std::wstring& id);  // cached; loads the driver once to read it
    double seconds() const;
    bool hidden() const;

    MainWindow& window_;
    Tray tray_;
    UpdateChecker updates_;
    std::string updateStatus_ = "idle";  // idle, checking, uptodate, available, error
    std::string updateLatest_;
    bool manualCheck_ = false;
    double nextAutoCheck_ = 0;           // seconds() of the next automatic check
    std::filesystem::path settingsFile_;
    Settings settings_;
    SaveDebouncer saver_;
    Engine engine_;
    std::vector<AudioDevice> endpoints_;
    std::vector<AudioApp> apps_;
    std::vector<AsioDriver> asioDrivers_;
    std::map<std::wstring, OutputFormat> asioFormats_;  // channels 0: couldn't be read
    std::wstring defaultId_;
    bool pageReady_ = false;
    int startWithWindows_ = -1;  // what the Run value was last set to (-1: not yet)
    bool stale_ = false;     // state or devices changed while hidden
    bool idleSent_ = false;  // the at-rest telemetry went out; nothing more until something plays
};

}  // namespace gfx

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace gfx {

// A named set of filter values. Only cutoff, slope and gain: switching
// presets never touches devices or hardware protection.
struct Preset {
    std::string id;
    std::string name;
    double cutoff = 80.0;
    int slope = 24;
    double gain = 0.0;
    bool builtIn = false;

    bool operator==(const Preset&) const = default;
};

// Everything saved in settings.json. Key names match the UI's `set` messages.
// Strings are UTF-8.
struct Settings {
    static constexpr int kVersion = 1;

    std::string sourceMode = "system";     // "system" | "process"
    std::string captureDevice = "default"; // "default" (follow Windows) or an endpoint ID
    std::string captureDeviceName;         // shown while the device is missing
    std::string processExe;                // App mode target
    std::string driver = "wasapi";         // "wasapi" | "asio"
    std::string shakerId;
    std::string shakerName;                // shown while the device is missing
    int buffer = 64;                       // 32, 64, 128 or 256

    bool startWithWindows = true;
    bool startMinimized = false;
    bool autoStart = false;                // start the engine on launch
    bool closeToTray = true;
    bool checkUpdates = true;

    bool limiter = true;
    int subsonic = 20;                     // 0 (off), 10, 15, 20, 30
    int subsonicSlope = 12;                // 12 or 24

    double cutoff = 80.0;                  // current filter values
    int slope = 24;
    double gain = 0.0;
    std::vector<Preset> presets{defaultPreset()};
    std::string activePresetId = "default";

    std::string notifiedVersion;           // last update the user was told about

    static Preset defaultPreset() { return {"default", "Default", 80.0, 24, 0.0, true}; }

    bool operator==(const Settings&) const = default;
};

// Reading and writing settings.json.
//  - load: a missing file gives defaults; invalid JSON is kept as
//    settings.json.bak and gives defaults; fields with wrong types or values
//    out of range fall back to their defaults one by one.
//  - save: writes settings.json.tmp, then replaces settings.json, so a crash
//    never leaves a half-written file. Creates the folder if needed.
std::string toJson(const Settings& s);
Settings fromJson(const std::string& text, bool* valid = nullptr);
Settings loadSettings(const std::filesystem::path& file);
bool saveSettings(const std::filesystem::path& file, const Settings& s);

// %APPDATA%\GravitonFX\settings.json
std::filesystem::path defaultSettingsPath();

// Collapses bursts of changes into one save, 300 ms after the last change.
class SaveDebouncer {
public:
    static constexpr double kDelaySeconds = 0.3;
    void changed(double now) { due_ = now + kDelaySeconds; pending_ = true; }
    // True once per burst, when it's time to write.
    bool due(double now)
    {
        if (pending_ && now >= due_) {
            pending_ = false;
            return true;
        }
        return false;
    }
    bool pending() const { return pending_; }

private:
    double due_ = 0.0;
    bool pending_ = false;
};

// ---------------------------------------------------------------------------
// Presets and reset. Each returns an error message for the user, or "" when
// it worked.

const Preset* findPreset(const Settings& s, const std::string& id);
const Preset& activePreset(const Settings& s);
bool presetModified(const Settings& s);  // current values differ from the active preset

void selectPreset(Settings& s, const std::string& id);
void revertPreset(Settings& s);
void savePreset(Settings& s);  // does nothing for Default
std::string createPreset(Settings& s, const std::string& name);
std::string duplicatePreset(Settings& s, const std::string& id);
std::string renamePreset(Settings& s, const std::string& id, const std::string& name);
void deletePreset(Settings& s, const std::string& id);

// Every setting back to its default, Default active; user presets and the
// notified version are kept.
void resetSettings(Settings& s);

}  // namespace gfx

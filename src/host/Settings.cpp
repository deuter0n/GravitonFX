// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "host/Settings.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace gfx {

using nlohmann::json;

namespace {

constexpr size_t kMaxNameLength = 24;

bool oneOf(int v, std::initializer_list<int> allowed)
{
    return std::find(allowed.begin(), allowed.end(), v) != allowed.end();
}

// Reads `key` into `out` if it is present, of the right type and accepted
// by `ok`; otherwise leaves the default and marks the result invalid.
template <class T, class Ok>
void read(const json& j, const char* key, T& out, bool& valid, Ok ok)
{
    const auto it = j.find(key);
    if (it == j.end()) {
        return;  // missing keys simply keep their defaults
    }
    try {
        T v = it->get<T>();
        if (ok(v)) {
            out = v;
            return;
        }
    } catch (const json::exception&) {
    }
    valid = false;
}

template <class T> void read(const json& j, const char* key, T& out, bool& valid)
{
    read(j, key, out, valid, [](const T&) { return true; });
}

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string trim(const std::string& s)
{
    const auto first = s.find_first_not_of(" \t");
    if (first == std::string::npos) {
        return {};
    }
    return s.substr(first, s.find_last_not_of(" \t") - first + 1);
}

// Length in characters, not bytes (names may be non-ASCII).
size_t utf8Length(const std::string& s)
{
    return static_cast<size_t>(std::count_if(s.begin(), s.end(), [](char c) { return (c & 0xC0) != 0x80; }));
}

bool nameTaken(const Settings& s, const std::string& name, const std::string& exceptId = {})
{
    const std::string wanted = lower(name);
    return std::any_of(s.presets.begin(), s.presets.end(),
                       [&](const Preset& p) { return p.id != exceptId && lower(p.name) == wanted; });
}

std::string checkName(const Settings& s, const std::string& name, const std::string& exceptId = {})
{
    if (name.empty()) {
        return "Enter a name.";
    }
    if (utf8Length(name) > kMaxNameLength) {
        return "Use 24 characters or fewer.";
    }
    if (nameTaken(s, name, exceptId)) {
        return "\"" + name + "\" already exists. Pick another name.";
    }
    return {};
}

std::string newId(const Settings& s)
{
    int n = 1;
    while (findPreset(s, "p" + std::to_string(n))) {
        ++n;
    }
    return "p" + std::to_string(n);
}

bool validCutoff(double v) { return v >= 20.0 && v <= 200.0; }
bool validSlope(int v) { return oneOf(v, {12, 24, 36, 48}); }
bool validGain(double v) { return v >= -12.0 && v <= 12.0; }

}  // namespace

// ---------------------------------------------------------------------------
// JSON

std::string toJson(const Settings& s)
{
    json presets = json::array();
    for (const Preset& p : s.presets) {
        if (!p.builtIn) {  // Default is built in and never saved
            presets.push_back({{"id", p.id}, {"name", p.name}, {"cutoff", p.cutoff}, {"slope", p.slope}, {"gain", p.gain}});
        }
    }
    const json j = {
        {"version", Settings::kVersion},
        {"sourceMode", s.sourceMode},
        {"captureDevice", s.captureDevice},
        {"captureDeviceName", s.captureDeviceName},
        {"processExe", s.processExe},
        {"driver", s.driver},
        {"shakerId", s.shakerId},
        {"shakerName", s.shakerName},
        {"buffer", s.buffer},
        {"startWithWindows", s.startWithWindows},
        {"startMinimized", s.startMinimized},
        {"autoStart", s.autoStart},
        {"closeToTray", s.closeToTray},
        {"checkUpdates", s.checkUpdates},
        {"limiter", s.limiter},
        {"subsonic", s.subsonic},
        {"subsonicSlope", s.subsonicSlope},
        {"cutoff", s.cutoff},
        {"slope", s.slope},
        {"gain", s.gain},
        {"presets", presets},
        {"activePresetId", s.activePresetId},
        {"notifiedVersion", s.notifiedVersion},
    };
    return j.dump(2);
}

Settings fromJson(const std::string& text, bool* validOut)
{
    Settings s;
    bool valid = true;
    json j;
    try {
        j = json::parse(text);
    } catch (const json::exception&) {
        if (validOut) {
            *validOut = false;
        }
        return s;
    }
    if (!j.is_object()) {
        if (validOut) {
            *validOut = false;
        }
        return s;
    }

    read(j, "sourceMode", s.sourceMode, valid, [](const std::string& v) { return v == "system" || v == "process"; });
    read(j, "captureDevice", s.captureDevice, valid, [](const std::string& v) { return !v.empty(); });
    read(j, "captureDeviceName", s.captureDeviceName, valid);
    read(j, "processExe", s.processExe, valid);
    read(j, "driver", s.driver, valid, [](const std::string& v) { return v == "wasapi" || v == "asio"; });
    read(j, "shakerId", s.shakerId, valid);
    read(j, "shakerName", s.shakerName, valid);
    read(j, "buffer", s.buffer, valid, [](int v) { return oneOf(v, {32, 64, 128, 256}); });
    read(j, "startWithWindows", s.startWithWindows, valid);
    read(j, "startMinimized", s.startMinimized, valid);
    read(j, "autoStart", s.autoStart, valid);
    read(j, "closeToTray", s.closeToTray, valid);
    read(j, "checkUpdates", s.checkUpdates, valid);
    read(j, "limiter", s.limiter, valid);
    read(j, "subsonic", s.subsonic, valid, [](int v) { return oneOf(v, {0, 10, 15, 20, 30}); });
    read(j, "subsonicSlope", s.subsonicSlope, valid, [](int v) { return oneOf(v, {12, 24}); });
    read(j, "cutoff", s.cutoff, valid, validCutoff);
    read(j, "slope", s.slope, valid, validSlope);
    read(j, "gain", s.gain, valid, validGain);
    read(j, "notifiedVersion", s.notifiedVersion, valid);

    // User presets: each one is kept only if it is complete and valid.
    if (const auto it = j.find("presets"); it != j.end() && it->is_array()) {
        for (const json& item : *it) {
            Preset p;
            bool ok = item.is_object();
            if (ok) {
                read(item, "id", p.id, ok);
                read(item, "name", p.name, ok);
                read(item, "cutoff", p.cutoff, ok, validCutoff);
                read(item, "slope", p.slope, ok, validSlope);
                read(item, "gain", p.gain, ok, validGain);
            }
            ok = ok && !p.id.empty() && p.id != "default" && checkName(s, p.name).empty() && !findPreset(s, p.id);
            if (ok) {
                s.presets.push_back(p);
            } else {
                valid = false;
            }
        }
    }
    read(j, "activePresetId", s.activePresetId, valid);
    if (!findPreset(s, s.activePresetId)) {
        s.activePresetId = "default";
        valid = false;
    }
    if (validOut) {
        *validOut = valid;
    }
    return s;
}

Settings loadSettings(const std::filesystem::path& file)
{
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return Settings{};  // first run
    }
    std::stringstream text;
    text << in.rdbuf();
    in.close();

    try {
        (void)json::parse(text.str());
    } catch (const json::exception&) {
        // Unreadable: keep a copy for the user, start with defaults.
        std::error_code ec;
        std::filesystem::path backup = file;
        backup += ".bak";
        std::filesystem::rename(file, backup, ec);
        return Settings{};
    }
    return fromJson(text.str());
}

bool saveSettings(const std::filesystem::path& file, const Settings& s)
{
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    std::filesystem::path tmp = file;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return false;
        }
        out << toJson(s);
        out.flush();
        if (!out) {
            return false;
        }
    }
    std::filesystem::rename(tmp, file, ec);  // replaces settings.json in one step
    return !ec;
}

std::filesystem::path defaultSettingsPath()
{
    wchar_t* appData = nullptr;
    size_t length = 0;
    std::filesystem::path base = std::filesystem::temp_directory_path();
    if (_wdupenv_s(&appData, &length, L"APPDATA") == 0 && appData) {
        base = appData;
    }
    free(appData);
    return base / "GravitonFX" / "settings.json";
}

// ---------------------------------------------------------------------------
// Presets

const Preset* findPreset(const Settings& s, const std::string& id)
{
    for (const Preset& p : s.presets) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

const Preset& activePreset(const Settings& s)
{
    const Preset* p = findPreset(s, s.activePresetId);
    return p ? *p : s.presets.front();
}

bool presetModified(const Settings& s)
{
    const Preset& p = activePreset(s);
    return p.cutoff != s.cutoff || p.slope != s.slope || p.gain != s.gain;
}

void selectPreset(Settings& s, const std::string& id)
{
    if (const Preset* p = findPreset(s, id)) {
        s.activePresetId = p->id;
        s.cutoff = p->cutoff;
        s.slope = p->slope;
        s.gain = p->gain;
    }
}

void revertPreset(Settings& s) { selectPreset(s, s.activePresetId); }

void savePreset(Settings& s)
{
    for (Preset& p : s.presets) {
        if (p.id == s.activePresetId && !p.builtIn) {
            p.cutoff = s.cutoff;
            p.slope = s.slope;
            p.gain = s.gain;
        }
    }
}

std::string createPreset(Settings& s, const std::string& rawName)
{
    const std::string name = trim(rawName);
    if (std::string error = checkName(s, name); !error.empty()) {
        return error;
    }
    Preset p{newId(s), name, s.cutoff, s.slope, s.gain, false};
    s.presets.push_back(p);
    s.activePresetId = p.id;
    return {};
}

std::string duplicatePreset(Settings& s, const std::string& id)
{
    const Preset* source = findPreset(s, id);
    if (!source) {
        return "That preset no longer exists.";
    }
    const Preset copy = *source;
    // "Name copy", then "Name copy 2", ... within the 24-character limit.
    std::string name;
    for (int n = 1;; ++n) {
        const std::string suffix = n == 1 ? " copy" : " copy " + std::to_string(n);
        std::string base = copy.name;
        while (utf8Length(base) + suffix.size() > kMaxNameLength && !base.empty()) {
            do {
                base.pop_back();
            } while (!base.empty() && (base.back() & 0xC0) == 0x80);
        }
        name = trim(base) + suffix;
        if (!nameTaken(s, name)) {
            break;
        }
    }
    s.presets.push_back({newId(s), name, copy.cutoff, copy.slope, copy.gain, false});
    return {};
}

std::string renamePreset(Settings& s, const std::string& id, const std::string& rawName)
{
    const std::string name = trim(rawName);
    for (Preset& p : s.presets) {
        if (p.id == id) {
            if (p.builtIn) {
                return "Default can't be renamed.";
            }
            if (std::string error = checkName(s, name, id); !error.empty()) {
                return error;
            }
            p.name = name;
            return {};
        }
    }
    return "That preset no longer exists.";
}

void deletePreset(Settings& s, const std::string& id)
{
    const auto it = std::find_if(s.presets.begin(), s.presets.end(),
                                 [&](const Preset& p) { return p.id == id && !p.builtIn; });
    if (it == s.presets.end()) {
        return;
    }
    s.presets.erase(it);
    if (s.activePresetId == id) {
        selectPreset(s, "default");
    }
}

void resetSettings(Settings& s)
{
    Settings fresh;
    fresh.presets = s.presets;  // user presets stay
    fresh.notifiedVersion = s.notifiedVersion;
    s = fresh;
    selectPreset(s, "default");
}

}  // namespace gfx

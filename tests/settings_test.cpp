// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "host/Settings.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>

using namespace gfx;
namespace fs = std::filesystem;

namespace {

// A fresh folder under the temp directory, removed afterwards.
struct TempDir {
    fs::path path;
    TempDir()
    {
        path = fs::temp_directory_path() / ("gfx-test-" + std::to_string(std::rand()) + std::to_string(std::rand()));
        fs::remove_all(path);
    }
    ~TempDir() { fs::remove_all(path); }
};

void writeFile(const fs::path& p, const std::string& text)
{
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << text;
}

}  // namespace

TEST_CASE("Defaults match the spec")
{
    const Settings s;
    CHECK(s.sourceMode == "system");
    CHECK(s.captureDevice == "default");
    CHECK(s.driver == "wasapi");
    CHECK(s.buffer == 64);
    CHECK(s.subsonic == 20);
    CHECK(s.subsonicSlope == 12);
    CHECK(s.limiter);
    CHECK(s.cutoff == 80.0);
    CHECK(s.slope == 24);
    CHECK(s.gain == 0.0);
    CHECK(s.startWithWindows);
    CHECK_FALSE(s.startMinimized);
    CHECK_FALSE(s.autoStart);
    CHECK(s.closeToTray);
    CHECK(s.checkUpdates);
    REQUIRE(s.presets.size() == 1);
    CHECK(s.presets[0].builtIn);
    CHECK(s.activePresetId == "default");
}

TEST_CASE("Round trip: every field survives save and load")
{
    TempDir dir;
    const fs::path file = dir.path / "GravitonFX" / "settings.json";  // folder doesn't exist yet

    Settings s;
    s.sourceMode = "process";
    s.captureDevice = "{0.0.0.00000000}.{abc}";
    s.captureDeviceName = "Rig Speakers";
    s.processExe = "RaceSim.exe";
    s.driver = "asio";
    s.shakerId = "dac-id";
    s.shakerName = "USB DAC \xC3\xA9";  // non-ASCII survives
    s.buffer = 128;
    s.startWithWindows = false;
    s.startMinimized = true;
    s.autoStart = true;
    s.closeToTray = false;
    s.checkUpdates = false;
    s.limiter = false;
    s.subsonic = 30;
    s.subsonicSlope = 24;
    s.cutoff = 95.0;
    s.slope = 36;
    s.gain = -2.5;
    REQUIRE(createPreset(s, "Sim racing").empty());
    s.notifiedVersion = "0.2.0";

    REQUIRE(saveSettings(file, s));
    CHECK(fs::exists(file));
    CHECK_FALSE(fs::exists(fs::path(file) += ".tmp"));
    CHECK(loadSettings(file) == s);
}

TEST_CASE("Missing file or folder gives the defaults")
{
    TempDir dir;
    CHECK(loadSettings(dir.path / "nope" / "settings.json") == Settings{});
}

TEST_CASE("Invalid JSON: defaults, and the bad file is kept as settings.json.bak")
{
    TempDir dir;
    const fs::path file = dir.path / "settings.json";
    writeFile(file, "{ this is not json");
    CHECK(loadSettings(file) == Settings{});
    CHECK(fs::exists(fs::path(file) += ".bak"));
    CHECK_FALSE(fs::exists(file));
}

TEST_CASE("Valid parts are kept when some fields are wrong")
{
    bool valid = true;
    const Settings s = fromJson(R"({
        "buffer": 100,
        "cutoff": 120,
        "slope": "steep",
        "limiter": false,
        "subsonic": 25,
        "presets": [
            {"id": "p1", "name": "Movies", "cutoff": 60, "slope": 48, "gain": 3},
            {"id": "p2", "name": "movies", "cutoff": 60, "slope": 48, "gain": 3},
            {"id": "p3", "name": "Broken", "cutoff": 999, "slope": 24, "gain": 0}
        ],
        "activePresetId": "p9"
    })", &valid);
    CHECK_FALSE(valid);
    CHECK(s.buffer == 64);      // 100 isn't a buffer size
    CHECK(s.cutoff == 120.0);   // kept
    CHECK(s.slope == 24);       // wrong type
    CHECK_FALSE(s.limiter);     // kept
    CHECK(s.subsonic == 20);    // 25 isn't an option
    REQUIRE(s.presets.size() == 2);  // Default + Movies; the duplicate and the broken one are dropped
    CHECK(s.presets[1].name == "Movies");
    CHECK(s.activePresetId == "default");  // p9 doesn't exist
}

TEST_CASE("Default is never saved or overwritten from the file")
{
    const Settings s = fromJson(R"({"presets": [{"id": "default", "name": "Mine", "cutoff": 50, "slope": 12, "gain": 0}]})");
    REQUIRE(s.presets.size() == 1);
    CHECK(s.presets[0] == Settings::defaultPreset());
    CHECK(toJson(s).find("\"Default\"") == std::string::npos);
}

TEST_CASE("Save debouncer: one write 300 ms after the last change")
{
    SaveDebouncer d;
    d.changed(0.0);
    d.changed(0.1);
    d.changed(0.2);
    CHECK_FALSE(d.due(0.4));
    CHECK(d.due(0.5));
    CHECK_FALSE(d.due(0.6));  // only once per burst
}

// ---------------------------------------------------------------------------
// Presets

TEST_CASE("Create: becomes active with the current values; names are unique ignoring case")
{
    Settings s;
    s.cutoff = 70.0;
    CHECK(createPreset(s, "Sim racing").empty());
    CHECK(activePreset(s).name == "Sim racing");
    CHECK(activePreset(s).cutoff == 70.0);
    CHECK(createPreset(s, "default") == "\"default\" already exists. Pick another name.");
    CHECK(createPreset(s, "SIM RACING") != "");
    CHECK(createPreset(s, "   ") == "Enter a name.");
    CHECK(createPreset(s, std::string(25, 'x')) == "Use 24 characters or fewer.");
    CHECK(createPreset(s, std::string(24, 'x')).empty());
}

TEST_CASE("Modified, revert and save")
{
    Settings s;
    CHECK_FALSE(presetModified(s));
    s.cutoff = 100.0;
    CHECK(presetModified(s));
    savePreset(s);  // Default can't be overwritten
    CHECK(activePreset(s).cutoff == 80.0);
    revertPreset(s);
    CHECK(s.cutoff == 80.0);
    CHECK_FALSE(presetModified(s));

    createPreset(s, "Movies");
    s.gain = 4.0;
    CHECK(presetModified(s));
    savePreset(s);
    CHECK_FALSE(presetModified(s));
    CHECK(activePreset(s).gain == 4.0);
}

TEST_CASE("Select applies only cutoff, slope and gain")
{
    Settings s;
    s.cutoff = 60.0;
    s.slope = 48;
    s.gain = 3.0;
    createPreset(s, "Movies");
    selectPreset(s, "default");
    s.buffer = 256;
    s.subsonic = 30;
    selectPreset(s, activePreset(s).id == "default" ? s.presets[1].id : "default");
    CHECK(s.cutoff == 60.0);
    CHECK(s.slope == 48);
    CHECK(s.gain == 3.0);
    CHECK(s.buffer == 256);
    CHECK(s.subsonic == 30);
}

TEST_CASE("Rename: Default is locked, names stay unique, renaming to itself is fine")
{
    Settings s;
    createPreset(s, "Movies");
    createPreset(s, "Racing");
    const std::string movies = s.presets[1].id;
    CHECK(renamePreset(s, "default", "Base") == "Default can't be renamed.");
    CHECK(renamePreset(s, movies, "racing") != "");
    CHECK(renamePreset(s, movies, "MOVIES").empty());
    CHECK(renamePreset(s, movies, "Films").empty());
    CHECK(findPreset(s, movies)->name == "Films");
}

TEST_CASE("Duplicate makes 'Name copy', then 'Name copy 2', within 24 characters")
{
    Settings s;
    createPreset(s, "Sim racing");
    const std::string id = s.presets[1].id;
    CHECK(duplicatePreset(s, id).empty());
    CHECK(duplicatePreset(s, id).empty());
    CHECK(s.presets[2].name == "Sim racing copy");
    CHECK(s.presets[3].name == "Sim racing copy 2");
    CHECK(s.presets[2].cutoff == s.presets[1].cutoff);
    CHECK(duplicatePreset(s, "default").empty());
    CHECK(s.presets.back().name == "Default copy");

    createPreset(s, std::string(24, 'a'));
    duplicatePreset(s, s.presets.back().id);
    CHECK(s.presets.back().name.size() <= 24);
    CHECK(s.presets.back().name.ends_with(" copy"));
}

TEST_CASE("Delete: Default can't be deleted; deleting the active preset activates Default")
{
    Settings s;
    s.cutoff = 150.0;
    createPreset(s, "Loud");
    deletePreset(s, "default");
    CHECK(s.presets.size() == 2);
    deletePreset(s, s.activePresetId);
    CHECK(s.presets.size() == 1);
    CHECK(s.activePresetId == "default");
    CHECK(s.cutoff == 80.0);  // Default's values applied
}

TEST_CASE("Reset to defaults keeps user presets and activates Default")
{
    Settings s;
    createPreset(s, "Movies");
    createPreset(s, "Racing");
    s.buffer = 256;
    s.limiter = false;
    s.checkUpdates = false;
    s.notifiedVersion = "0.3.0";
    resetSettings(s);
    CHECK(s.presets.size() == 3);
    CHECK(s.activePresetId == "default");
    CHECK(s.buffer == 64);
    CHECK(s.limiter);
    CHECK(s.checkUpdates);
    CHECK(s.notifiedVersion == "0.3.0");
}

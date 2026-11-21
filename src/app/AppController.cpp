// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "app/AppController.h"

#include "app/Autostart.h"
#include "host/Updates.h"

#include <nlohmann/json.hpp>
#include <shellapi.h>

#include <chrono>

namespace gfx {

using nlohmann::json;

namespace {

std::wstring widen(const std::string& s)
{
    if (s.empty()) {
        return {};
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

std::string narrow(const std::wstring& w)
{
    if (w.empty()) {
        return {};
    }
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

// "stereo", "5.1", "7.1" or "N ch".
std::string layout(int channels)
{
    switch (channels) {
    case 2: return "stereo";
    case 6: return "5.1";
    case 8: return "7.1";
    default: return std::to_string(channels) + " ch";
    }
}

std::string bitsText(int bits, bool isFloat)
{
    return isFloat ? "32-bit float" : std::to_string(bits) + "-bit";
}

const char* phaseText(Phase p)
{
    switch (p) {
    case Phase::Starting: return "starting";
    case Phase::Waiting: return "waiting";
    case Phase::Switching: return "switching";
    case Phase::Error: return "error";
    default: return nullptr;  // Stopped and Running carry no phase
    }
}

// Keys the UI may change with `set`; everything else is ignored.
bool settable(const std::string& key)
{
    static const char* keys[] = {"sourceMode", "captureDevice", "processExe", "driver", "shakerId", "buffer",
                                 "startWithWindows", "startMinimized", "autoStart", "closeToTray", "checkUpdates",
                                 "limiter", "subsonic", "subsonicSlope", "cutoff", "slope", "gain"};
    for (const char* k : keys) {
        if (key == k) {
            return true;
        }
    }
    return false;
}

}  // namespace

AppController::AppController(MainWindow& window, const std::filesystem::path& settingsFile)
    : window_(window), updates_(window.hwnd(), WM_APP_UPDATE), settingsFile_(settingsFile),
      settings_(loadSettings(settingsFile))
{
    nextAutoCheck_ = seconds() + 10;  // the first automatic update check
    const HWND hwnd = window.hwnd();
    engine_.setStateListener([hwnd](const EngineState&) { PostMessageW(hwnd, WM_APP_ENGINE_STATE, 0, 0); });
    engine_.setDevicesListener([hwnd] { PostMessageW(hwnd, WM_APP_DEVICES, 0, 0); });
    tray_.create(hwnd, GetModuleHandleW(nullptr));
    refreshDevices();
    chooseShakerIfUnset();
    applyToEngine();
    syncStartWithWindows();
    updateTray();
}

AppController::~AppController() { saveNow(); }

double AppController::seconds() const
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

void AppController::saveNow()
{
    saveSettings(settingsFile_, settings_);
}

// ---------------------------------------------------------------------------
// Engine and devices

void AppController::applyToEngine()
{
    EngineSettings e;
    e.sourceMode = settings_.sourceMode == "process" ? EngineSettings::SourceMode::App
                                                     : EngineSettings::SourceMode::SystemMix;
    e.captureFollowsDefault = settings_.captureDevice == "default";
    e.captureDeviceId = e.captureFollowsDefault ? std::wstring() : widen(settings_.captureDevice);
    e.captureDeviceName = widen(settings_.captureDeviceName);
    e.appExe = widen(settings_.processExe);
    e.driver = settings_.driver == "asio" ? OutputDriver::Asio : OutputDriver::WasapiExclusive;
    e.shakerDeviceId = widen(settings_.shakerId);
    e.shakerDeviceName = widen(settings_.shakerName);
    e.bufferFrames = settings_.buffer;
    e.filter.cutoffHz = settings_.cutoff;
    e.filter.slope = settings_.slope;
    e.filter.subsonicHz = settings_.subsonic;
    e.filter.subsonicSlope = settings_.subsonicSlope;
    e.filter.gainDb = settings_.gain;
    e.limiter = settings_.limiter;
    engine_.apply(e);
}

void AppController::refreshDevices()
{
    endpoints_ = engine_.renderDevices();
    apps_ = engine_.audioApps();
    asioDrivers_ = listAsioDrivers();
    defaultId_.clear();
    for (const auto& d : endpoints_) {
        if (d.isDefault) {
            defaultId_ = d.id;
        }
    }
}

// First run: pick a shaker device so the panel isn't empty. Prefers a device
// that isn't the Windows default (the default is usually what you listen on).
// Nothing plays until the user presses Start or a test.
void AppController::chooseShakerIfUnset()
{
    if (!settings_.shakerId.empty() || endpoints_.empty()) {
        return;
    }
    const AudioDevice* pick = &endpoints_.front();
    for (const auto& d : endpoints_) {
        if (!d.isDefault) {
            pick = &d;
            break;
        }
    }
    settings_.shakerId = narrow(pick->id);
    settings_.shakerName = narrow(pick->name);
    saver_.changed(seconds());
}

std::string AppController::deviceName(const std::string& id) const
{
    const std::wstring wid = widen(id);
    for (const auto& d : endpoints_) {
        if (d.id == wid) {
            return narrow(d.name);
        }
    }
    for (const auto& d : asioDrivers_) {
        if (_wcsicmp(d.id.c_str(), wid.c_str()) == 0) {
            return narrow(d.name);
        }
    }
    return {};
}

bool AppController::asioPresent(const std::wstring& id) const
{
    for (const auto& d : asioDrivers_) {
        if (_wcsicmp(d.id.c_str(), id.c_str()) == 0) {
            return true;
        }
    }
    return false;
}

// An ASIO driver's rate, sample type and outputs are known only once it's
// loaded. The selected driver is loaded once, briefly, while the engine
// doesn't hold it, and the result is kept for the session.
OutputFormat AppController::asioFormat(const std::wstring& id)
{
    auto it = asioFormats_.find(id);
    if (it == asioFormats_.end()) {
        OutputFormat f;
        if (!describeAsioDriver(id, f)) {
            f = {};
        }
        it = asioFormats_.emplace(id, f).first;
    }
    return it->second;
}

void AppController::onDevicesChanged()
{
    refreshDevices();
    postDevices();
    postState();
}

void AppController::onEngineState()
{
    postState();
    updateTray();
}

// ---------------------------------------------------------------------------
// Tray, window and Windows integration

void AppController::updateTray()
{
    const EngineState es = engine_.state();
    TrayState icon = TrayState::Stopped;
    const wchar_t* word = L"Stopped";
    switch (es.phase) {
    case Phase::Stopped: break;
    case Phase::Starting: icon = TrayState::Running; word = L"Starting"; break;
    case Phase::Running: icon = TrayState::Running; word = L"Running"; break;
    case Phase::Switching: icon = TrayState::Running; word = L"Switching"; break;
    case Phase::Waiting: icon = TrayState::Waiting; word = L"Waiting"; break;
    case Phase::Error: icon = TrayState::Error; word = L"Error"; break;
    }
    if (es.checkOutput) {  // as the status pill: shown even while stopped
        icon = TrayState::Error;
        word = L"Check output";
    }
    tray_.set(icon, std::wstring(L"Graviton FX \u00b7 ") + word);
}

void AppController::onTrayEvent(LPARAM lParam)
{
    switch (LOWORD(lParam)) {
    case NIN_SELECT:     // click
    case NIN_KEYSELECT:  // Enter or Space on the icon
        showWindow();
        break;
    case NIN_BALLOONUSERCLICK:  // the "new version" notification
        openReleasePage();
        break;
    case WM_CONTEXTMENU:
        switch (tray_.showMenu(engine_.state().phase != Phase::Stopped)) {
        case Tray::Open: showWindow(); break;
        case Tray::StartStop:
            if (engine_.state().phase != Phase::Stopped) {
                engine_.stop();
            } else {
                engine_.start();
            }
            break;
        case Tray::Quit:
            if (onQuit) {
                onQuit();
            }
            break;
        default: break;
        }
        break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// Update checks (automatic only while the setting is on; Check now always)

void AppController::checkForUpdates(bool manual)
{
    if (!updates_.start()) {
        return;  // one is already running
    }
    manualCheck_ = manual;
    if (manual) {
        updateStatus_ = "checking";
        postState();
    }
}

void AppController::onUpdateResult()
{
    const UpdateChecker::Result r = updates_.result();
    switch (r.kind) {
    case UpdateChecker::Result::Kind::Available:
        updateStatus_ = "available";
        updateLatest_ = r.latest;
        if (settings_.notifiedVersion != r.latest) {  // one Windows notification per version
            tray_.notify(L"Graviton FX", L"Graviton FX " + std::wstring(r.latest.begin(), r.latest.end()) + L" is available");
            settings_.notifiedVersion = r.latest;
            saver_.changed(seconds());
        }
        break;
    case UpdateChecker::Result::Kind::UpToDate:
        updateStatus_ = "uptodate";
        updateLatest_.clear();
        break;
    case UpdateChecker::Result::Kind::Failed:
        if (manualCheck_) {
            updateStatus_ = "error";  // automatic checks fail silently
        }
        break;
    }
    postState();
}

void AppController::openReleasePage()
{
    ShellExecuteW(nullptr, L"open", widen(std::string(kRepoUrl) + "/releases/latest").c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void AppController::showWindow()
{
    window_.showAndFocus();
}

void AppController::hideToTray()
{
    window_.hide();
}

void AppController::closeRequested()
{
    if (settings_.closeToTray) {
        hideToTray();
    } else if (onQuit) {
        onQuit();
    }
}

// Keeps the Run value in step with "Start with Windows" (written only when it changes).
void AppController::syncStartWithWindows()
{
    const int on = settings_.startWithWindows ? 1 : 0;
    if (on != startWithWindows_) {
        startWithWindows_ = on;
        setStartWithWindows(on != 0);
    }
}

// ---------------------------------------------------------------------------
// Messages from the UI

void AppController::onPageReady()
{
    pageReady_ = true;
    postDevices();
    postState();
}

// Messages come from the page. Anything malformed (not JSON, not an object,
// a field of the wrong type) is ignored, never fatal.
void AppController::onUiMessage(const std::string& text)
{
    try {
        const json m = json::parse(text);
        if (m.is_object()) {
            handleCommand(m);
        }
    } catch (const json::exception&) {
    }
}

void AppController::handleCommand(const json& m)
{
    const std::string cmd = m.value("cmd", "");
    const std::string id = m.value("id", "");
    const std::string name = m.value("name", "");

    if (cmd == "start") {
        engine_.start();
    } else if (cmd == "stop") {
        engine_.stop();
    } else if (cmd == "set" && m.contains("key") && m.contains("value")) {
        set(m["key"].get<std::string>(), m["value"]);
    } else if (cmd == "pulseStart") {
        engine_.pulseStart();
    } else if (cmd == "pulseStop") {
        engine_.pulseStop();
    } else if (cmd == "sweep") {
        engine_.sweep();
    } else if (cmd == "refreshProcesses") {
        apps_ = engine_.audioApps();
        postDevices();
        postState();
    } else if (cmd == "resetSettings") {
        resetSettings(settings_);
        chooseShakerIfUnset();
        changed();
    } else if (cmd == "presetSelect") {
        selectPreset(settings_, id);
        changed();
    } else if (cmd == "presetRevert") {
        revertPreset(settings_);
        changed();
    } else if (cmd == "presetSave") {
        savePreset(settings_);
        changed(false);
    } else if (cmd == "presetCreate") {
        createPreset(settings_, name);  // the UI checks names before sending
        changed(false);
    } else if (cmd == "presetDuplicate") {
        duplicatePreset(settings_, id);
        changed(false);
    } else if (cmd == "presetRename") {
        renamePreset(settings_, id, name);
        changed(false);
    } else if (cmd == "presetDelete") {
        deletePreset(settings_, id);
        changed();
    } else if (cmd == "checkUpdates") {
        checkForUpdates(true);
    } else if (cmd == "openUrl") {
        // Only the project's own GitHub pages.
        const std::string url = m.value("url", "");
        if (isProjectUrl(url)) {
            ShellExecuteW(nullptr, L"open", widen(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
    } else if (cmd == "windowMinimize") {
        // Posted, so they take the same path as Windows' own minimize and
        // close, after this WebView callback has returned.
        PostMessageW(window_.hwnd(), WM_SYSCOMMAND, SC_MINIMIZE, 0);
    } else if (cmd == "windowClose") {
        PostMessageW(window_.hwnd(), WM_CLOSE, 0, 0);
    }
}

// Changes one setting. The value goes through the same validation as
// settings.json, so the UI can't store anything out of range.
void AppController::set(const std::string& key, const json& value)
{
    if (!settable(key)) {
        return;
    }
    json j = json::parse(toJson(settings_));
    j[key] = value;
    bool valid = true;
    Settings next = fromJson(j.dump(), &valid);
    if (j[key] != json::parse(toJson(next))[key]) {
        return;  // rejected
    }

    // Remember device names so the UI can name them while they're missing.
    if (key == "captureDevice" && next.captureDevice != "default") {
        next.captureDeviceName = deviceName(next.captureDevice);
    }
    if (key == "shakerId") {
        next.shakerName = deviceName(next.shakerId);
    }
    if (key == "driver" && next.driver != settings_.driver) {
        // Switching drivers selects the first device of the new driver.
        next.shakerId.clear();
        next.shakerName.clear();
        if (next.driver == "wasapi" && !endpoints_.empty()) {
            next.shakerId = narrow(endpoints_.front().id);
            next.shakerName = narrow(endpoints_.front().name);
        }
        if (next.driver == "asio" && !asioDrivers_.empty()) {
            next.shakerId = narrow(asioDrivers_.front().id);
            next.shakerName = narrow(asioDrivers_.front().name);
        }
    }
    const bool driverChanged = key == "driver" && next.driver != settings_.driver;
    settings_ = next;
    if (driverChanged) {
        postDevices();  // the shaker list depends on the driver
    }
    changed();
}

void AppController::changed(bool engineToo)
{
    saver_.changed(seconds());
    if (engineToo) {
        applyToEngine();
    }
    syncStartWithWindows();
    postState();
}

void AppController::onTimer()
{
    if (saver_.due(seconds())) {
        saveNow();
    }
    if (settings_.checkUpdates && seconds() >= nextAutoCheck_) {
        nextAutoCheck_ = seconds() + 24 * 3600;  // then once a day while running
        checkForUpdates(false);
    }
    if (stale_ && !hidden()) {
        postDevices();  // what changed while minimized or hidden
        postState();
    }
    postTelemetry();
}

// ---------------------------------------------------------------------------
// Messages to the UI

// Minimized or hidden: the page is suspended, and a message would wake it.
bool AppController::hidden() const
{
    return !IsWindowVisible(window_.hwnd()) || IsIconic(window_.hwnd());
}

void AppController::postState()
{
    if (!pageReady_) {
        return;
    }
    if (hidden()) {
        stale_ = true;  // sent on restore
        return;
    }
    stale_ = false;
    const EngineState es = engine_.state();
    json m = json::parse(toJson(settings_));
    m.erase("version");

    // Presets, with Default first (the file leaves it out).
    json presets = json::array();
    for (const Preset& p : settings_.presets) {
        presets.push_back({{"id", p.id}, {"name", p.name}, {"builtIn", p.builtIn}, {"cutoff", p.cutoff},
                           {"slope", p.slope}, {"gain", p.gain}});
    }
    m["presets"] = presets;

    m["type"] = "state";
    m["running"] = es.phase != Phase::Stopped;
    const char* phase = phaseText(es.phase);
    m["phase"] = phase ? json(phase) : json(nullptr);
    m["errorMsg"] = es.messageIsError ? json(narrow(es.message)) : json(nullptr);
    m["errorPanel"] = es.messagePanel == Panel::Source ? "source" : "shaker";  // which selector to outline
    m["retrying"] = es.retrying;
    m["feedback"] = es.checkOutput;
    m["testing"] = es.phase == Phase::Stopped && es.outputOpen;
    m["pulsing"] = es.pulsing;
    m["sweeping"] = es.sweeping;
    m["testMsg"] = es.testMessage.empty() ? json(nullptr) : json(narrow(es.testMessage));
    m["version"] = GFX_VERSION;

    // The System mix device being captured (or that would be).
    const std::wstring capId = settings_.captureDevice == "default" ? defaultId_ : widen(settings_.captureDevice);
    const AudioDevice* cap = nullptr;
    for (const auto& d : endpoints_) {
        if (d.id == capId) {
            cap = &d;
        }
    }
    json captureDev = {{"name", cap ? narrow(cap->name) : settings_.captureDeviceName}, {"missing", cap == nullptr}};
    if (es.captureOpen && settings_.sourceMode == "system") {
        captureDev["rate"] = es.captureFormat.sampleRate;
        captureDev["fmt"] = bitsText(es.captureFormat.bits, es.captureFormat.isFloat) + " · " + layout(es.captureFormat.channels);
    } else {
        captureDev["rate"] = cap ? cap->sampleRate : 48000;
        captureDev["fmt"] = "32-bit float · " + layout(cap ? cap->channels : 2);  // loopback is always float
    }
    m["captureDev"] = captureDev;
    m["captureSel"] = {{"id", settings_.captureDevice},
                       {"name", settings_.captureDevice == "default" ? "Windows default"
                                                                     : (cap ? narrow(cap->name) : settings_.captureDeviceName)},
                       {"present", settings_.captureDevice == "default" || cap != nullptr}};

    // The selected app.
    json process = {{"exe", settings_.processExe}, {"name", settings_.processExe}, {"running", false}, {"playing", false}};
    for (const auto& a : apps_) {
        if (_wcsicmp(a.exe.c_str(), widen(settings_.processExe).c_str()) == 0) {
            process = {{"exe", narrow(a.exe)}, {"name", narrow(a.name)}, {"running", true}, {"playing", a.playing}};
        }
    }
    m["process"] = process;

    // The shaker device: its format once open, otherwise what Windows reports.
    const std::wstring shId = widen(settings_.shakerId);
    const AudioDevice* sh = nullptr;
    for (const auto& d : endpoints_) {
        if (d.id == shId) {
            sh = &d;
        }
    }
    const bool asio = settings_.driver == "asio";
    const bool present = asio ? asioPresent(shId) : sh != nullptr;
    const std::string name = asio && asioDrivers_.empty() ? std::string("No ASIO driver installed")
                           : asio ? (present ? deviceName(settings_.shakerId) : settings_.shakerName)
                                  : (sh ? narrow(sh->name) : settings_.shakerName);
    json shakerDev = {{"id", settings_.shakerId}, {"name", name}, {"present", present}};
    const OutputFormat known = es.outputOpen ? es.outputFormat
                               : (asio && present) ? asioFormat(shId)
                                                   : OutputFormat{};
    if (known.channels > 0) {
        shakerDev["rate"] = known.sampleRate;
        shakerDev["fmt"] = bitsText(known.bits, known.isFloat) + " · " + layout(known.channels);
        shakerDev["ch"] = known.channels;
    } else if (asio) {
        shakerDev["rate"] = 48000;  // not readable now (another app may hold the driver)
        shakerDev["fmt"] = "ASIO";
        shakerDev["ch"] = 2;
    } else {
        shakerDev["rate"] = sh ? sh->sampleRate : 48000;
        shakerDev["fmt"] = layout(sh ? sh->channels : 2);
        shakerDev["ch"] = sh ? sh->channels : 2;
    }
    m["shakerDev"] = shakerDev;

    m["update"] = {{"status", updateStatus_}, {"latest", updateLatest_}, {"current", GFX_VERSION}};
    window_.web().post(m.dump());
}

void AppController::postDevices()
{
    if (!pageReady_) {
        return;
    }
    if (hidden()) {
        stale_ = true;
        return;
    }
    json endpoints = json::array(), shaker = json::array(), processes = json::array();
    for (const auto& d : endpoints_) {
        endpoints.push_back({{"id", narrow(d.id)}, {"name", narrow(d.name)}, {"endpoint", narrow(d.id)}});
        if (settings_.driver == "wasapi") {
            shaker.push_back({{"id", narrow(d.id)}, {"name", narrow(d.name)}, {"endpoint", narrow(d.id)},
                              {"rate", d.sampleRate}, {"fmt", layout(d.channels)}, {"ch", d.channels}});
        }
    }
    if (settings_.driver == "asio") {
        for (const auto& d : asioDrivers_) {
            // Formats only for drivers already read; listing never loads a driver.
            const auto it = asioFormats_.find(d.id);
            const OutputFormat f = it != asioFormats_.end() ? it->second : OutputFormat{};
            shaker.push_back({{"id", narrow(d.id)}, {"name", narrow(d.name)}, {"endpoint", narrow(d.id)},
                              {"rate", f.channels ? f.sampleRate : 48000.0},
                              {"fmt", f.channels ? bitsText(f.bits, f.isFloat) + " · " + layout(f.channels) : std::string("ASIO")},
                              {"ch", f.channels ? f.channels : 2}});
        }
    }
    for (const auto& a : apps_) {
        processes.push_back({{"exe", narrow(a.exe)}, {"name", narrow(a.name)}, {"playing", a.playing}});
    }
    const json m = {{"type", "devices"}, {"renderEndpoints", endpoints}, {"defaultId", narrow(defaultId_)},
                    {"processes", processes}, {"shaker", shaker}};
    window_.web().post(m.dump());
}

void AppController::postTelemetry()
{
    if (!pageReady_ || hidden()) {
        return;  // nobody is looking
    }
    // Stopped with no test signal: nothing changes, so send one last update
    // (the levels at rest) and then nothing until something plays.
    const EngineState es = engine_.state();
    const bool idle = es.phase == Phase::Stopped && !es.outputOpen;
    if (idle && idleSent_) {
        return;
    }
    idleSent_ = idle;
    const Telemetry t = engine_.telemetry();
    const json m = {
        {"type", "telemetry"},
        {"test", t.test},
        {"sweepHz", t.sweepHz},
        {"spectrum", {{"in", t.spectrumIn}, {"out", t.spectrumOut}}},
        {"inDb", t.inDb},
        {"outDb", t.outDb},
        {"latencyMs", t.latencyMs},
        {"driftPpm", t.driftPpm},
        {"locked", t.locked},
        {"fill", t.fill},
        {"dropouts", t.dropouts},
        {"limiterHit", t.limiterHit || t.clipped},
        {"noSignal", t.noSignal},
    };
    window_.web().post(m.dump());
}

}  // namespace gfx

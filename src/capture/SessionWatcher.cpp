// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "capture/SessionWatcher.h"

#include <windows.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <wil/com.h>
#include <wil/resource.h>

#include <algorithm>
#include <map>
#include <vector>

namespace gfx {

namespace {

std::wstring lower(std::wstring s)
{
    std::transform(s.begin(), s.end(), s.begin(), ::towlower);
    return s;
}

// Full path of a process's executable, or empty if it can't be read.
std::wstring imagePath(DWORD pid)
{
    wil::unique_handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
    if (!process) {
        return {};
    }
    wchar_t path[MAX_PATH * 2];
    DWORD size = ARRAYSIZE(path);
    return QueryFullProcessImageNameW(process.get(), 0, path, &size) ? std::wstring(path, size) : std::wstring();
}

// The "File description" from the executable's version info, if any.
std::wstring fileDescription(const std::wstring& path)
{
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &ignored);
    if (size == 0) {
        return {};
    }
    std::vector<uint8_t> data(size);
    if (!GetFileVersionInfoW(path.c_str(), 0, size, data.data())) {
        return {};
    }
    struct Translation {
        WORD language;
        WORD codePage;
    }* translations = nullptr;
    UINT bytes = 0;
    if (!VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<void**>(&translations), &bytes) ||
        bytes < sizeof(Translation)) {
        return {};
    }
    wchar_t key[64];
    swprintf_s(key, L"\\StringFileInfo\\%04x%04x\\FileDescription", translations[0].language,
               translations[0].codePage);
    wchar_t* value = nullptr;
    UINT length = 0;
    if (!VerQueryValueW(data.data(), key, reinterpret_cast<void**>(&value), &length) || length <= 1) {
        return {};
    }
    return value;
}

}  // namespace

std::vector<AudioApp> SessionWatcher::list() const
{
    std::map<std::wstring, AudioApp> byExe;  // keyed by lower-case exe name
    const DWORD self = GetCurrentProcessId();

    wil::com_ptr<IMMDeviceCollection> devices;
    if (FAILED(enumerator_->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices))) {
        return {};
    }
    UINT deviceCount = 0;
    devices->GetCount(&deviceCount);
    for (UINT d = 0; d < deviceCount; ++d) {
        wil::com_ptr<IMMDevice> device;
        wil::com_ptr<IAudioSessionManager2> manager;
        wil::com_ptr<IAudioSessionEnumerator> sessions;
        if (FAILED(devices->Item(d, &device)) ||
            FAILED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, manager.put_void())) ||
            FAILED(manager->GetSessionEnumerator(&sessions))) {
            continue;
        }
        int sessionCount = 0;
        sessions->GetCount(&sessionCount);
        for (int s = 0; s < sessionCount; ++s) {
            wil::com_ptr<IAudioSessionControl> control;
            if (FAILED(sessions->GetSession(s, &control))) {
                continue;
            }
            auto control2 = control.try_query<IAudioSessionControl2>();
            DWORD pid = 0;
            if (!control2 || control2->IsSystemSoundsSession() == S_OK || FAILED(control2->GetProcessId(&pid)) ||
                pid == 0 || pid == self) {
                continue;
            }
            AudioSessionState state = AudioSessionStateInactive;
            control->GetState(&state);
            if (state == AudioSessionStateExpired) {
                continue;
            }
            const std::wstring path = imagePath(pid);
            if (path.empty()) {
                continue;
            }

            AudioApp app;
            app.processId = pid;
            app.exe = path.substr(path.find_last_of(L'\\') + 1);
            app.name = fileDescription(path);
            if (app.name.empty()) {
                app.name = app.exe.substr(0, app.exe.find_last_of(L'.'));
            }
            app.playing = state == AudioSessionStateActive;

            auto [it, inserted] = byExe.try_emplace(lower(app.exe), app);
            if (!inserted && app.playing && !it->second.playing) {
                it->second = app;  // prefer the process that is playing
            }
        }
    }

    std::vector<AudioApp> result;
    for (auto& [key, app] : byExe) {
        result.push_back(std::move(app));
    }
    std::sort(result.begin(), result.end(), [](const AudioApp& a, const AudioApp& b) {
        return lower(a.name) < lower(b.name);
    });
    return result;
}

uint32_t SessionWatcher::findProcess(const std::wstring& exe) const
{
    const std::wstring wanted = lower(exe);
    for (const AudioApp& app : list()) {
        if (lower(app.exe) == wanted) {
            return app.processId;
        }
    }
    return 0;
}

}  // namespace gfx

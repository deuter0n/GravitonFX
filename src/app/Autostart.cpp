// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "app/Autostart.h"

#include <windows.h>
#include <shellapi.h>

#include <string>

namespace gfx {

namespace {

constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kValueName[] = L"GravitonFX";
constexpr wchar_t kFlag[] = L"--autostart";

std::wstring exePath()
{
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (n < path.size()) {
            path.resize(n);
            return path;
        }
        path.resize(path.size() * 2);
    }
}

// True for builds running from the source folder (GFX_SOURCE_DIR is set by CMake).
bool isDevelopmentBuild(const std::wstring& exe)
{
    std::wstring source = GFX_SOURCE_DIR;
    for (auto& c : source) {
        if (c == L'/') {
            c = L'\\';
        }
    }
    return exe.size() > source.size() && _wcsnicmp(exe.c_str(), source.c_str(), source.size()) == 0;
}

}  // namespace

bool setStartWithWindows(bool on)
{
    const std::wstring exe = exePath();
    if (isDevelopmentBuild(exe)) {
        return false;
    }
    if (!on) {
        const LSTATUS r = RegDeleteKeyValueW(HKEY_CURRENT_USER, kRunKey, kValueName);
        return r == ERROR_SUCCESS || r == ERROR_FILE_NOT_FOUND;
    }
    const std::wstring command = L"\"" + exe + L"\" " + kFlag;
    return RegSetKeyValueW(HKEY_CURRENT_USER, kRunKey, kValueName, REG_SZ, command.c_str(),
                           static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}

bool launchedAtSignIn()
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool found = false;
    for (int i = 1; argv && i < argc; ++i) {
        if (_wcsicmp(argv[i], kFlag) == 0) {
            found = true;
        }
    }
    LocalFree(argv);
    return found;
}

}  // namespace gfx

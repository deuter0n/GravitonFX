// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "app/SingleInstance.h"

#include "app/AppMessages.h"
#include "app/MainWindow.h"

namespace gfx {

namespace {

constexpr wchar_t kMutexName[] = L"Local\\GravitonFX.SingleInstance";

}  // namespace

SingleInstance::SingleInstance()
{
    mutex_ = CreateMutexW(nullptr, FALSE, kMutexName);
    first_ = !(mutex_ && GetLastError() == ERROR_ALREADY_EXISTS);
}

SingleInstance::~SingleInstance()
{
    if (mutex_) {
        CloseHandle(mutex_);
    }
}

void SingleInstance::showFirstInstance()
{
    // The window exists even while hidden in the tray. If the first instance
    // is still starting up, there is nothing to show yet.
    const HWND window = FindWindowW(MainWindow::kClassName, nullptr);
    if (!window) {
        return;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    AllowSetForegroundWindow(pid);  // we were just launched, so we may pass this on
    PostMessageW(window, WM_APP_SHOW, 0, 0);
}

}  // namespace gfx

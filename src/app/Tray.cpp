// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "app/Tray.h"

#include "app/AppMessages.h"
#include "app/resource.h"

#include <commctrl.h>
#include <shellapi.h>

#include <cwchar>

namespace gfx {

namespace {

constexpr UINT kIconId = 1;
constexpr UINT_PTR kRetryTimer = 0x7472;  // "tr"; the app's own timer uses 1
constexpr UINT kRetryMs = 2000;

}  // namespace

Tray::~Tray()
{
    if (owner_) {
        NOTIFYICONDATAW d = data(0);
        Shell_NotifyIconW(NIM_DELETE, &d);
    }
    for (HICON icon : icons_) {
        if (icon) {
            DestroyIcon(icon);
        }
    }
}

void Tray::create(HWND owner, HINSTANCE instance)
{
    owner_ = owner;
    const int ids[4] = {IDI_TRAY_RUNNING, IDI_TRAY_STOPPED, IDI_TRAY_WAITING, IDI_TRAY_ERROR};
    for (int i = 0; i < 4; ++i) {
        LoadIconMetric(instance, MAKEINTRESOURCEW(ids[i]), LIM_SMALL, &icons_[i]);  // the system's small-icon size
    }
    taskbarCreated_ = RegisterWindowMessageW(L"TaskbarCreated");
    add();
}

NOTIFYICONDATAW Tray::data(UINT flags) const
{
    NOTIFYICONDATAW d{};
    d.cbSize = sizeof(d);
    d.hWnd = owner_;
    d.uID = kIconId;
    d.uFlags = flags;
    return d;
}

bool Tray::tryAdd()
{
    NOTIFYICONDATAW d = data(NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP);
    d.uCallbackMessage = WM_APP_TRAY;
    d.hIcon = icons_[static_cast<int>(state_)];
    wcsncpy_s(d.szTip, tooltip_.c_str(), _TRUNCATE);
    if (!Shell_NotifyIconW(NIM_ADD, &d)) {
        return false;
    }
    d.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &d);
    return true;
}

// Adds the icon now, or keeps retrying every 2 s until Explorer accepts it.
void Tray::add()
{
    if (tryAdd()) {
        KillTimer(owner_, kRetryTimer);
    } else {
        SetTimer(owner_, kRetryTimer, kRetryMs, nullptr);
    }
}

bool Tray::handleTimer(UINT_PTR timerId)
{
    if (timerId != kRetryTimer) {
        return false;
    }
    add();
    return true;
}

void Tray::set(TrayState state, const std::wstring& tooltip)
{
    if (state == state_ && tooltip == tooltip_) {
        return;
    }
    state_ = state;
    tooltip_ = tooltip;
    NOTIFYICONDATAW d = data(NIF_ICON | NIF_TIP | NIF_SHOWTIP);
    d.hIcon = icons_[static_cast<int>(state_)];
    wcsncpy_s(d.szTip, tooltip_.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &d);
}

void Tray::notify(const std::wstring& title, const std::wstring& text)
{
    NOTIFYICONDATAW d = data(NIF_INFO);
    wcsncpy_s(d.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(d.szInfo, text.c_str(), _TRUNCATE);
    d.dwInfoFlags = NIIF_USER | NIIF_LARGE_ICON;
    Shell_NotifyIconW(NIM_MODIFY, &d);
}

bool Tray::handleTaskbarCreated(UINT msg)
{
    if (!owner_ || msg != taskbarCreated_) {
        return false;
    }
    NOTIFYICONDATAW d = data(0);
    Shell_NotifyIconW(NIM_DELETE, &d);  // in case the old one survived
    add();
    return true;
}

Tray::Command Tray::showMenu(bool running)
{
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, Open, L"Open");
    AppendMenuW(menu, MF_STRING, StartStop, running ? L"Stop" : L"Start");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, Quit, L"Quit");
    SetMenuDefaultItem(menu, Open, FALSE);

    // The owner must be in front, or the menu doesn't close when you click elsewhere.
    POINT at;
    GetCursorPos(&at);
    SetForegroundWindow(owner_);
    const UINT align = GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN;
    const int chosen = TrackPopupMenuEx(menu, align | TPM_BOTTOMALIGN | TPM_RETURNCMD | TPM_NONOTIFY, at.x, at.y, owner_, nullptr);
    PostMessageW(owner_, WM_NULL, 0, 0);
    DestroyMenu(menu);
    return static_cast<Command>(chosen);
}

}  // namespace gfx

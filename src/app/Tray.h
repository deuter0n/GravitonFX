// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include <windows.h>
#include <shellapi.h>

#include <string>

namespace gfx {

enum class TrayState { Running, Stopped, Waiting, Error };

// The notification-area icon: its picture follows the engine state, its
// tooltip says the state, and it has an Open / Start-Stop / Quit menu. Events
// arrive at the owner window as WM_APP_TRAY. UI thread only.
class Tray {
public:
    enum Command { None = 0, Open, StartStop, Quit };

    Tray() = default;
    ~Tray();
    Tray(const Tray&) = delete;
    Tray& operator=(const Tray&) = delete;

    void create(HWND owner, HINSTANCE instance);
    void set(TrayState state, const std::wstring& tooltip);
    void notify(const std::wstring& title, const std::wstring& text);  // a Windows notification

    // Call for every message the owner gets: re-adds the icon after Explorer
    // restarts. Returns true if it was that message.
    bool handleTaskbarCreated(UINT msg);

    // Shows the menu at the cursor and returns the chosen command.
    Command showMenu(bool running);

private:
    void add();
    NOTIFYICONDATAW data(UINT flags) const;

    HWND owner_ = nullptr;
    HICON icons_[4] = {};
    TrayState state_ = TrayState::Stopped;
    std::wstring tooltip_ = L"Graviton FX";
    UINT taskbarCreated_ = 0;
};

}  // namespace gfx

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include "host/WebViewHost.h"

#include <windows.h>

#include <functional>
#include <string>

namespace gfx {

// The main window: frameless (the dashboard draws its own title bar), dark,
// resizable from the sides and bottom, with the WebView filling it.
class MainWindow {
public:
    using MessageHandler = WebViewHost::MessageHandler;
    static constexpr wchar_t kClassName[] = L"GravitonFXMainWindow";

    bool create(HINSTANCE instance, MessageHandler onMessage, WebViewHost::ReadyHandler onReady);
    void show(int showCommand);
    void showAndFocus();  // from the tray or a second launch: restore and bring to front
    void hide();
    HWND hwnd() const { return hwnd_; }
    WebViewHost& web() { return web_; }

    // Called for WM_CLOSE (the title bar's close goes through the UI instead).
    std::function<void()> onCloseRequested;
    // Called when Windows asks to minimize (Win+Down, the taskbar): the app
    // hides to the tray instead.
    std::function<void()> onMinimizeRequested;
    // Called when Windows ends the session (shutdown, sign-out) or an
    // installer closes the app: save and quit right away.
    std::function<void()> onEndSession;

    // Called for WM_TIMER and messages from WM_APP up; returns true if handled.
    std::function<bool(UINT msg, WPARAM wParam, LPARAM lParam)> onAppMessage;

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handle(UINT msg, WPARAM wParam, LPARAM lParam);

    HWND hwnd_ = nullptr;
    bool minimized_ = false;
    WebViewHost web_;
};

}  // namespace gfx

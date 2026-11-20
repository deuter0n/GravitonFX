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

    bool create(HINSTANCE instance, MessageHandler onMessage, WebViewHost::ReadyHandler onReady);
    void show(int showCommand);
    void hide();
    HWND hwnd() const { return hwnd_; }
    WebViewHost& web() { return web_; }

    // Called for WM_CLOSE (the title bar's close goes through the UI instead).
    std::function<void()> onCloseRequested;

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handle(UINT msg, WPARAM wParam, LPARAM lParam);

    HWND hwnd_ = nullptr;
    WebViewHost web_;
};

}  // namespace gfx

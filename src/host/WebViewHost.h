// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include <windows.h>

#include <functional>
#include <memory>
#include <string>

namespace gfx {

// Hosts the dashboard (ui/) in a WebView2 control filling a window.
//
//  - Debug builds serve ui/ straight from the source tree, with Edge DevTools
//    on F12.
//  - Release builds serve the files embedded in the executable (mock.js is
//    not among them) and turn off DevTools, the context menu and browser
//    keys.
//  - The page is https://app.graviton/index.html; its user data lives in
//    %LOCALAPPDATA%\GravitonFX\WebView2.
//
// Everything here runs on the UI thread.
class WebViewHost {
public:
    using MessageHandler = std::function<void(const std::string& json)>;  // UTF-8 JSON from the page
    using ReadyHandler = std::function<void()>;

    WebViewHost();
    ~WebViewHost();

    // Starts creating the control; `onReady` runs once the page can receive
    // messages. Returns false (after telling the user) if WebView2 is missing.
    bool create(HWND parent, MessageHandler onMessage, ReadyHandler onReady);

    void resize(const RECT& bounds);
    void post(const std::string& json);  // UTF-8 JSON to the page
    void setVisible(bool visible);       // hidden windows also suspend the page
    void focus();                        // keyboard focus into the page

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

std::wstring localAppDataFolder();  // %LOCALAPPDATA%\GravitonFX

}  // namespace gfx

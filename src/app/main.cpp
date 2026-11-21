// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "app/AppController.h"
#include "app/MainWindow.h"

#include <windows.h>
#include <objbase.h>

#include <memory>

namespace {

constexpr UINT_PTR kTimerId = 1;
constexpr UINT kTimerMs = 33;  // telemetry at about 30 Hz

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {  // WebView2 needs an STA
        return 1;
    }

    // The page becomes ready only once the message loop runs, so the
    // controller always exists by then.
    std::unique_ptr<gfx::AppController> app;
    gfx::MainWindow window;
    const bool created = window.create(
        instance,
        [&app](const std::string& json) {
            if (app) {
                app->onUiMessage(json);
            }
        },
        [&app] {
            if (app) {
                app->onPageReady();
            }
        });
    if (!created) {
        CoUninitialize();
        return 1;
    }

    app = std::make_unique<gfx::AppController>(window, gfx::defaultSettingsPath());
    // Tray, close-to-tray and quit come with task group 9; for now closing quits.
    app->onMinimize = [&window] { ShowWindow(window.hwnd(), SW_MINIMIZE); };
    app->onClose = [&window] { DestroyWindow(window.hwnd()); };
    window.onCloseRequested = app->onClose;
    window.onAppMessage = [&app](UINT msg, WPARAM wParam, LPARAM) {
        switch (msg) {
        case WM_TIMER:
            if (wParam != kTimerId) {
                return false;
            }
            app->onTimer();
            return true;
        case gfx::WM_APP_ENGINE_STATE:
            app->onEngineState();
            return true;
        case gfx::WM_APP_DEVICES:
            app->onDevicesChanged();
            return true;
        default:
            return false;
        }
    };
    SetTimer(window.hwnd(), kTimerId, kTimerMs, nullptr);
    window.show(showCommand);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    app.reset();  // stops the engine and saves the settings
    CoUninitialize();
    return static_cast<int>(msg.wParam);
}

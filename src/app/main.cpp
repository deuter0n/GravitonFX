// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "app/AppController.h"
#include "app/Autostart.h"
#include "app/MainWindow.h"
#include "app/SingleInstance.h"

#include <windows.h>
#include <objbase.h>

#include <memory>

namespace {

constexpr UINT_PTR kTimerId = 1;
constexpr UINT kTimerMs = 33;  // telemetry at about 30 Hz

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    // One instance only: a second launch shows the first one's window.
    gfx::SingleInstance single;
    if (!single.isFirst()) {
        gfx::SingleInstance::showFirstInstance();
        return 0;
    }

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
    app->onQuit = [&window] { DestroyWindow(window.hwnd()); };
    window.onCloseRequested = [&app] { app->closeRequested(); };
    // The process may be ended soon after WM_ENDSESSION returns, so save first.
    window.onEndSession = [&app, &window] {
        app->saveNow();
        DestroyWindow(window.hwnd());
    };
    window.onAppMessage = [&app](UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_TIMER:
            if (wParam != kTimerId) {
                return app->tray().handleTimer(wParam);  // the tray's retry timer
            }
            app->onTimer();
            return true;
        case gfx::WM_APP_ENGINE_STATE: app->onEngineState(); return true;
        case gfx::WM_APP_DEVICES: app->onDevicesChanged(); return true;
        case gfx::WM_APP_SHOW: app->showWindow(); return true;
        case gfx::WM_APP_TRAY: app->onTrayEvent(lParam); return true;
        case gfx::WM_APP_UPDATE: app->onUpdateResult(); return true;
        default: return app->tray().handleTaskbarCreated(msg);  // Explorer restarted
        }
    };
    SetTimer(window.hwnd(), kTimerId, kTimerMs, nullptr);

    // Startup options. At sign-in with "Start in the tray with Windows" only the tray
    // icon appears; "Start engine on launch" starts the engine either way.
    if (app->settings().autoStart) {
        app->engine().start();
    }
    if (!(gfx::launchedAtSignIn() && app->settings().startMinimized)) {
        window.show(showCommand);
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    app.reset();  // stops the engine, saves the settings and removes the tray icon
    CoUninitialize();
    return static_cast<int>(msg.wParam);
}

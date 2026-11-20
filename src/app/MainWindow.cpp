// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "app/MainWindow.h"

#include <dwmapi.h>

#include <algorithm>

namespace gfx {

namespace {

constexpr wchar_t kWindowClass[] = L"GravitonFXMainWindow";
constexpr COLORREF kBackground = RGB(0x0c, 0x0f, 0x15);  // --bg in the UI
constexpr int kMinWidth = 1000;   // at 100% scaling
constexpr int kMinHeight = 680;

}  // namespace

bool MainWindow::create(HINSTANCE instance, MessageHandler onMessage, WebViewHost::ReadyHandler onReady)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(kBackground);
    wc.lpszClassName = kWindowClass;
    RegisterClassExW(&wc);

    // 1440 x 900 at the primary monitor's scaling, but never larger than 90%
    // of its work area; centered.
    const UINT dpi = GetDpiForSystem();
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int workW = work.right - work.left, workH = work.bottom - work.top;
    const int width = std::min(MulDiv(1440, static_cast<int>(dpi), 96), workW * 9 / 10);
    const int height = std::min(MulDiv(900, static_cast<int>(dpi), 96), workH * 9 / 10);

    hwnd_ = CreateWindowExW(0, kWindowClass, L"Graviton FX", WS_OVERLAPPEDWINDOW, work.left + (workW - width) / 2,
                            work.top + (workH - height) / 2, width, height, nullptr, nullptr, instance, this);
    if (!hwnd_) {
        return false;
    }

    // Dark frame and rounded corners, and re-evaluate the frame so the
    // caption disappears (see WM_NCCALCSIZE).
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));
    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);

    return web_.create(hwnd_, std::move(onMessage), std::move(onReady));
}

void MainWindow::show(int showCommand)
{
    ShowWindow(hwnd_, showCommand);
    web_.setVisible(true);
    SetForegroundWindow(hwnd_);
}

void MainWindow::hide()
{
    web_.setVisible(false);
    ShowWindow(hwnd_, SW_HIDE);
}

LRESULT CALLBACK MainWindow::windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE) {
        auto* self = static_cast<MainWindow*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    auto* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->handle(msg, wParam, lParam) : DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::handle(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_NCCALCSIZE:
        if (wParam) {
            // Keep the standard (invisible) resize borders on the sides and
            // bottom, but drop the caption: the dashboard's header is the
            // title bar. When maximized, keep the top inside the monitor.
            auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
            const LONG top = params->rgrc[0].top;
            DefWindowProcW(hwnd_, msg, wParam, lParam);
            params->rgrc[0].top = top;
            if (IsZoomed(hwnd_)) {
                const UINT dpi = GetDpiForWindow(hwnd_);
                params->rgrc[0].top += GetSystemMetricsForDpi(SM_CYFRAME, dpi) +
                                       GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
            }
            return 0;
        }
        break;

    case WM_SIZE: {
        RECT bounds;
        GetClientRect(hwnd_, &bounds);
        web_.resize(bounds);
        return 0;
    }

    case WM_SETFOCUS:  // the page takes keyboard input whenever the window is active
        web_.focus();
        return 0;

    case WM_GETMINMAXINFO: {
        const UINT dpi = GetDpiForWindow(hwnd_);
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = MulDiv(kMinWidth, static_cast<int>(dpi), 96);
        info->ptMinTrackSize.y = MulDiv(kMinHeight, static_cast<int>(dpi), 96);
        return 0;
    }

    case WM_DPICHANGED: {
        const auto* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                     suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }

    case WM_CLOSE:  // Alt+F4 or the taskbar's Close window
        if (onCloseRequested) {
            onCloseRequested();
            return 0;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

}  // namespace gfx

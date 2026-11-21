// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include <windows.h>

namespace gfx {

// Window messages posted to the main window (from other threads, the tray or
// a second instance of the app).
constexpr UINT WM_APP_ENGINE_STATE = WM_APP + 1;  // the engine state changed
constexpr UINT WM_APP_DEVICES = WM_APP + 2;       // devices or audio apps changed
constexpr UINT WM_APP_SHOW = WM_APP + 3;          // a second launch: show the window
constexpr UINT WM_APP_TRAY = WM_APP + 4;          // tray icon events

}  // namespace gfx

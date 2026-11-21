// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

namespace gfx {

// "Start with Windows": the per-user Run value `GravitonFX` =
// "<exe>" --autostart (no administrator rights needed).
//
// A build that runs from inside the source folder never registers itself, so
// trying a Debug or Release build doesn't add it to your sign-in. Returns
// false when the registry wasn't changed for that reason or failed.
bool setStartWithWindows(bool on);

// True when the command line has --autostart (launched at sign-in).
bool launchedAtSignIn();

}  // namespace gfx

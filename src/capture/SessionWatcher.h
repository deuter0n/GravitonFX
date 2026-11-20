// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct IMMDeviceEnumerator;

namespace gfx {

// An application with an audio session on any output device.
struct AudioApp {
    uint32_t processId = 0;
    std::wstring exe;   // e.g. "RaceSim.exe" (how the selection is saved)
    std::wstring name;  // e.g. "Race Sim" (file description, or the exe name)
    bool playing = false;
};

// Finds applications with audio sessions. The engine's control thread polls
// it every 2 s to notice the selected app starting, stopping or restarting
// (new process ID). Call from a thread that has initialized COM.
class SessionWatcher {
public:
    explicit SessionWatcher(IMMDeviceEnumerator* enumerator) : enumerator_(enumerator) {}

    // One entry per executable, sorted by name. If an exe has several
    // processes with sessions, the playing one wins. Graviton FX itself and
    // the Windows system sounds are left out.
    std::vector<AudioApp> list() const;

    // Process ID to capture for `exe` (case-insensitive), or 0 if it has no
    // audio session right now.
    uint32_t findProcess(const std::wstring& exe) const;

private:
    IMMDeviceEnumerator* enumerator_;
};

}  // namespace gfx

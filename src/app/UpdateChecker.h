// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include <windows.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace gfx {

// Asks GitHub for the latest regular release of Graviton FX, on its own
// thread (never an audio thread). Sends only the request itself, with a
// User-Agent of GravitonFX/<version>, to api.github.com over HTTPS. When done
// it posts `message` to `window`; read the outcome with result().
class UpdateChecker {
public:
    struct Result {
        enum class Kind { UpToDate, Available, Failed };
        Kind kind = Kind::Failed;
        std::string latest;  // the newer version, without the "v"
    };

    UpdateChecker(HWND window, UINT message);
    ~UpdateChecker();  // cancels a check in progress
    UpdateChecker(const UpdateChecker&) = delete;
    UpdateChecker& operator=(const UpdateChecker&) = delete;

    bool start();  // false if a check is already running
    bool busy() const { return busy_.load(); }
    Result result() const;

private:
    void run();

    HWND window_;
    UINT message_;
    std::thread thread_;
    std::atomic<bool> busy_{false};
    std::atomic<void*> session_{nullptr};  // closed to cancel
    std::atomic<bool> cancelled_{false};
    mutable std::mutex mutex_;
    Result result_;
};

}  // namespace gfx

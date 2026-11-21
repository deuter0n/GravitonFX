// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include <windows.h>

namespace gfx {

// Keeps the app to one instance per Windows session with a named mutex.
class SingleInstance {
public:
    SingleInstance();
    ~SingleInstance();
    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;

    bool isFirst() const { return first_; }

    // From a second launch: asks the first instance to show its window.
    static void showFirstInstance();

private:
    HANDLE mutex_ = nullptr;
    bool first_ = true;
};

}  // namespace gfx

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include <string>

namespace gfx {

// Why opening a capture source or the shaker device failed. Missing is not
// an error: the engine waits for the source or device to come back.
enum class OpenStatus {
    Ok,
    Missing,            // device not connected, or app not running
    InUse,              // another app holds the device exclusively
    UnsupportedFormat,  // no usable format
    BufferRefused,      // the chosen buffer size isn't supported
    Unsupported,        // this Windows version can't do it
    Failed,             // anything else
};

struct OpenResult {
    OpenStatus status = OpenStatus::Failed;
    std::wstring message;  // for the user, empty when Ok
    bool ok() const { return status == OpenStatus::Ok; }
};

}  // namespace gfx

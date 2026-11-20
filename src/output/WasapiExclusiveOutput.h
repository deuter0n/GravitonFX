// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include "output/OutputDevice.h"

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

struct IMMDeviceEnumerator;

namespace gfx {

// Shaker output through WASAPI in exclusive, event-driven mode, at the
// device's native rate and the best sample format it accepts (float32,
// int32, int24-in-32, int24, int16). The render thread is registered with
// MMCSS "Pro Audio".
class WasapiExclusiveOutput final : public OutputDevice {
public:
    explicit WasapiExclusiveOutput(IMMDeviceEnumerator* enumerator);
    ~WasapiExclusiveOutput() override;

    OpenResult open(const std::wstring& deviceId, int bufferFrames, Renderer* renderer) override;
    bool start() override;
    void stop() override;
    void close() override;

    const OutputFormat& format() const override { return format_; }
    int64_t lastCallbackTime() const override { return lastCallback_.load(std::memory_order_relaxed); }
    bool failed() const override { return failed_.load(std::memory_order_relaxed); }

private:
    struct Impl;
    void renderLoop();
    bool fillBuffer();

    std::unique_ptr<Impl> impl_;
    Renderer* renderer_ = nullptr;
    OutputFormat format_;
    std::vector<float> mono_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> failed_{false};
    std::atomic<int64_t> lastCallback_{0};
};

}  // namespace gfx

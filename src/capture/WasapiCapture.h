// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include "common/CaptureFormat.h"
#include "common/OpenResult.h"
#include "dsp/InputStage.h"
#include "dsp/SpscRing.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

struct IMMDeviceEnumerator;

namespace gfx {

// Captures audio as mono float into an SpscRing, on its own event-driven
// thread (MMCSS "Pro Audio"):
//  - openLoopback: everything playing on one render device ("System mix").
//  - openProcess:  one app and its child processes ("App"); Windows converts
//    its audio to 48 kHz float stereo for us. Needs Windows 10 2004+.
//
// All channels, including LFE, are summed to mono. When no packets arrive for
// 30 ms the source is reported idle (silence) and nothing is written. Packets
// Windows marks as silent are written as zeros. Nothing is ever played on the
// captured device.
class WasapiCapture {
public:
    explicit WasapiCapture(IMMDeviceEnumerator* enumerator);
    ~WasapiCapture();
    WasapiCapture(const WasapiCapture&) = delete;
    WasapiCapture& operator=(const WasapiCapture&) = delete;

    OpenResult openLoopback(const std::wstring& deviceId, SpscRing* ring);
    OpenResult openProcess(uint32_t processId, SpscRing* ring);
    bool start();
    void stop();
    void close();

    // Optional second ring that receives a copy of the samples (analysis).
    // Samples that don't fit are dropped there. Set before start().
    void setTap(SpscRing* tap) { tap_ = tap; }

    const CaptureFormat& format() const { return format_; }

    // For the output thread; reads atomics only.
    CaptureSignals signals() const;

    // True once the source went away or failed while capturing.
    bool failed() const { return failed_.load(std::memory_order_relaxed); }

    // Process loopback needs Windows 10 version 2004 (build 19041) or later.
    static bool processLoopbackSupported();

private:
    struct Impl;
    OpenResult finishOpen(const void* waveFormat);
    void captureLoop();
    bool drainPackets();
    void deliver(const uint8_t* data, uint32_t frames);

    std::unique_ptr<Impl> impl_;
    SpscRing* ring_ = nullptr;
    SpscRing* tap_ = nullptr;
    CaptureFormat format_;
    int containerBits_ = 32;
    std::vector<float> mono_;
    bool firstPacket_ = true;

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> failed_{false};
    std::atomic<bool> idle_{true};
    std::atomic<uint32_t> overruns_{0};
    std::atomic<uint32_t> discontinuities_{0};
};

}  // namespace gfx

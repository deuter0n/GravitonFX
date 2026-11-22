// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include "common/OpenResult.h"
#include "dsp/ChannelWriter.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace gfx {

// Produces the mono shaker signal. Called on the output device's real-time
// thread: must not allocate, lock, log or block.
class Renderer {
public:
    virtual ~Renderer() = default;
    virtual void render(float* mono, size_t frames) = 0;
};

struct OutputFormat {
    double sampleRate = 0.0;
    SampleFormat sampleFormat = SampleFormat::Float32;
    int bits = 0;          // as shown to the user: 16, 24 or 32
    bool isFloat = false;
    int channels = 0;
    int bufferFrames = 0;  // per callback

    bool operator==(const OutputFormat&) const = default;
};

// The shaker output: WASAPI exclusive or ASIO. Opened and closed by the
// control thread; render() is called from the driver's own thread.
class OutputDevice {
public:
    virtual ~OutputDevice() = default;

    virtual OpenResult open(const std::wstring& deviceId, int bufferFrames, Renderer* renderer) = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void close() = 0;

    virtual const OutputFormat& format() const = 0;

    // QueryPerformanceCounter time of the last callback, for the stall watchdog.
    virtual int64_t lastCallbackTime() const = 0;

    // True once the device reported that it went away or failed while running.
    virtual bool failed() const = 0;

    // How often the driver reported that it lost samples (ASIO resync), for
    // diagnosis. Real-time safe.
    virtual uint32_t resyncs() const { return 0; }

    // The capture source's sample rate, set before open(). The device uses it
    // when it can, so the input stage only corrects clock drift instead of
    // also converting rates; 0 means use the device's own rate.
    void setPreferredRate(double hz) { preferredRate_ = hz; }

protected:
    double preferredRate_ = 0.0;
};

}  // namespace gfx

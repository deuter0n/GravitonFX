// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include "capture/DeviceEvent.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

struct IMMDevice;
struct IMMDeviceEnumerator;

namespace gfx {

// An active Windows render endpoint (speakers, headphones, USB DAC, ...).
struct AudioDevice {
    std::wstring id;    // endpoint ID, stable across reboots
    std::wstring name;  // friendly name, e.g. "Speakers (USB Audio DAC)"
    bool isDefault = false;
    int channels = 0;   // channels of the device's own format
    int sampleRate = 0; // the device's own sample rate
};


// Lists render endpoints and reports changes. Call from a thread that has
// initialized COM (multithreaded). Notification callbacks run on a Windows
// thread: keep them short and hand the work to the control thread.
class DeviceEnumerator {
public:
    using Listener = std::function<void(DeviceEvent event, const std::wstring& deviceId)>;

    DeviceEnumerator();
    ~DeviceEnumerator();
    DeviceEnumerator(const DeviceEnumerator&) = delete;
    DeviceEnumerator& operator=(const DeviceEnumerator&) = delete;

    std::vector<AudioDevice> renderDevices() const;
    std::wstring defaultRenderId() const;

    // Looks up an active device by ID; returns false if it isn't connected.
    bool find(const std::wstring& id, AudioDevice& out) const;

    // Starts reporting device changes to `listener` (replaces any previous one).
    void setListener(Listener listener);

    IMMDeviceEnumerator* native() const;

private:
    class Notifier;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Reads the name, channel count and sample rate of an endpoint.
AudioDevice describeDevice(IMMDevice* device, const std::wstring& defaultId);

}  // namespace gfx

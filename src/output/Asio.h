// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include "output/OutputDevice.h"

#include <memory>
#include <string>
#include <vector>

namespace gfx {

// ASIO shaker output. Built only with the Steinberg ASIO SDK
// (CMake option GFX_WITH_ASIO); without it these report no drivers and the
// output fails to open.

struct AsioDriver {
    std::wstring id;    // the driver's COM class ID, e.g. "{232685C6-...}"
    std::wstring name;  // as registered, e.g. "FlexASIO"
};

bool asioSupported();  // this build includes ASIO

// Installed drivers, from HKLM\SOFTWARE\ASIO. Doesn't load any driver.
std::vector<AsioDriver> listAsioDrivers();
bool findAsioDriver(const std::wstring& id, AsioDriver& out);

// Loads the driver just long enough to read its sample rate, sample type and
// output count. Fails if another app holds the driver. The rate is the one the
// app would run it at: preferredRate when the driver takes it, else its current
// rate. The driver is left as it was.
bool describeAsioDriver(const std::wstring& id, OutputFormat& out, double preferredRate = 0.0);

// One driver at a time; all driver calls run on the output's own thread.
std::unique_ptr<OutputDevice> makeAsioOutput();

}  // namespace gfx

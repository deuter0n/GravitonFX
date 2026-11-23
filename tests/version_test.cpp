// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include <doctest/doctest.h>

#include <regex>
#include <string>

TEST_CASE("the version is set from CMake")
{
    // Only CMakeLists.txt names the version; this checks it arrived in the form
    // the update check compares (major.minor.patch).
    CHECK(std::regex_match(std::string(GFX_VERSION), std::regex(R"(\d+\.\d+\.\d+)")));
}

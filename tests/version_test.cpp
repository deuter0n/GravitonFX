// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include <doctest/doctest.h>

#include <string_view>

TEST_CASE("the version is set from CMake")
{
    CHECK(std::string_view(GFX_VERSION) == "0.1.1");
}

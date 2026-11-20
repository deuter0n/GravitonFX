# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 deuter0n

find_package(doctest CONFIG REQUIRED)

add_executable(gfx_tests
    main.cpp
    placeholder_test.cpp)
target_link_libraries(gfx_tests PRIVATE gfx_options doctest::doctest)
# Use the real <iosfwd> instead of doctest's own std forward declarations,
# which newer MSVC standard libraries reject.
target_compile_definitions(gfx_tests PRIVATE DOCTEST_CONFIG_USE_STD_HEADERS)

add_test(NAME gfx_tests COMMAND gfx_tests)

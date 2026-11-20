# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 deuter0n

find_package(doctest CONFIG REQUIRED)

add_executable(gfx_tests
    main.cpp
    version_test.cpp
    spsc_ring_test.cpp
    butterworth_test.cpp
    filter_chain_test.cpp
    limiter_test.cpp
    resampler_test.cpp
    drift_test.cpp
    test_signals_test.cpp
    channel_writer_test.cpp
    engine_core_test.cpp
    analyzer_test.cpp)
target_link_libraries(gfx_tests PRIVATE gfx_engine doctest::doctest)
# Use the real <iosfwd> instead of doctest's own std forward declarations,
# which newer MSVC standard libraries reject.
target_compile_definitions(gfx_tests PRIVATE DOCTEST_CONFIG_USE_STD_HEADERS)

add_test(NAME gfx_tests COMMAND gfx_tests)

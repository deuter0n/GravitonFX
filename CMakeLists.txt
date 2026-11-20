# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 deuter0n

cmake_minimum_required(VERSION 3.25)

project(GravitonFX VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static C runtime, so the exe runs without the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Shared compile settings for every target in the project.
add_library(gfx_options INTERFACE)
target_compile_options(gfx_options INTERFACE /W4 /permissive- /utf-8 /EHsc)
target_compile_definitions(gfx_options INTERFACE
    UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES
    GFX_VERSION="${PROJECT_VERSION}")
target_include_directories(gfx_options INTERFACE src)

# Signal processing: no Windows dependencies, unit-tested headless.
add_library(gfx_dsp STATIC
    src/dsp/Butterworth.cpp
    src/dsp/FilterChain.cpp
    src/dsp/Limiter.cpp
    src/dsp/DriftController.cpp
    src/dsp/InputStage.cpp
    src/dsp/TestSignals.cpp
    src/dsp/ChannelWriter.cpp)
target_link_libraries(gfx_dsp PUBLIC gfx_options)

# The app.
add_executable(GravitonFX WIN32
    src/app/main.cpp)
target_link_libraries(GravitonFX PRIVATE gfx_options dwmapi)

# Unit tests.
enable_testing()
add_subdirectory(tests)

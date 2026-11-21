# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 deuter0n

cmake_minimum_required(VERSION 3.25)

project(GravitonFX VERSION 0.1.0 LANGUAGES CXX RC)

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

# Engine: state machine and render path. No Windows dependencies, so it is
# unit-tested with fake devices.
add_library(gfx_engine STATIC
    src/engine/RenderPath.cpp
    src/engine/EngineCore.cpp
    src/analysis/Analyzer.cpp)
target_link_libraries(gfx_engine PUBLIC gfx_dsp)

# Windows audio: device lists, capture and shaker output.
find_package(wil CONFIG REQUIRED)
add_library(gfx_audio STATIC
    src/capture/DeviceEnumerator.cpp
    src/capture/SessionWatcher.cpp
    src/capture/WasapiCapture.cpp
    src/output/WasapiExclusiveOutput.cpp)
target_link_libraries(gfx_audio PUBLIC gfx_dsp WIL::WIL ole32 avrt propsys mmdevapi version)

# Settings and presets: plain C++ and JSON, unit-tested.
find_package(nlohmann_json CONFIG REQUIRED)
add_library(gfx_host_core STATIC
    src/host/Settings.cpp)
target_link_libraries(gfx_host_core PUBLIC gfx_options nlohmann_json::nlohmann_json)

# The engine on real devices: control thread + Windows backend.
add_library(gfx_runtime STATIC
    src/engine/Engine.cpp
    src/engine/WindowsBackend.cpp)
target_link_libraries(gfx_runtime PUBLIC gfx_engine gfx_audio)

# Console tool for checking the audio layers by hand (not shipped).
add_executable(gfxprobe src/tools/probe.cpp)
target_link_libraries(gfxprobe PRIVATE gfx_runtime)

# The app.
find_package(unofficial-webview2 CONFIG REQUIRED)
include(cmake/UiResources.cmake)
add_executable(GravitonFX WIN32
    src/app/main.cpp
    src/app/AppController.cpp
    src/app/MainWindow.cpp
    src/host/WebViewHost.cpp)
target_link_libraries(GravitonFX PRIVATE gfx_runtime gfx_host_core unofficial::webview2::webview2 dwmapi shlwapi)
# Debug serves ui/ from the source tree; Release embeds it.
target_compile_definitions(GravitonFX PRIVATE $<$<CONFIG:Debug>:GFX_UI_DIR=L"${CMAKE_SOURCE_DIR}/ui">)
gfx_embed_ui(GravitonFX "${CMAKE_SOURCE_DIR}/ui")

# Unit tests.
enable_testing()
add_subdirectory(tests)

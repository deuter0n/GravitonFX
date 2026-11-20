// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "engine/Engine.h"

#include "engine/EngineCore.h"
#include "engine/WindowsBackend.h"

#include <windows.h>
#include <objbase.h>
#include <wil/com.h>

#include <chrono>
#include <future>

namespace gfx {

namespace {
constexpr auto kTelemetryPeriod = std::chrono::milliseconds(33);
constexpr auto kTickPeriod = std::chrono::milliseconds(100);
}  // namespace

Engine::Engine()
{
    thread_ = std::thread([this] { run(); });
    analysisThread_ = std::thread([this] { analyze(); });
}

Engine::~Engine()
{
    {
        std::lock_guard lock(mutex_);
        quit_ = true;
    }
    wake_.notify_all();
    analysisWake_.notify_all();
    analysisThread_.join();
    thread_.join();
}

// Levels, spectra and no-signal detection, about 30 times a second. Reads
// the taps the output thread writes; if it falls behind, the taps simply
// drop samples, so it can never delay audio.
void Engine::analyze()
{
    Analyzer analyzer;
    bool prepared = false;
    std::vector<float> block(4096);
    auto last = std::chrono::steady_clock::now();
    std::unique_lock lock(mutex_);
    while (!quit_) {
        analysisWake_.wait_for(lock, kTelemetryPeriod, [this] { return quit_; });
        const double rate = state_.outputFormat.sampleRate;
        const bool running = state_.phase == Phase::Running;
        lock.unlock();

        if (rate > 0.0 && (!prepared || rate != analyzer.sampleRate())) {
            analyzer.prepare(rate);
            prepared = true;
        }
        if (!prepared) {  // no output open yet: nothing to analyze
            inTap_.clear();
            outTap_.clear();
            lock.lock();
            continue;
        }
        size_t n;
        while ((n = inTap_.read(block.data(), block.size())) > 0) {
            analyzer.pushInput(block.data(), n);
        }
        while ((n = outTap_.read(block.data(), block.size())) > 0) {
            analyzer.pushOutput(block.data(), n);
        }
        if (!running) {
            analyzer.resetNoSignal();
        }
        const auto now = std::chrono::steady_clock::now();
        const auto snap = analyzer.snapshot(std::chrono::duration<double>(now - last).count());
        last = now;

        lock.lock();
        analysis_ = snap;
        analysis_.noSignal = snap.noSignal && running;
    }
}

void Engine::setStateListener(StateListener listener)
{
    std::lock_guard lock(mutex_);
    stateListener_ = std::move(listener);
}

void Engine::setDevicesListener(DevicesListener listener)
{
    std::lock_guard lock(mutex_);
    devicesListener_ = std::move(listener);
}

void Engine::post(Command command)
{
    {
        std::lock_guard lock(mutex_);
        queue_.push_back(std::move(command));
    }
    wake_.notify_one();
}

template <class T> T Engine::ask(std::function<T()> query)
{
    auto promise = std::make_shared<std::promise<T>>();
    auto result = promise->get_future();
    post([promise, query] { promise->set_value(query()); });
    return result.get();
}

void Engine::apply(const EngineSettings& settings)
{
    post([this, settings] { core_->apply(settings); });
}
void Engine::start() { post([this] { core_->start(); }); }
void Engine::stop() { post([this] { core_->stop(); }); }
void Engine::pulseStart() { post([this] { core_->pulseStart(); }); }
void Engine::pulseStop() { post([this] { core_->pulseStop(); }); }
void Engine::sweep() { post([this] { core_->sweep(); }); }

EngineState Engine::state() const
{
    std::lock_guard lock(mutex_);
    return state_;
}

Telemetry Engine::telemetry() const
{
    std::lock_guard lock(mutex_);
    Telemetry t = telemetry_;
    t.inDb = analysis_.inDb;
    t.outDb = analysis_.outDb;
    t.noSignal = analysis_.noSignal;
    t.spectrumIn = analysis_.in;
    t.spectrumOut = analysis_.out;
    return t;
}

std::vector<AudioDevice> Engine::renderDevices()
{
    return ask<std::vector<AudioDevice>>([this] { return backend_->devices().renderDevices(); });
}

std::vector<AudioApp> Engine::audioApps()
{
    return ask<std::vector<AudioApp>>([this] { return backend_->sessions().list(); });
}

void Engine::run()
{
    const auto com = wil::CoInitializeEx(COINIT_MULTITHREADED);
    WindowsBackend backend;
    EngineCore core(backend);
    core.setTaps(&inTap_, &outTap_);
    backend_ = &backend;
    core_ = &core;

    core.setStateListener([this](const EngineState& s) {
        StateListener listener;
        {
            std::lock_guard lock(mutex_);
            state_ = s;
            listener = stateListener_;
        }
        if (listener) {
            listener(s);
        }
    });

    // Device changes arrive on a Windows thread: queue them for this one.
    backend.devices().setListener([this](DeviceEvent event, const std::wstring& id) {
        post([this, event, id] {
            core_->onDeviceEvent(event, id);
            DevicesListener listener;
            {
                std::lock_guard lock(mutex_);
                listener = devicesListener_;
            }
            if (listener) {
                listener();
            }
        });
    });

    auto nextTick = std::chrono::steady_clock::now();
    std::unique_lock lock(mutex_);
    while (!quit_) {
        wake_.wait_for(lock, kTelemetryPeriod, [this] { return quit_ || !queue_.empty(); });
        while (!queue_.empty() && !quit_) {
            Command command = std::move(queue_.front());
            queue_.pop_front();
            lock.unlock();
            command();
            lock.lock();
        }
        if (quit_) {
            break;
        }
        lock.unlock();
        if (std::chrono::steady_clock::now() >= nextTick) {
            core.tick();
            nextTick += kTickPeriod;
        }
        const Telemetry t = core.telemetry();
        lock.lock();
        telemetry_ = t;
    }
    lock.unlock();

    backend.devices().setListener(nullptr);
    core.stop();
    core_ = nullptr;
    backend_ = nullptr;
}

}  // namespace gfx

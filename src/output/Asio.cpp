// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n
//
// ASIO output. With GFX_WITH_ASIO this uses the Steinberg ASIO SDK's driver
// interface (iasiodrv.h, asio.h; the SDK is used under its GPLv3 option);
// without it, the functions below are stubs.

#include "output/Asio.h"

#include <windows.h>

#include <vector>

#ifdef GFX_WITH_ASIO
namespace gfx {

namespace {

constexpr wchar_t kAsioKey[] = L"SOFTWARE\\ASIO";

// Lists HKLM\SOFTWARE\ASIO\<name> keys and their CLSID values.
std::vector<AsioDriver> readRegistry()
{
    std::vector<AsioDriver> drivers;
    HKEY root = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, kAsioKey, 0, KEY_READ, &root) != ERROR_SUCCESS) {
        return drivers;
    }
    for (DWORD i = 0;; ++i) {
        wchar_t name[256];
        DWORD nameLength = 256;
        if (RegEnumKeyExW(root, i, name, &nameLength, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) {
            break;
        }
        wchar_t clsid[64];
        DWORD size = sizeof(clsid);
        if (RegGetValueW(root, name, L"CLSID", RRF_RT_REG_SZ, nullptr, clsid, &size) == ERROR_SUCCESS) {
            drivers.push_back({clsid, name});
        }
    }
    RegCloseKey(root);
    return drivers;
}

}  // namespace

}  // namespace gfx
#endif

#ifndef GFX_WITH_ASIO

namespace gfx {

bool asioSupported() { return false; }
std::vector<AsioDriver> listAsioDrivers() { return {}; }
bool findAsioDriver(const std::wstring&, AsioDriver&) { return false; }
bool describeAsioDriver(const std::wstring&, OutputFormat&) { return false; }

namespace {

class NoAsioOutput final : public OutputDevice {
public:
    OpenResult open(const std::wstring&, int, Renderer*) override
    {
        return {OpenStatus::Failed, L"This build has no ASIO. Use WASAPI Exclusive."};
    }
    bool start() override { return false; }
    void stop() override {}
    void close() override {}
    const OutputFormat& format() const override { return format_; }
    int64_t lastCallbackTime() const override { return 0; }
    bool failed() const override { return false; }

private:
    OutputFormat format_;
};

}  // namespace

std::unique_ptr<OutputDevice> makeAsioOutput() { return std::make_unique<NoAsioOutput>(); }

}  // namespace gfx

#else  // GFX_WITH_ASIO

#include <objbase.h>

#include "iasiodrv.h"

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <thread>

namespace gfx {

namespace {

// ASIO drivers are in-process COM objects that expect to be created and
// called on one single-threaded apartment thread, often with a message loop
// and a window handle. This thread provides that: run() executes a function
// on it and waits.
class DriverThread {
public:
    DriverThread()
    {
        wake_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        std::promise<void> started;
        auto ready = started.get_future();
        thread_ = std::thread([this, &started] { loop(started); });
        ready.wait();
    }

    ~DriverThread()
    {
        {
            std::lock_guard lock(mutex_);
            quit_ = true;
        }
        SetEvent(wake_);
        thread_.join();
        CloseHandle(wake_);
    }

    DriverThread(const DriverThread&) = delete;
    DriverThread& operator=(const DriverThread&) = delete;

    HWND window() const { return window_; }

    void run(std::function<void()> work)
    {
        std::packaged_task<void()> task(std::move(work));
        auto done = task.get_future();
        {
            std::lock_guard lock(mutex_);
            tasks_.push_back(std::move(task));
        }
        SetEvent(wake_);
        done.get();
    }

private:
    void loop(std::promise<void>& started)
    {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        WNDCLASSW wc{};
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"GravitonFXAsio";
        RegisterClassW(&wc);  // fails harmlessly if already registered
        window_ = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPED, 0, 0, 0, 0, nullptr, nullptr, wc.hInstance, nullptr);
        started.set_value();

        for (;;) {
            // MWMO_INPUTAVAILABLE: also wake for messages already queued (a driver
            // may have peeked at them during a task).
            const DWORD r = MsgWaitForMultipleObjectsEx(1, &wake_, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            if (r == WAIT_OBJECT_0) {
                std::deque<std::packaged_task<void()>> tasks;
                bool quit;
                {
                    std::lock_guard lock(mutex_);
                    tasks.swap(tasks_);
                    quit = quit_;
                }
                for (auto& task : tasks) {
                    task();
                }
                if (quit) {
                    break;
                }
            } else {
                MSG msg;
                while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
            }
        }
        DestroyWindow(window_);
        CoUninitialize();
    }

    std::thread thread_;
    HANDLE wake_ = nullptr;
    HWND window_ = nullptr;
    std::mutex mutex_;
    std::deque<std::packaged_task<void()>> tasks_;
    bool quit_ = false;
};

bool toSampleFormat(ASIOSampleType type, OutputFormat& f)
{
    switch (type) {
    case ASIOSTFloat32LSB: f.sampleFormat = SampleFormat::Float32; f.bits = 32; f.isFloat = true; return true;
    case ASIOSTInt32LSB: f.sampleFormat = SampleFormat::Int32; f.bits = 32; f.isFloat = false; return true;
    case ASIOSTInt24LSB: f.sampleFormat = SampleFormat::Int24; f.bits = 24; f.isFloat = false; return true;
    case ASIOSTInt16LSB: f.sampleFormat = SampleFormat::Int16; f.bits = 16; f.isFloat = false; return true;
    default: return false;  // big-endian, float64 and the right-aligned 32-bit variants
    }
}

// The driver accepts a buffer size between min and max, either any power of
// two (granularity -1), steps of `granularity`, or only its preferred size (0).
bool bufferSizeAllowed(long size, long min, long max, long preferred, long granularity)
{
    if (size < min || size > max) {
        return false;
    }
    if (granularity == -1) {
        return (size & (size - 1)) == 0;
    }
    if (granularity == 0) {
        return size == preferred;
    }
    return (size - min) % granularity == 0;
}

class AsioOutput final : public OutputDevice {
public:
    AsioOutput() = default;
    ~AsioOutput() override { close(); }

    OpenResult open(const std::wstring& id, int bufferFrames, Renderer* renderer) override
    {
        OpenResult result;
        thread_.run([&] { result = openOnThread(id, bufferFrames, renderer); });
        return result;
    }

    bool start() override
    {
        bool ok = false;
        thread_.run([&] {
            if (driver_) {
                // The stall watchdog measures from here until the first callback.
                LARGE_INTEGER now;
                QueryPerformanceCounter(&now);
                lastCallback_.store(now.QuadPart, std::memory_order_relaxed);
                active_.store(this);
                ok = driver_->start() == ASE_OK;
                if (!ok) {
                    active_.store(nullptr);
                }
            }
        });
        return ok;
    }

    void stop() override
    {
        thread_.run([&] {
            if (driver_) {
                driver_->stop();
            }
        });
    }

    void close() override
    {
        thread_.run([&] { release(); });
    }

    const OutputFormat& format() const override { return format_; }
    int64_t lastCallbackTime() const override { return lastCallback_.load(std::memory_order_relaxed); }
    bool failed() const override { return failed_.load(std::memory_order_relaxed); }

    // Reads the driver's format without creating buffers.
    bool describe(const std::wstring& id, OutputFormat& out)
    {
        bool ok = false;
        thread_.run([&] {
            if (load(id).ok() && readFormat(out)) {
                ok = true;
            }
            release();
        });
        return ok;
    }

private:
    // --- on the driver thread ---

    OpenResult load(const std::wstring& id)
    {
        AsioDriver info;
        if (!findAsioDriver(id, info)) {
            return {OpenStatus::Missing, L"Not connected."};
        }
        CLSID clsid;
        if (FAILED(CLSIDFromString(id.c_str(), &clsid)) ||
            FAILED(CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER, clsid, reinterpret_cast<void**>(&driver_))) ||
            !driver_) {
            driver_ = nullptr;
            return {OpenStatus::Failed, L"Can't open. Reconnect it or pick another device."};
        }
        if (!driver_->init(thread_.window())) {
            // Most often another app holds the driver (ASIO allows one at a time).
            driver_->Release();
            driver_ = nullptr;
            return {OpenStatus::InUse, L"In use by another app. Close it or pick another device."};
        }
        return {OpenStatus::Ok, {}};
    }

    bool readFormat(OutputFormat& f)
    {
        long inputs = 0, outputs = 0;
        ASIOSampleRate rate = 0;
        if (driver_->getChannels(&inputs, &outputs) != ASE_OK || outputs < 1 || driver_->getSampleRate(&rate) != ASE_OK) {
            return false;
        }
        ASIOChannelInfo channel{};
        channel.channel = 0;
        channel.isInput = ASIOFalse;
        if (driver_->getChannelInfo(&channel) != ASE_OK || !toSampleFormat(channel.type, f)) {
            return false;
        }
        f.sampleRate = rate;
        f.channels = static_cast<int>(outputs);
        return true;
    }

    OpenResult openOnThread(const std::wstring& id, int bufferFrames, Renderer* renderer)
    {
        release();
        failed_.store(false);
        if (OpenResult r = load(id); !r.ok()) {
            return r;
        }
        OutputFormat f;
        if (!readFormat(f)) {
            release();
            return {OpenStatus::UnsupportedFormat, L"No supported format. Pick another device."};
        }
        long min = 0, max = 0, preferred = 0, granularity = 0;
        if (driver_->getBufferSize(&min, &max, &preferred, &granularity) != ASE_OK ||
            !bufferSizeAllowed(bufferFrames, min, max, preferred, granularity)) {
            release();
            return {OpenStatus::BufferRefused, L"Buffer size not supported. Pick a larger one."};
        }

        // Buffers for outputs 1 and 2 only; the driver keeps every other output silent.
        used_ = f.channels >= 2 ? 2 : 1;
        for (int i = 0; i < used_; ++i) {
            buffers_[i] = {};
            buffers_[i].isInput = ASIOFalse;
            buffers_[i].channelNum = i;
        }
        static ASIOCallbacks callbacks{&onBufferSwitch, &onSampleRateChanged, &onMessage, &onBufferSwitchTimeInfo};
        if (driver_->createBuffers(buffers_, used_, bufferFrames, &callbacks) != ASE_OK) {
            release();
            return {OpenStatus::BufferRefused, L"Buffer size not supported. Pick a larger one."};
        }
        buffersCreated_ = true;
        for (int i = 0; i < used_; ++i) {  // both halves start silent
            for (void* half : buffers_[i].buffers) {
                std::memset(half, 0, static_cast<size_t>(bufferFrames) * bytesPerSample(f.sampleFormat));
            }
        }
        postOutput_ = driver_->outputReady() == ASE_OK;  // the driver wants outputReady() after each buffer

        f.bufferFrames = bufferFrames;
        format_ = f;
        renderer_ = renderer;
        mono_.assign(static_cast<size_t>(bufferFrames), 0.0f);
        return {OpenStatus::Ok, {}};
    }

    void release()
    {
        // Stop the driver before detaching, so no callback runs on what is
        // about to be disposed.
        if (driver_) {
            driver_->stop();
        }
        if (active_.load() == this) {
            active_.store(nullptr);
        }
        if (driver_) {
            if (buffersCreated_) {
                driver_->disposeBuffers();
            }
            driver_->Release();
        }
        driver_ = nullptr;
        buffersCreated_ = false;
        renderer_ = nullptr;
    }

    // --- on the driver's audio thread (real-time: no locks or allocation) ---

    void process(long index)
    {
        const size_t frames = mono_.size();
        if (renderer_) {
            renderer_->render(mono_.data(), frames);
        }
        for (int i = 0; i < used_; ++i) {
            writeChannel(mono_.data(), buffers_[i].buffers[index], frames, format_.sampleFormat);
        }
        if (postOutput_) {
            driver_->outputReady();
        }
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        lastCallback_.store(now.QuadPart, std::memory_order_relaxed);
    }

    // ASIO callbacks carry no context pointer; only one driver runs at a time.
    static inline std::atomic<AsioOutput*> active_{nullptr};

    static void onBufferSwitch(long index, ASIOBool)
    {
        if (AsioOutput* self = active_.load(std::memory_order_acquire)) {
            self->process(index);
        }
    }

    static ASIOTime* onBufferSwitchTimeInfo(ASIOTime*, long index, ASIOBool directProcess)
    {
        onBufferSwitch(index, directProcess);
        return nullptr;
    }

    static void onSampleRateChanged(ASIOSampleRate)
    {
        if (AsioOutput* self = active_.load()) {
            self->failed_.store(true);  // the engine reopens at the new rate
        }
    }

    static long onMessage(long selector, long value, void*, double*)
    {
        switch (selector) {
        case kAsioSelectorSupported:
            return value == kAsioEngineVersion || value == kAsioResetRequest || value == kAsioResyncRequest ||
                   value == kAsioLatenciesChanged || value == kAsioSupportsTimeInfo
                       ? 1L
                       : 0L;
        case kAsioEngineVersion:
            return 2L;
        case kAsioResetRequest:  // the driver changed its settings: reopen
            if (AsioOutput* self = active_.load()) {
                self->failed_.store(true);
            }
            return 1L;
        case kAsioResyncRequest:
        case kAsioLatenciesChanged:
            return 1L;
        case kAsioSupportsTimeInfo:
            return 0L;  // plain bufferSwitch is enough
        default:
            return 0L;
        }
    }

    DriverThread thread_;
    IASIO* driver_ = nullptr;
    ASIOBufferInfo buffers_[2]{};
    int used_ = 0;
    bool buffersCreated_ = false;
    bool postOutput_ = false;
    Renderer* renderer_ = nullptr;
    OutputFormat format_;
    std::vector<float> mono_;
    std::atomic<bool> failed_{false};
    std::atomic<int64_t> lastCallback_{0};
};

}  // namespace

bool asioSupported() { return true; }

std::vector<AsioDriver> listAsioDrivers() { return readRegistry(); }

bool findAsioDriver(const std::wstring& id, AsioDriver& out)
{
    for (const auto& d : readRegistry()) {
        if (_wcsicmp(d.id.c_str(), id.c_str()) == 0) {
            out = d;
            return true;
        }
    }
    return false;
}

bool describeAsioDriver(const std::wstring& id, OutputFormat& out)
{
    AsioOutput probe;
    return probe.describe(id, out);
}

std::unique_ptr<OutputDevice> makeAsioOutput() { return std::make_unique<AsioOutput>(); }

}  // namespace gfx

#endif  // GFX_WITH_ASIO

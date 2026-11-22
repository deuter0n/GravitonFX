// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "capture/WasapiCapture.h"

#include "capture/DeviceEnumerator.h"

#include <windows.h>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <avrt.h>
#include <ks.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <wil/com.h>
#include <wil/resource.h>
#include <wrl/implements.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace gfx {

namespace {

constexpr DWORD kIdleTimeoutMs = 30;  // no packets this long = silence
constexpr size_t kMaxChunkFrames = 4096;
// Windows' capture buffer: room for 100 ms. Every event still reads all
// packets at once, so this adds no latency; if the capture thread wakes up
// late, the audio waits here instead of being lost.
constexpr REFERENCE_TIME kCaptureBuffer = 1'000'000;

OpenResult fail(OpenStatus status, std::wstring message) { return {status, std::move(message)}; }

// Completion handler for ActivateAudioInterfaceAsync. It must be agile (free
// threaded), which FtmBase provides.
class ActivationHandler
    : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
                                          Microsoft::WRL::FtmBase, IActivateAudioInterfaceCompletionHandler> {
public:
    ActivationHandler() { done.create(); }

    STDMETHOD(ActivateCompleted)(IActivateAudioInterfaceAsyncOperation* operation) override
    {
        wil::com_ptr<IUnknown> unknown;
        HRESULT activateResult = E_FAIL;
        HRESULT hr = operation->GetActivateResult(&activateResult, &unknown);
        result = FAILED(hr) ? hr : activateResult;
        if (SUCCEEDED(result)) {
            result = unknown->QueryInterface(IID_PPV_ARGS(&client));
        }
        done.SetEvent();
        return S_OK;
    }

    wil::unique_event_nothrow done;
    HRESULT result = E_FAIL;
    wil::com_ptr<IAudioClient> client;
};

}  // namespace

struct WasapiCapture::Impl {
    wil::com_ptr<IMMDeviceEnumerator> enumerator;
    wil::com_ptr<IAudioClient> client;
    wil::com_ptr<IAudioCaptureClient> captureClient;
    wil::unique_event_nothrow event;
};

WasapiCapture::WasapiCapture(IMMDeviceEnumerator* enumerator) : impl_(std::make_unique<Impl>())
{
    impl_->enumerator = enumerator;
}

WasapiCapture::~WasapiCapture() { close(); }

bool WasapiCapture::processLoopbackSupported()
{
    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
    const auto rtlGetVersion =
        reinterpret_cast<RtlGetVersionFn>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
    OSVERSIONINFOW info{sizeof(info)};
    return rtlGetVersion && rtlGetVersion(&info) == 0 && info.dwBuildNumber >= 19041;
}

OpenResult WasapiCapture::openLoopback(const std::wstring& deviceId, SpscRing* ring)
{
    close();
    ring_ = ring;

    wil::com_ptr<IMMDevice> device;
    DWORD state = 0;
    if (FAILED(impl_->enumerator->GetDevice(deviceId.c_str(), &device)) || FAILED(device->GetState(&state)) ||
        state != DEVICE_STATE_ACTIVE) {
        return fail(OpenStatus::Missing, L"Not connected.");
    }
    HRESULT hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, impl_->client.put_void());
    if (FAILED(hr)) {
        return fail(OpenStatus::Failed, L"Can't capture this device. Pick another one.");
    }

    // Loopback runs at the device's shared-mode mix format.
    wil::unique_cotaskmem_ptr<WAVEFORMATEX> mix;
    if (FAILED(impl_->client->GetMixFormat(wil::out_param(mix)))) {
        close();
        return fail(OpenStatus::Failed, L"Can't capture this device. Pick another one.");
    }
    hr = impl_->client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                   kCaptureBuffer, 0, mix.get(), nullptr);
    if (FAILED(hr)) {
        close();
        return fail(hr == AUDCLNT_E_DEVICE_INVALIDATED ? OpenStatus::Missing : OpenStatus::Failed,
                    L"Can't capture this device. Pick another one.");
    }
    REFERENCE_TIME period = 100'000;  // 10 ms
    impl_->client->GetDevicePeriod(&period, nullptr);
    format_.packetFrames = static_cast<size_t>(mix->nSamplesPerSec * period / 10'000'000);
    return finishOpen(mix.get());
}

OpenResult WasapiCapture::openProcess(uint32_t processId, SpscRing* ring)
{
    close();
    ring_ = ring;
    if (!processLoopbackSupported()) {
        return fail(OpenStatus::Unsupported, L"Needs Windows 10 2004 or later. Use System mix.");
    }

    AUDIOCLIENT_ACTIVATION_PARAMS params{};
    params.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
    params.ProcessLoopbackParams.TargetProcessId = processId;
    params.ProcessLoopbackParams.ProcessLoopbackMode = PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
    PROPVARIANT activation{};
    activation.vt = VT_BLOB;
    activation.blob.cbSize = sizeof(params);
    activation.blob.pBlobData = reinterpret_cast<BYTE*>(&params);

    auto handler = Microsoft::WRL::Make<ActivationHandler>();
    wil::com_ptr<IActivateAudioInterfaceAsyncOperation> operation;
    HRESULT hr = ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, __uuidof(IAudioClient),
                                             &activation, handler.Get(), &operation);
    if (FAILED(hr) || !handler->done.wait(5000) || FAILED(handler->result)) {
        return fail(OpenStatus::Failed, L"Can't capture this app. Restart it or pick another.");
    }
    impl_->client = handler->client;

    // Windows converts the app's audio to this format, including downmixing.
    WAVEFORMATEX wf{};
    wf.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    wf.nChannels = 2;
    wf.nSamplesPerSec = 48000;
    wf.wBitsPerSample = 32;
    wf.nBlockAlign = wf.nChannels * wf.wBitsPerSample / 8;
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
    hr = impl_->client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                   AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK, kCaptureBuffer, 0, &wf,
                                   nullptr);
    if (FAILED(hr)) {
        close();
        return fail(OpenStatus::Failed, L"Can't capture this app. Restart it or pick another.");
    }
    format_.packetFrames = 480;  // process loopback has no device period; ~10 ms
    return finishOpen(&wf);
}

OpenResult WasapiCapture::finishOpen(const void* waveFormat)
{
    const auto* wf = static_cast<const WAVEFORMATEX*>(waveFormat);
    bool isFloat = wf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
    int validBits = wf->wBitsPerSample;
    if (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(wf);
        isFloat = ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        validBits = ext->Samples.wValidBitsPerSample;
    }
    format_.sampleRate = wf->nSamplesPerSec;
    format_.channels = wf->nChannels;
    format_.bits = validBits;
    format_.isFloat = isFloat;
    containerBits_ = wf->wBitsPerSample;
    if (!isFloat && containerBits_ != 16 && containerBits_ != 32) {
        close();
        return fail(OpenStatus::UnsupportedFormat, L"Unsupported format. Pick another device.");
    }

    impl_->event.create();
    if (FAILED(impl_->client->SetEventHandle(impl_->event.get())) ||
        FAILED(impl_->client->GetService(__uuidof(IAudioCaptureClient), impl_->captureClient.put_void()))) {
        close();
        return fail(OpenStatus::Failed, L"Can't capture this device. Pick another one.");
    }
    mono_.assign(kMaxChunkFrames, 0.0f);  // allocated here, never on the capture thread
    return {OpenStatus::Ok, {}};
}

bool WasapiCapture::start()
{
    if (!impl_->client || running_) {
        return false;
    }
    failed_ = false;
    idle_ = true;
    firstPacket_ = true;
    running_ = true;
    thread_ = std::thread([this] { captureLoop(); });
    if (FAILED(impl_->client->Start())) {
        stop();
        return false;
    }
    return true;
}

void WasapiCapture::stop()
{
    if (!running_) {
        return;
    }
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
    if (impl_->client) {
        impl_->client->Stop();
    }
}

void WasapiCapture::close()
{
    stop();
    impl_->captureClient.reset();
    impl_->client.reset();
    impl_->event.reset();
}

CaptureSignals WasapiCapture::signals() const
{
    CaptureSignals s;
    s.idle = idle_.load(std::memory_order_acquire);
    s.overruns = overruns_.load(std::memory_order_relaxed);
    s.discontinuities = discontinuities_.load(std::memory_order_relaxed);
    return s;
}

void WasapiCapture::captureLoop()
{
    const auto com = wil::CoInitializeEx_failfast(COINIT_MULTITHREADED);
    DWORD taskIndex = 0;
    HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

    while (running_) {
        if (WaitForSingleObject(impl_->event.get(), kIdleTimeoutMs) != WAIT_OBJECT_0) {
            idle_.store(true, std::memory_order_release);  // nothing is playing
            continue;
        }
        if (!drainPackets()) {
            failed_ = true;
            break;
        }
    }

    if (mmcss) {
        AvRevertMmThreadCharacteristics(mmcss);
    }
}

bool WasapiCapture::drainPackets()
{
    UINT32 next = 0;
    while (true) {
        if (FAILED(impl_->captureClient->GetNextPacketSize(&next))) {
            return false;  // device unplugged or app gone
        }
        if (next == 0) {
            return true;
        }
        BYTE* data = nullptr;
        UINT32 frames = 0;
        DWORD flags = 0;
        if (FAILED(impl_->captureClient->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) {
            return false;
        }
        // Windows flags the first packet after Start as a discontinuity; that
        // one isn't a glitch.
        if ((flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) && !firstPacket_) {
            discontinuities_.fetch_add(1, std::memory_order_relaxed);
        }
        firstPacket_ = false;
        deliver((flags & AUDCLNT_BUFFERFLAGS_SILENT) ? nullptr : data, frames);
        impl_->captureClient->ReleaseBuffer(frames);
        idle_.store(false, std::memory_order_release);
    }
}

// Sums all channels to mono and writes to the ring (and the tap). A null
// `data` means a silent packet.
void WasapiCapture::deliver(const uint8_t* data, uint32_t frames)
{
    const int channels = format_.channels;
    const size_t bytesPerFrame = static_cast<size_t>(channels) * containerBits_ / 8;
    while (frames > 0) {
        const uint32_t n = std::min<uint32_t>(frames, static_cast<uint32_t>(mono_.size()));
        for (uint32_t i = 0; i < n; ++i) {
            float sum = 0.0f;
            if (data) {
                const uint8_t* frame = data + i * bytesPerFrame;
                for (int ch = 0; ch < channels; ++ch) {
                    if (format_.isFloat) {
                        float v;
                        std::memcpy(&v, frame + ch * 4, 4);
                        sum += v;
                    } else if (containerBits_ == 16) {
                        int16_t v;
                        std::memcpy(&v, frame + ch * 2, 2);
                        sum += v / 32768.0f;
                    } else {
                        int32_t v;
                        std::memcpy(&v, frame + ch * 4, 4);
                        sum += static_cast<float>(v / 2147483648.0);
                    }
                }
            }
            mono_[i] = std::isfinite(sum) ? sum : 0.0f;  // one NaN or Inf from an app would poison the filters for good
        }
        if (ring_->write(mono_.data(), n) < n) {
            overruns_.fetch_add(1, std::memory_order_relaxed);
        }
        if (tap_) {
            tap_->write(mono_.data(), n);
        }
        if (data) {
            data += n * bytesPerFrame;
        }
        frames -= n;
    }
}

}  // namespace gfx

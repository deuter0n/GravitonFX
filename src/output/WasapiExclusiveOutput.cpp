// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "output/WasapiExclusiveOutput.h"

#include "capture/DeviceEnumerator.h"

#include <windows.h>
#include <audioclient.h>
#include <avrt.h>
#include <ks.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <wil/com.h>
#include <wil/resource.h>

namespace gfx {

namespace {

struct Candidate {
    SampleFormat format;
    WORD containerBits;
    WORD validBits;
    bool isFloat;
};

// Tried in this order; the first one the device accepts wins.
constexpr Candidate kCandidates[] = {
    {SampleFormat::Float32, 32, 32, true},
    {SampleFormat::Int32, 32, 32, false},
    {SampleFormat::Int24In32, 32, 24, false},
    {SampleFormat::Int24, 24, 24, false},
    {SampleFormat::Int16, 16, 16, false},
};

DWORD defaultChannelMask(int channels)
{
    switch (channels) {
    case 1: return KSAUDIO_SPEAKER_MONO;
    case 2: return KSAUDIO_SPEAKER_STEREO;
    case 4: return KSAUDIO_SPEAKER_QUAD;
    case 6: return KSAUDIO_SPEAKER_5POINT1;
    case 8: return KSAUDIO_SPEAKER_7POINT1_SURROUND;
    default: return 0;
    }
}

WAVEFORMATEXTENSIBLE makeFormat(const Candidate& c, DWORD rate, int channels, DWORD channelMask)
{
    WAVEFORMATEXTENSIBLE f{};
    f.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    f.Format.nChannels = static_cast<WORD>(channels);
    f.Format.nSamplesPerSec = rate;
    f.Format.wBitsPerSample = c.containerBits;
    f.Format.nBlockAlign = static_cast<WORD>(channels * c.containerBits / 8);
    f.Format.nAvgBytesPerSec = rate * f.Format.nBlockAlign;
    f.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    f.Samples.wValidBitsPerSample = c.validBits;
    f.dwChannelMask = channelMask;
    f.SubFormat = c.isFloat ? KSDATAFORMAT_SUBTYPE_IEEE_FLOAT : KSDATAFORMAT_SUBTYPE_PCM;
    return f;
}

OpenResult fail(OpenStatus status, std::wstring message) { return {status, std::move(message)}; }

// Maps a WASAPI error to what the user is told.
OpenResult mapError(HRESULT hr)
{
    switch (hr) {
    case AUDCLNT_E_DEVICE_IN_USE:
        return fail(OpenStatus::InUse, L"In use by another app. Close it or pick another device.");
    case AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED:
        return fail(OpenStatus::Failed, L"Exclusive mode is off. Turn it on in Windows sound settings.");
    case AUDCLNT_E_DEVICE_INVALIDATED:
    case E_NOTFOUND:
        return fail(OpenStatus::Missing, L"Not connected.");
    case AUDCLNT_E_UNSUPPORTED_FORMAT:
        return fail(OpenStatus::UnsupportedFormat, L"No supported format. Pick another device.");
    case AUDCLNT_E_INVALID_DEVICE_PERIOD:
    case AUDCLNT_E_BUFFER_SIZE_ERROR:
        return fail(OpenStatus::BufferRefused, L"Buffer size not supported. Pick a larger one.");
    default:
        return fail(OpenStatus::Failed, L"Can't open. Reconnect it or pick another device.");
    }
}

int64_t now()
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

}  // namespace

struct WasapiExclusiveOutput::Impl {
    wil::com_ptr<IMMDeviceEnumerator> enumerator;
    wil::com_ptr<IMMDevice> device;
    wil::com_ptr<IAudioClient> client;
    wil::com_ptr<IAudioRenderClient> renderClient;
    wil::unique_event_nothrow event;
};

WasapiExclusiveOutput::WasapiExclusiveOutput(IMMDeviceEnumerator* enumerator) : impl_(std::make_unique<Impl>())
{
    impl_->enumerator = enumerator;
}

WasapiExclusiveOutput::~WasapiExclusiveOutput() { close(); }

OpenResult WasapiExclusiveOutput::open(const std::wstring& deviceId, int bufferFrames, Renderer* renderer)
{
    close();
    renderer_ = renderer;
    failed_ = false;

    // The device must be connected.
    DWORD state = 0;
    if (FAILED(impl_->enumerator->GetDevice(deviceId.c_str(), &impl_->device)) ||
        FAILED(impl_->device->GetState(&state)) || state != DEVICE_STATE_ACTIVE) {
        impl_->device.reset();
        return fail(OpenStatus::Missing, L"Not connected.");
    }
    const AudioDevice info = describeDevice(impl_->device.get(), L"");
    if (info.sampleRate <= 0 || info.channels <= 0) {
        return mapError(E_FAIL);
    }

    HRESULT hr = impl_->device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, impl_->client.put_void());
    if (FAILED(hr)) {
        return mapError(hr);
    }

    // Native rate and channel count, best accepted sample format.
    const DWORD rate = static_cast<DWORD>(info.sampleRate);
    const DWORD mask = defaultChannelMask(info.channels);
    WAVEFORMATEXTENSIBLE wf{};
    const Candidate* chosen = nullptr;
    for (const Candidate& c : kCandidates) {
        wf = makeFormat(c, rate, info.channels, mask);
        if (impl_->client->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, &wf.Format, nullptr) == S_OK) {
            chosen = &c;
            break;
        }
    }
    if (!chosen) {
        return mapError(AUDCLNT_E_UNSUPPORTED_FORMAT);
    }

    // Buffer period from the chosen sample count.
    REFERENCE_TIME minPeriod = 0;
    impl_->client->GetDevicePeriod(nullptr, &minPeriod);
    auto periodFor = [rate](UINT32 frames) {
        return static_cast<REFERENCE_TIME>(10'000'000.0 * frames / rate + 0.5);
    };
    REFERENCE_TIME period = periodFor(static_cast<UINT32>(bufferFrames));
    if (period < minPeriod) {
        return mapError(AUDCLNT_E_INVALID_DEVICE_PERIOD);
    }

    hr = impl_->client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, period, period,
                                   &wf.Format, nullptr);
    if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
        // The device wants a slightly different size: ask for that one on a
        // fresh client, as WASAPI requires.
        UINT32 aligned = 0;
        impl_->client->GetBufferSize(&aligned);
        impl_->client.reset();
        hr = impl_->device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, impl_->client.put_void());
        if (SUCCEEDED(hr)) {
            period = periodFor(aligned);
            hr = impl_->client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, period,
                                           period, &wf.Format, nullptr);
        }
    }
    if (FAILED(hr)) {
        close();
        return mapError(hr);
    }

    impl_->event.create();
    UINT32 frames = 0;
    if (FAILED(hr = impl_->client->SetEventHandle(impl_->event.get())) ||
        FAILED(hr = impl_->client->GetBufferSize(&frames)) ||
        FAILED(hr = impl_->client->GetService(__uuidof(IAudioRenderClient), impl_->renderClient.put_void()))) {
        close();
        return mapError(hr);
    }

    format_.sampleRate = rate;
    format_.sampleFormat = chosen->format;
    format_.bits = chosen->validBits;
    format_.isFloat = chosen->isFloat;
    format_.channels = info.channels;
    format_.bufferFrames = static_cast<int>(frames);
    mono_.assign(frames, 0.0f);  // allocated here, never on the render thread
    return {OpenStatus::Ok, {}};
}

bool WasapiExclusiveOutput::start()
{
    if (!impl_->client || running_) {
        return false;
    }
    // Start with one buffer of silence so the first period isn't garbage.
    BYTE* data = nullptr;
    if (SUCCEEDED(impl_->renderClient->GetBuffer(static_cast<UINT32>(format_.bufferFrames), &data))) {
        impl_->renderClient->ReleaseBuffer(static_cast<UINT32>(format_.bufferFrames), AUDCLNT_BUFFERFLAGS_SILENT);
    }
    lastCallback_ = now();
    running_ = true;
    thread_ = std::thread([this] { renderLoop(); });
    if (FAILED(impl_->client->Start())) {
        stop();
        return false;
    }
    return true;
}

void WasapiExclusiveOutput::stop()
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
        impl_->client->Reset();
    }
}

void WasapiExclusiveOutput::close()
{
    stop();
    impl_->renderClient.reset();
    impl_->client.reset();
    impl_->device.reset();
    impl_->event.reset();
}

bool WasapiExclusiveOutput::fillBuffer()
{
    const auto frames = static_cast<UINT32>(format_.bufferFrames);
    BYTE* data = nullptr;
    if (FAILED(impl_->renderClient->GetBuffer(frames, &data))) {
        return false;  // device unplugged or reset
    }
    renderer_->render(mono_.data(), frames);
    writeInterleaved(mono_.data(), data, frames, format_.channels, format_.sampleFormat);
    impl_->renderClient->ReleaseBuffer(frames, 0);
    lastCallback_.store(now(), std::memory_order_relaxed);
    return true;
}

void WasapiExclusiveOutput::renderLoop()
{
    const auto com = wil::CoInitializeEx_failfast(COINIT_MULTITHREADED);
    DWORD taskIndex = 0;
    HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

    while (running_) {
        // A timeout just loops; the watchdog notices missing callbacks.
        if (WaitForSingleObject(impl_->event.get(), 200) != WAIT_OBJECT_0) {
            continue;
        }
        if (!fillBuffer()) {
            failed_ = true;
            break;
        }
    }

    if (mmcss) {
        AvRevertMmThreadCharacteristics(mmcss);
    }
}

}  // namespace gfx

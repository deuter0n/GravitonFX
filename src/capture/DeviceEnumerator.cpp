// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "capture/DeviceEnumerator.h"

#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmreg.h>
#include <propvarutil.h>
#include <wil/com.h>
#include <wil/resource.h>

#include <atomic>
#include <mutex>

namespace gfx {

namespace {

std::wstring defaultIdOf(IMMDeviceEnumerator* enumerator)
{
    wil::com_ptr<IMMDevice> device;
    if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device))) {
        return {};
    }
    wil::unique_cotaskmem_string id;
    if (FAILED(device->GetId(&id))) {
        return {};
    }
    return id.get();
}

}  // namespace

AudioDevice describeDevice(IMMDevice* device, const std::wstring& defaultId)
{
    AudioDevice d;
    wil::unique_cotaskmem_string id;
    if (SUCCEEDED(device->GetId(&id))) {
        d.id = id.get();
    }
    d.isDefault = !d.id.empty() && d.id == defaultId;

    wil::com_ptr<IPropertyStore> props;
    if (FAILED(device->OpenPropertyStore(STGM_READ, &props))) {
        return d;
    }
    wil::unique_prop_variant name;
    if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &name)) && name.vt == VT_LPWSTR) {
        d.name = name.pwszVal;
    }
    // The format the device runs at in shared mode is its native format.
    wil::unique_prop_variant format;
    if (SUCCEEDED(props->GetValue(PKEY_AudioEngine_DeviceFormat, &format)) && format.vt == VT_BLOB &&
        format.blob.cbSize >= sizeof(WAVEFORMATEX)) {
        const auto* wf = reinterpret_cast<const WAVEFORMATEX*>(format.blob.pBlobData);
        d.channels = wf->nChannels;
        d.sampleRate = static_cast<int>(wf->nSamplesPerSec);
    }
    return d;
}

// Forwards IMMNotificationClient calls to the listener.
class DeviceEnumerator::Notifier final : public IMMNotificationClient {
public:
    void setListener(Listener listener)
    {
        std::lock_guard lock(mutex_);
        listener_ = std::move(listener);
    }

    // IUnknown
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG refs = --refs_;
        if (refs == 0) {
            delete this;
        }
        return refs;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override
    {
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IMMNotificationClient)) {
            *out = static_cast<IMMNotificationClient*>(this);
            AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }

    // IMMNotificationClient
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR id) override
    {
        if (flow == eRender && role == eConsole) {
            notify(DeviceEvent::DefaultChanged, id);
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR id) override { return notify(DeviceEvent::Added, id); }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR id) override { return notify(DeviceEvent::Removed, id); }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR id, DWORD) override
    {
        return notify(DeviceEvent::StateChanged, id);
    }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }

private:
    HRESULT notify(DeviceEvent event, LPCWSTR id)
    {
        std::lock_guard lock(mutex_);
        if (listener_) {
            listener_(event, id ? id : L"");
        }
        return S_OK;
    }

    std::atomic<ULONG> refs_{1};
    std::mutex mutex_;
    Listener listener_;
};

struct DeviceEnumerator::Impl {
    wil::com_ptr<IMMDeviceEnumerator> enumerator;
    Notifier* notifier = nullptr;  // registered with the enumerator while set
};

DeviceEnumerator::DeviceEnumerator() : impl_(std::make_unique<Impl>())
{
    impl_->enumerator = wil::CoCreateInstance<MMDeviceEnumerator, IMMDeviceEnumerator>();
}

DeviceEnumerator::~DeviceEnumerator()
{
    if (impl_->notifier) {
        impl_->enumerator->UnregisterEndpointNotificationCallback(impl_->notifier);
        impl_->notifier->setListener(nullptr);
        impl_->notifier->Release();
    }
}

IMMDeviceEnumerator* DeviceEnumerator::native() const { return impl_->enumerator.get(); }

std::wstring DeviceEnumerator::defaultRenderId() const { return defaultIdOf(impl_->enumerator.get()); }

std::vector<AudioDevice> DeviceEnumerator::renderDevices() const
{
    std::vector<AudioDevice> result;
    wil::com_ptr<IMMDeviceCollection> collection;
    if (FAILED(impl_->enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection))) {
        return result;
    }
    const std::wstring defaultId = defaultRenderId();
    UINT count = 0;
    collection->GetCount(&count);
    for (UINT i = 0; i < count; ++i) {
        wil::com_ptr<IMMDevice> device;
        if (SUCCEEDED(collection->Item(i, &device))) {
            result.push_back(describeDevice(device.get(), defaultId));
        }
    }
    return result;
}

bool DeviceEnumerator::find(const std::wstring& id, AudioDevice& out) const
{
    wil::com_ptr<IMMDevice> device;
    DWORD state = 0;
    if (id.empty() || FAILED(impl_->enumerator->GetDevice(id.c_str(), &device)) ||
        FAILED(device->GetState(&state)) || state != DEVICE_STATE_ACTIVE) {
        return false;
    }
    out = describeDevice(device.get(), defaultRenderId());
    return true;
}

void DeviceEnumerator::setListener(Listener listener)
{
    if (!impl_->notifier) {
        impl_->notifier = new Notifier();
        impl_->enumerator->RegisterEndpointNotificationCallback(impl_->notifier);
    }
    impl_->notifier->setListener(std::move(listener));
}

}  // namespace gfx

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "host/WebViewHost.h"

#include <objbase.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <WebView2.h>
#include <wil/com.h>
#include <wil/resource.h>
#include <wrl.h>

#include <string_view>

#ifdef NDEBUG
#include "ui_resources.h"  // generated: kUiResources[] = { path, resource id, MIME type }
#endif

using Microsoft::WRL::Callback;

namespace gfx {

namespace {

constexpr wchar_t kHost[] = L"app.graviton";
constexpr wchar_t kStartUrl[] = L"https://app.graviton/index.html";
constexpr COREWEBVIEW2_COLOR kBackground = {255, 0x0c, 0x0f, 0x15};  // --bg in the UI

std::wstring widen(const std::string& s)
{
    if (s.empty()) {
        return {};
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

std::string narrow(const wchar_t* w)
{
    if (!w || !*w) {
        return {};
    }
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}

}  // namespace

std::wstring localAppDataFolder()
{
    wil::unique_cotaskmem_string base;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &base))) {
        return {};
    }
    return std::wstring(base.get()) + L"\\GravitonFX";
}

struct WebViewHost::Impl {
    HWND parent = nullptr;
    MessageHandler onMessage;
    ReadyHandler onReady;
    wil::com_ptr<ICoreWebView2Environment> environment;
    wil::com_ptr<ICoreWebView2Controller> controller;
    wil::com_ptr<ICoreWebView2> webview;
    bool ready = false;

    void configure();
#ifdef NDEBUG
    void serveEmbeddedFiles();
#endif
};

WebViewHost::WebViewHost() : impl_(std::make_unique<Impl>()) {}

WebViewHost::~WebViewHost()
{
    if (impl_->controller) {
        impl_->controller->Close();
    }
}

bool WebViewHost::create(HWND parent, MessageHandler onMessage, ReadyHandler onReady)
{
    impl_->parent = parent;
    impl_->onMessage = std::move(onMessage);
    impl_->onReady = std::move(onReady);

    const std::wstring userData = localAppDataFolder() + L"\\WebView2";
    Impl* impl = impl_.get();
    const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userData.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [impl](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                if (FAILED(result)) {
                    return result;
                }
                impl->environment = environment;
                return environment->CreateCoreWebView2Controller(
                    impl->parent,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [impl](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(result)) {
                                return result;
                            }
                            impl->controller = controller;
                            impl->controller->get_CoreWebView2(&impl->webview);
                            impl->configure();
                            return S_OK;
                        })
                        .Get());
            })
            .Get());

    if (FAILED(hr)) {
        MessageBoxW(parent,
                    L"Graviton FX needs the Microsoft Edge WebView2 Runtime, which isn't installed.\n\n"
                    L"Install it from microsoft.com/edge/webview2 (or run the Graviton FX installer), then start Graviton FX again.",
                    L"Graviton FX", MB_ICONERROR);
        return false;
    }
    return true;
}

void WebViewHost::Impl::configure()
{
    // Dark until the page paints, so the window never flashes white.
    if (auto c2 = controller.try_query<ICoreWebView2Controller2>()) {
        c2->put_DefaultBackgroundColor(kBackground);
    }

    wil::com_ptr<ICoreWebView2Settings> settings;
    webview->get_Settings(&settings);
    settings->put_IsStatusBarEnabled(FALSE);
    settings->put_IsZoomControlEnabled(FALSE);
    if (auto s9 = settings.try_query<ICoreWebView2Settings9>()) {
        s9->put_IsNonClientRegionSupportEnabled(TRUE);  // CSS app-region: drag on the header
    }
#ifdef NDEBUG
    settings->put_AreDevToolsEnabled(FALSE);
    settings->put_AreDefaultContextMenusEnabled(FALSE);
    if (auto s3 = settings.try_query<ICoreWebView2Settings3>()) {
        s3->put_AreBrowserAcceleratorKeysEnabled(FALSE);  // no F5, Ctrl+F, F12 ...
    }
    serveEmbeddedFiles();
#else
    settings->put_AreDevToolsEnabled(TRUE);  // F12 opens Edge DevTools
    if (auto w3 = webview.try_query<ICoreWebView2_3>()) {
        w3->SetVirtualHostNameToFolderMapping(kHost, GFX_UI_DIR, COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);
    }
#endif

    // Messages from the page.
    webview->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                wil::unique_cotaskmem_string json;
                if (SUCCEEDED(args->get_WebMessageAsJson(&json)) && onMessage) {
                    onMessage(narrow(json.get()));
                }
                return S_OK;
            })
            .Get(),
        nullptr);

    // The page is ready for messages once it has loaded.
    webview->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [this](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
                ready = true;
                if (GetForegroundWindow() == parent) {
                    controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
                }
                if (onReady) {
                    onReady();
                }
                return S_OK;
            })
            .Get(),
        nullptr);

    RECT bounds;
    GetClientRect(parent, &bounds);
    controller->put_Bounds(bounds);
    webview->Navigate(kStartUrl);
}

#ifdef NDEBUG
// Answers every request for https://app.graviton/... from the resources
// embedded in the executable; anything else gets a 404.
void WebViewHost::Impl::serveEmbeddedFiles()
{
    webview->AddWebResourceRequestedFilter(L"https://app.graviton/*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    webview->add_WebResourceRequested(
        Callback<ICoreWebView2WebResourceRequestedEventHandler>(
            [this](ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
                wil::com_ptr<ICoreWebView2WebResourceRequest> request;
                wil::unique_cotaskmem_string uri;
                args->get_Request(&request);
                request->get_Uri(&uri);
                std::wstring_view path(uri.get());
                path.remove_prefix(std::min(path.size(), std::wstring_view(L"https://app.graviton").size()));
                path = path.substr(0, path.find_first_of(L"?#"));

                for (const auto& r : kUiResources) {
                    if (path != r.path) {
                        continue;
                    }
                    HMODULE module = GetModuleHandleW(nullptr);
                    HRSRC found = FindResourceW(module, MAKEINTRESOURCEW(r.id), RT_RCDATA);
                    HGLOBAL loaded = found ? LoadResource(module, found) : nullptr;
                    if (!loaded) {
                        break;
                    }
                    const auto* data = static_cast<const BYTE*>(LockResource(loaded));
                    wil::com_ptr<IStream> stream;
                    stream.attach(SHCreateMemStream(data, SizeofResource(module, found)));
                    wil::com_ptr<ICoreWebView2WebResourceResponse> response;
                    const std::wstring headers = std::wstring(L"Content-Type: ") + r.mime;
                    environment->CreateWebResourceResponse(stream.get(), 200, L"OK", headers.c_str(), &response);
                    args->put_Response(response.get());
                    return S_OK;
                }
                wil::com_ptr<ICoreWebView2WebResourceResponse> notFound;
                environment->CreateWebResourceResponse(nullptr, 404, L"Not Found", L"", &notFound);
                args->put_Response(notFound.get());
                return S_OK;
            })
            .Get(),
        nullptr);
}
#endif

void WebViewHost::resize(const RECT& bounds)
{
    if (impl_->controller) {
        impl_->controller->put_Bounds(bounds);
    }
}

void WebViewHost::post(const std::string& json)
{
    if (impl_->webview && impl_->ready) {
        impl_->webview->PostWebMessageAsJson(widen(json).c_str());
    }
}

void WebViewHost::focus()
{
    if (impl_->controller) {
        impl_->controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
    }
}

void WebViewHost::setVisible(bool visible)
{
    if (!impl_->controller) {
        return;
    }
    impl_->controller->put_IsVisible(visible ? TRUE : FALSE);
    if (auto w3 = impl_->webview.try_query<ICoreWebView2_3>()) {
        if (visible) {
            w3->Resume();
        } else {
            w3->TrySuspend(nullptr);  // allowed once hidden; cuts CPU and GPU use
        }
    }
}

}  // namespace gfx

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "app/UpdateChecker.h"

#include "host/Updates.h"

#include <winhttp.h>

#include <string>
#include <vector>

namespace gfx {

namespace {

struct Endpoint {
    std::wstring host = L"api.github.com";
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
    std::wstring path = L"/repos/deuter0n/GravitonFX/releases/latest";
    bool secure = true;
};

// Debug builds only: GFX_UPDATE_TEST_URL (for example http://127.0.0.1:8770/latest)
// points the check at a local test server.
Endpoint endpoint()
{
    Endpoint e;
#ifndef NDEBUG
    wchar_t url[512];
    if (GetEnvironmentVariableW(L"GFX_UPDATE_TEST_URL", url, 512) > 0) {
        wchar_t host[256] = {}, path[256] = {};
        URL_COMPONENTS parts{};
        parts.dwStructSize = sizeof(parts);
        parts.lpszHostName = host;
        parts.dwHostNameLength = 256;
        parts.lpszUrlPath = path;
        parts.dwUrlPathLength = 256;
        if (WinHttpCrackUrl(url, 0, 0, &parts)) {
            e = {host, parts.nPort, path, parts.nScheme == INTERNET_SCHEME_HTTPS};
        }
    }
#endif
    return e;
}

// The reply body, or false on any failure (offline, timeout, HTTP error).
// GitHub answers 404 while the project has no releases yet; that means
// "nothing newer", so it comes back as an empty body.
//
// The request handle is shared through `slot` so that quitting can close it,
// which aborts the request at once; whoever takes it from the slot closes it.
bool fetch(HINTERNET session, std::atomic<void*>& slot, const std::atomic<bool>& cancelled, std::string& body)
{
    const Endpoint e = endpoint();
    HINTERNET connection = WinHttpConnect(session, e.host.c_str(), e.port, 0);
    HINTERNET request = connection ? WinHttpOpenRequest(connection, L"GET", e.path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                                        WINHTTP_DEFAULT_ACCEPT_TYPES, e.secure ? WINHTTP_FLAG_SECURE : 0)
                                   : nullptr;
    if (request) {
        slot.store(request);
        if (cancelled.load()) {  // quitting already
            if (slot.exchange(nullptr)) {
                WinHttpCloseHandle(request);
            }
            request = nullptr;
        }
    }
    bool ok = request && WinHttpSendRequest(request, L"Accept: application/vnd.github+json\r\n", static_cast<DWORD>(-1),
                                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
              WinHttpReceiveResponse(request, nullptr);
    DWORD status = 0, size = sizeof(status);
    if (ok) {
        ok = WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                                 &status, &size, WINHTTP_NO_HEADER_INDEX) &&
             (status == 200 || status == 404);
    }
    body.clear();
    while (ok && status == 200) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) {
            ok = false;
            break;
        }
        if (available == 0) {
            break;
        }
        std::vector<char> chunk(available);
        DWORD read = 0;
        if (!WinHttpReadData(request, chunk.data(), available, &read)) {
            ok = false;
            break;
        }
        body.append(chunk.data(), read);
        if (body.size() > (1 << 20)) {  // a release reply is a few kB
            ok = false;
        }
    }
    if (request && slot.exchange(nullptr)) {  // not already closed by a cancel
        WinHttpCloseHandle(request);
    }
    if (connection) {
        WinHttpCloseHandle(connection);
    }
    return ok;
}

}  // namespace

UpdateChecker::UpdateChecker(HWND window, UINT message) : window_(window), message_(message) {}

UpdateChecker::~UpdateChecker()
{
    cancelled_.store(true);
    if (void* request = request_.exchange(nullptr)) {
        WinHttpCloseHandle(request);  // aborts a running request at once
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

bool UpdateChecker::start()
{
    if (busy_.exchange(true)) {
        return false;
    }
    if (thread_.joinable()) {
        thread_.join();  // the previous check, already finished
    }
    thread_ = std::thread([this] { run(); });
    return true;
}

UpdateChecker::Result UpdateChecker::result() const
{
    std::lock_guard lock(mutex_);
    return result_;
}

void UpdateChecker::run()
{
    Result r;
    HINTERNET session = WinHttpOpen(L"GravitonFX/" GFX_VERSION, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session) {
        WinHttpSetTimeouts(session, 5000, 5000, 5000, 10000);  // resolve, connect, send, receive (ms)
        std::string body;
        if (!cancelled_.load() && fetch(session, request_, cancelled_, body)) {
            const auto tag = body.empty() ? std::nullopt : releaseTag(body);
            if (tag && isNewer(*tag, GFX_VERSION)) {
                r.kind = Result::Kind::Available;
                r.latest = tag->front() == 'v' || tag->front() == 'V' ? tag->substr(1) : *tag;
            } else {
                r.kind = Result::Kind::UpToDate;
            }
        }
        WinHttpCloseHandle(session);
    }
    {
        std::lock_guard lock(mutex_);
        result_ = r;
    }
    busy_.store(false);
    PostMessageW(window_, message_, 0, 0);
}

}  // namespace gfx

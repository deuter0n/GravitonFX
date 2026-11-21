// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "host/Updates.h"

#include <nlohmann/json.hpp>

#include <charconv>

namespace gfx {

namespace {

// Reads a number and the separator after it; false if there's no number.
bool readNumber(std::string_view& s, int& out)
{
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    if (ec != std::errc() || out < 0) {
        return false;
    }
    s.remove_prefix(static_cast<size_t>(end - s.data()));
    return true;
}

}  // namespace

std::optional<Version> parseVersion(std::string_view s)
{
    if (!s.empty() && (s.front() == 'v' || s.front() == 'V')) {
        s.remove_prefix(1);
    }
    Version v;
    if (!readNumber(s, v.major) || s.empty() || s.front() != '.') {
        return std::nullopt;
    }
    s.remove_prefix(1);
    if (!readNumber(s, v.minor) || s.empty() || s.front() != '.') {
        return std::nullopt;
    }
    s.remove_prefix(1);
    if (!readNumber(s, v.patch)) {
        return std::nullopt;
    }
    if (!s.empty() && s.front() == '-') {
        v.pre = std::string(s.substr(1));
        s = {};
    }
    if (!s.empty() && s.front() == '+') {
        s = {};  // build metadata doesn't count
    }
    if (!s.empty()) {
        return std::nullopt;
    }
    return v;
}

int compareVersions(const Version& a, const Version& b)
{
    if (a.major != b.major) {
        return a.major < b.major ? -1 : 1;
    }
    if (a.minor != b.minor) {
        return a.minor < b.minor ? -1 : 1;
    }
    if (a.patch != b.patch) {
        return a.patch < b.patch ? -1 : 1;
    }
    if (a.pre == b.pre) {
        return 0;
    }
    if (a.pre.empty() || b.pre.empty()) {
        return a.pre.empty() ? 1 : -1;  // the regular release is the newer one
    }
    return a.pre < b.pre ? -1 : 1;  // good enough for "beta.1" < "beta.2" < "rc.1"
}

std::optional<std::string> releaseTag(const std::string& json)
{
    const auto j = nlohmann::json::parse(json, nullptr, false);
    if (!j.is_object() || !j.contains("tag_name") || !j["tag_name"].is_string()) {
        return std::nullopt;
    }
    // only a real true counts; null or another type never throws
    const auto flag = [&](const char* key) { return j.contains(key) && j[key].is_boolean() && j[key].get<bool>(); };
    if (flag("draft") || flag("prerelease")) {
        return std::nullopt;
    }
    return j["tag_name"].get<std::string>();
}

bool isProjectUrl(const std::string& url)
{
    const std::string base = kRepoUrl;
    return url == base || url.rfind(base + "/", 0) == 0;
}

bool isNewer(const std::string& tag, const std::string& current)
{
    const auto latest = parseVersion(tag);
    const auto installed = parseVersion(current);
    return latest && installed && latest->pre.empty() && compareVersions(*latest, *installed) > 0;
}

}  // namespace gfx

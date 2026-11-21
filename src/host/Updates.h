// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace gfx {

// A semantic version: "0.2.0", "v0.2.0" or "1.0.0-beta.1".
struct Version {
    int major = 0;
    int minor = 0;
    int patch = 0;
    std::string pre;  // prerelease part ("beta.1"), empty for a regular release
};

std::optional<Version> parseVersion(std::string_view text);

// <0, 0 or >0. A prerelease comes before its regular release (1.0.0-beta < 1.0.0).
int compareVersions(const Version& a, const Version& b);

// From the reply of GitHub's "latest release" API: the tag of a regular,
// published release, or nothing for a draft, a prerelease or a reply that
// can't be read.
std::optional<std::string> releaseTag(const std::string& json);

// True if `tag` names a regular release newer than `current`.
bool isNewer(const std::string& tag, const std::string& current);

// The project's GitHub page. The UI may open only this page and its subpages.
inline constexpr char kRepoUrl[] = "https://github.com/deuter0n/GravitonFX";
bool isProjectUrl(const std::string& url);

}  // namespace gfx

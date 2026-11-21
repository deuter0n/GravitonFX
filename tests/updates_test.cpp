// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "host/Updates.h"

#include <doctest/doctest.h>

using namespace gfx;

TEST_CASE("parseVersion reads tags with and without v")
{
    auto v = parseVersion("v0.2.0");
    REQUIRE(v);
    CHECK(v->major == 0);
    CHECK(v->minor == 2);
    CHECK(v->patch == 0);
    CHECK(v->pre.empty());

    v = parseVersion("1.10.3-beta.2");
    REQUIRE(v);
    CHECK(v->minor == 10);
    CHECK(v->pre == "beta.2");

    CHECK(parseVersion("1.2.3+build.7"));
    CHECK_FALSE(parseVersion(""));
    CHECK_FALSE(parseVersion("v1.2"));
    CHECK_FALSE(parseVersion("latest"));
    CHECK_FALSE(parseVersion("1.2.3x"));
}

TEST_CASE("compareVersions follows semantic versioning")
{
    const auto c = [](const char* a, const char* b) { return compareVersions(*parseVersion(a), *parseVersion(b)); };
    CHECK(c("0.2.0", "0.1.0") > 0);
    CHECK(c("0.1.0", "0.2.0") < 0);
    CHECK(c("0.10.0", "0.9.9") > 0);  // numbers, not text
    CHECK(c("1.0.0", "1.0.0") == 0);
    CHECK(c("1.0.0", "1.0.0-rc.1") > 0);  // the release is newer than its prerelease
    CHECK(c("1.0.0-beta.2", "1.0.0-beta.1") > 0);
}

TEST_CASE("isNewer: only regular releases above the installed version")
{
    CHECK(isNewer("v0.2.0", "0.1.0"));
    CHECK_FALSE(isNewer("v0.1.0", "0.1.0"));
    CHECK_FALSE(isNewer("v0.0.9", "0.1.0"));
    CHECK_FALSE(isNewer("v0.3.0-beta", "0.1.0"));  // prereleases are ignored
    CHECK_FALSE(isNewer("nonsense", "0.1.0"));
}

TEST_CASE("isProjectUrl: only the project's GitHub pages")
{
    CHECK(isProjectUrl("https://github.com/deuter0n/GravitonFX"));
    CHECK(isProjectUrl("https://github.com/deuter0n/GravitonFX/releases/latest"));
    CHECK_FALSE(isProjectUrl("https://github.com/deuter0n/GravitonFXevil"));
    CHECK_FALSE(isProjectUrl("https://github.com/someone/else"));
    CHECK_FALSE(isProjectUrl("https://example.com/https://github.com/deuter0n/GravitonFX"));
    CHECK_FALSE(isProjectUrl("http://github.com/deuter0n/GravitonFX"));
    CHECK_FALSE(isProjectUrl("file:///C:/Windows/System32/calc.exe"));
    CHECK_FALSE(isProjectUrl(""));
}

TEST_CASE("releaseTag skips drafts, prereleases and bad replies")
{
    CHECK(releaseTag(R"({"tag_name":"v0.2.0","draft":false,"prerelease":false})") == "v0.2.0");
    CHECK(releaseTag(R"({"tag_name":"v0.2.0"})") == "v0.2.0");
    CHECK_FALSE(releaseTag(R"({"tag_name":"v0.3.0-beta","prerelease":true})"));
    CHECK_FALSE(releaseTag(R"({"tag_name":"v0.3.0","draft":true})"));
    CHECK_FALSE(releaseTag(R"({"message":"Not Found"})"));
    CHECK_FALSE(releaseTag("<html>rate limited</html>"));
    CHECK_FALSE(releaseTag(R"("just a string")"));
}

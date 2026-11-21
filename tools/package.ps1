# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 deuter0n
#
# Packages a finished Release build for a GitHub release:
#   dist\GravitonFX-<version>-portable.zip   GravitonFX.exe + LICENSE.txt
#   dist\GravitonFX-Setup-<version>.exe      when Inno Setup 7 (or 6) is installed
#
#   cmake --build --preset release
#   powershell -ExecutionPolicy Bypass -File tools\package.ps1
#
# The version comes from project(... VERSION x.y.z) in CMakeLists.txt.

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root "build\release\GravitonFX.exe"
if (-not (Test-Path $exe)) { throw "No Release build. Run: cmake --build --preset release" }

$cmake = Get-Content (Join-Path $root "CMakeLists.txt") -Raw
if ($cmake -notmatch 'project\(GravitonFX VERSION (\d+\.\d+\.\d+)') { throw "Version not found in CMakeLists.txt" }
$version = $Matches[1]

$dist = Join-Path $root "dist"
New-Item -ItemType Directory -Force $dist | Out-Null

# Portable zip: the executable is self-contained (UI embedded, static runtime).
$stage = Join-Path $dist "portable"
Remove-Item -Recurse -Force $stage -ErrorAction SilentlyContinue
New-Item -ItemType Directory $stage | Out-Null
Copy-Item $exe $stage
Copy-Item (Join-Path $root "LICENSE") (Join-Path $stage "LICENSE.txt")
$zip = Join-Path $dist "GravitonFX-$version-portable.zip"
Remove-Item $zip -ErrorAction SilentlyContinue
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip
Remove-Item -Recurse -Force $stage
Write-Host "Portable: $zip"

# Installer (task 12.1): needs Inno Setup 7 (or 6) and installer\GravitonFX.iss.
$iss = Join-Path $root "installer\GravitonFX.iss"
$candidates = foreach ($v in 7, 6) {
    "$env:ProgramFiles\Inno Setup $v\ISCC.exe"
    "${env:ProgramFiles(x86)}\Inno Setup $v\ISCC.exe"
    "$env:LOCALAPPDATA\Programs\Inno Setup $v\ISCC.exe"
}
$iscc = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($iscc -and (Test-Path $iss)) {
    # Microsoft's small WebView2 bootstrapper, bundled so the installer can add
    # the runtime on PCs that don't have it (not committed; see .gitignore).
    $redist = Join-Path $root "installer\redist"
    $bootstrapper = Join-Path $redist "MicrosoftEdgeWebview2Setup.exe"
    if (-not (Test-Path $bootstrapper)) {
        New-Item -ItemType Directory -Force $redist | Out-Null
        Invoke-WebRequest "https://go.microsoft.com/fwlink/p/?LinkId=2124703" -OutFile $bootstrapper -UseBasicParsing
    }
    & $iscc "/DAppVersion=$version" "/O$dist" $iss
    if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed." }
    Write-Host "Installer: $(Join-Path $dist "GravitonFX-Setup-$version.exe")"
} else {
    Write-Host "Installer skipped (needs Inno Setup 7 and installer\GravitonFX.iss)."
}

# Spec Delta

## Purpose

Defines how Graviton FX is published and installed: the public GitHub repository and its license, the README, the per-user installer, the portable zip, and the contents of each GitHub Release.

## ADDED Requirements

### Requirement: Public repository and license
The project SHALL be published at `https://github.com/deuter0n/GravitonFX` under GPL-3.0-or-later, with a `LICENSE` file and an `SPDX-License-Identifier: GPL-3.0-or-later` header in every source file. Third-party SDKs that may not be redistributed in source form (the ASIO SDK) SHALL NOT be committed.

#### Scenario: Clean repository
- **WHEN** someone clones the repository
- **THEN** it contains the LICENSE, the sources with SPDX headers and the build instructions, and no ASIO SDK files

### Requirement: README
The repository SHALL have a `README.md` with: a one-line description and a screenshot of the running app (`docs/screenshot.png`), what it does, features, requirements, installation, a quick start, building from source (Build Tools for Visual Studio 2022 17.8+ or newer, or Visual Studio, with the Desktop development with C++ workload), and the license with an ASIO trademark note. The screenshot SHALL show the real app and SHALL be refreshed when a release changes the UI.

#### Scenario: GitHub page
- **WHEN** someone opens the repository page on GitHub
- **THEN** they see the description and the dashboard screenshot first, followed by the install link

### Requirement: Per-user installer
Each release SHALL include `GravitonFX-Setup-<version>.exe`. It SHALL install without administrator rights to `%LOCALAPPDATA%\Programs\Graviton FX`, add a Start menu shortcut and an optional desktop shortcut, install the WebView2 runtime silently if it is missing, and register an uninstaller under Settings › Apps.

#### Scenario: Fresh install
- **WHEN** a user without admin rights runs the installer
- **THEN** Graviton FX is installed, appears in the Start menu, and launches

#### Scenario: Missing WebView2
- **WHEN** the installer runs on a PC without the WebView2 runtime
- **THEN** it installs the runtime and the app launches normally

### Requirement: Upgrade in place
A newer installer SHALL install on top of an existing installation without the user uninstalling the previous version first. It SHALL ask to close Graviton FX if it is running, replace the program files (removing files the new version no longer uses), keep a single entry under Settings › Apps, and keep the user's settings, presets and Start with Windows registration.

#### Scenario: Upgrade while running
- **WHEN** version 0.1.0 is running in the tray and the user runs the 0.2.0 installer
- **THEN** the installer asks to close Graviton FX, upgrades it without an uninstall step, and the user's presets are still there afterwards

#### Scenario: One entry after upgrading
- **WHEN** the user has upgraded from 0.1.0 to 0.2.0
- **THEN** Settings › Apps lists Graviton FX once, as version 0.2.0

### Requirement: Clean uninstall
Uninstalling SHALL remove everything the installer and the app put on the system: the installation folder with all its files, the shortcuts, the uninstaller's entry, the Start with Windows registration, and the app's cache (`%LOCALAPPDATA%\GravitonFX`, including the WebView2 data). It SHALL ask whether to also delete the user's settings and presets (`%APPDATA%\GravitonFX`), and remove them only on Yes. The app SHALL keep all of its files in these locations so nothing is left behind.

#### Scenario: Keep settings
- **WHEN** the user uninstalls and answers No to deleting settings
- **THEN** `%APPDATA%\GravitonFX\settings.json` remains and is used if the app is installed again, and nothing else of Graviton FX remains

#### Scenario: Remove settings
- **WHEN** the user uninstalls and answers Yes to deleting settings
- **THEN** no Graviton FX files, folders, shortcuts or registry values remain

### Requirement: Portable zip
Each release SHALL also include `GravitonFX-<version>-portable.zip` containing the executable and its license (`LICENSE.txt`), which runs from any folder without installation. If Start with Windows is used and the executable is later moved, the app SHALL update the registration to its new location the next time it starts.

#### Scenario: Run from a folder
- **WHEN** a user unzips the portable build to a folder and runs it
- **THEN** the app works fully, with settings in `%APPDATA%\GravitonFX`

### Requirement: GitHub Releases
Each version SHALL be published as a GitHub Release tagged `v<major>.<minor>.<patch>`, with the installer and portable zip attached and release notes. Builds MAY be unsigned; the README SHALL explain the Windows SmartScreen prompt ("More info › Run anyway").

#### Scenario: Update check finds it
- **WHEN** release `v0.2.0` is published with both files
- **THEN** installed copies of 0.1.0 report the update, and View release shows both downloads

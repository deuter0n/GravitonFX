# Proposal

## Why

A bass shaker (tactile transducer) on a gaming or sim-racing chair needs its own low-frequency feed. Windows has no built-in way to copy the bass part of what is playing to a second audio device with low and stable latency. Graviton FX is a new Windows desktop app that does this: it captures system or per-app audio, keeps only the bass, and drives the shaker on a separate device while the original audio keeps playing normally.

## What Changes

- New C++ Windows desktop application, built from an empty folder, where every stage builds before the next begins.
- Audio capture through WASAPI loopback: **System mix** (the Windows default output, followed automatically, or a fixed output device) or **App** (one application via per-process loopback).
- Real-time DSP:
  - Mono sum of all captured channels.
  - Subsonic high-pass (Off/10/15/20/30 Hz, default 20 Hz, gentle 12 or steep 24 dB/oct).
  - Butterworth low-pass (20â€“200 Hz, 12/24/36/48 dB/oct).
  - Output gain (Â±12 dB) and an optional -1 dBFS limiter.
  - Every parameter change is smoothed so it never causes an audible or tactile click.
- Shaker output through WASAPI exclusive mode or ASIO at the device's native format, with a selectable buffer size. The signal always goes to output channels 1 and 2.
- Clock-drift compensation between the capture and output devices, with an **automatic** safety reserve (3 ms, growing up to 6 ms if underruns occur), so latency stays low and stable over long sessions with no user tuning.
- Test tools that work with the engine running or stopped: a hold-to-play **40 Hz pulse** and a **stepped sweep** (20â€“200 Hz in 5 Hz steps) to find the chair's resonance.
- An engine lifecycle with clear states (Stopped, Starting, Running, Waiting, Switching, Error, Check output):
  - Devices can be changed while running.
  - Missing sources or devices produce Waiting; real failures produce Error and are retried automatically.
  - Feedback loops are blocked.
- Filter presets: a locked built-in Default plus user presets that can be created, saved, renamed, duplicated and deleted.
- A dark dashboard UI rendered with WebView2 (HTML/CSS/JS embedded in the executable):
  - Header: status, latency and Start/Stop.
  - Source and Shaker output panels, plus an Engine health panel.
  - A live "gravity well" drive view, IN/OUT meters and a level history.
  - A filter chart with live spectrum that can be dragged to tune.
  - A Settings page.
- Settings saved automatically and restored on launch, plus a system tray icon, minimize/close to tray, start with Windows, start minimized, and start the engine on launch.
- An open-source project on GitHub (`deuter0n/GravitonFX`), licensed **GPL-3.0-or-later**, with a README that includes a screenshot of the app.
- Update notifications: the app checks GitHub Releases for a newer version (after launch, then daily, or on demand) and notifies once per new version. Settings links to the GitHub project.
- Distribution: each GitHub Release offers a per-user installer (no admin rights, Start menu shortcut, clean uninstall, upgrade in place) and a portable zip. Builds are unsigned for now.

## Capabilities

### New Capabilities

- `audio-capture`: Choosing and capturing the source audio (System mix or App), the source lists, waiting for missing sources, and silence handling.
- `signal-processing`: The DSP chain from captured audio to shaker signal (mono sum, subsonic filter, low-pass, gain, limiter, smoothing), level and spectrum analysis, and the pulse and sweep test signals.
- `shaker-output`: Driving the shaker device through WASAPI exclusive or ASIO, device lists, native format, buffer size, fixed output channels, clock-drift compensation and the automatic safety reserve.
- `engine-lifecycle`: Engine states, start and stop, changing devices while running, waiting versus errors, automatic retry, feedback protection, real-time safety and independence from the UI.
- `filter-presets`: The built-in Default preset and user presets, their management, and modified-state tracking.
- `dashboard-ui`: The main window and its panels, live visualizations, the Settings page, and the message contract between the UI and the host.
- `app-settings`: Persisting and restoring settings, reset to defaults, the app and tray icons, starting with Windows and other startup behavior.
- `app-updates`: Checking GitHub Releases for a newer version, notifying the user, and opening the release page.
- `distribution`: How Graviton FX reaches users: the GitHub repository and license, the README, the installer, the portable zip, and GitHub Releases.

### Modified Capabilities

None. This is a new project and no specs exist yet.

## Impact

- **New codebase**: C++20 application built with CMake and Ninja, using Build Tools for Visual Studio or Visual Studio. Dependencies come from vcpkg (WebView2 SDK, WIL, nlohmann-json, doctest), plus the Steinberg ASIO SDK, which must be downloaded separately and is never committed (see design).
- **Public repository and license**: `https://github.com/deuter0n/GravitonFX`, GPL-3.0-or-later. ASIO in release builds relies on Steinberg's GPLv3 licensing option for the ASIO SDK, whose current terms must be verified before the first release.
- **Network access**: the only network request is the update check to `api.github.com`, and it can be turned off. Fonts are bundled with the app instead of loaded from the web.
- **Packaging**: an Inno Setup script for the per-user installer, a portable zip, and GitHub Releases. Builds are unsigned, so Windows SmartScreen warns on first run until signing is set up later.
- **Platform**: Windows 10 or later. App capture needs Windows 10 version 2004 (build 19041) or later. The WebView2 runtime is preinstalled on Windows 11 and must be installed on older systems.
- **System integration**: writes `%APPDATA%\GravitonFX\settings.json` and the `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` registry value (when "Start with Windows" is on). WASAPI exclusive mode and ASIO take exclusive use of the shaker device while running or while a test signal plays.
- **Starting point for the UI**: the approved interactive mockup (https://claude.ai/artifact/PEqc2ovPpXaRoeVMEJtWXV, version 115) becomes the initial `ui/` source.
- **Out of scope for this change**: multiple shaker outputs (output routing per shaker), an "All apps" source, using only the LFE channel as the source, a hint about audio playing on other devices, automatic per-game preset switching, automatic download and installation of updates, code signing, and a winget listing. The design should not rule these out.

# Tasks

Every group ends in a state that builds (`cmake --build build --config Release`) and passes `ctest` before the next group starts.

## 1. Project scaffold

- [x] 1.0 Create the public repository: `git init`, a `.gitignore` (build output, `.vs/`, `third_party/asiosdk/`), the GPL-3.0 `LICENSE`, the existing `openspec/` folder, a first commit, then `gh repo create deuter0n/GravitonFX --public --source . --push` (confirm with the user right before running it); verify the repository page shows the files and the license
- [x] 1.1 Create `CMakeLists.txt` (C++20, MSVC /W4, static runtime, `project(GravitonFX VERSION 0.1.0)` feeding `GFX_VERSION`), `CMakePresets.json` (`debug` and `release`, Ninja, vcpkg toolchain from `VCPKG_ROOT`) and `vcpkg.json` (wil, nlohmann-json, doctest, webview2); every source file gets the `SPDX-License-Identifier: GPL-3.0-or-later` header; verify `cmake --preset release` configures from "Developer PowerShell for VS" (Build Tools) and from VS Code's CMake Tools with the MSVC kit
- [x] 1.2 Add `src/app/main.cpp` that opens an empty dark Win32 window titled "Graviton FX"; verify the Release build produces `GravitonFX.exe` and the window opens and closes cleanly
- [x] 1.3 Add a `tests/` doctest target registered with CTest containing one placeholder test; verify `ctest --test-dir build -C Release` passes
- [x] 1.4 Write `README.md` with the sections from the `distribution` spec (description, a screenshot placeholder, what it does, features, requirements, install, quick start, build from source with Build Tools for Visual Studio 2022 17.8+ or newer and the preset commands, the optional ASIO SDK download, SmartScreen note, license and ASIO trademark note); verify the build commands run as written on a clean clone

## 2. DSP core (headless, unit-tested)

- [x] 2.1 Implement `SpscRing` (lock-free mono float ring, preallocated, power-of-two capacity); verify tests cover wraparound, full/empty and a two-thread producer/consumer stress test
- [x] 2.2 Implement `Biquad` (TDF-II, double state) and `Butterworth` low-pass design (orders 2/4/6/8 → 1–4 sections); verify tests show -3 dB ±0.5 dB at cutoff and ≥45 dB attenuation at 320 Hz for 80 Hz / 24 dB/oct
- [x] 2.3 Add the `Butterworth` high-pass (subsonic) design, order 2 or 4 at 10/15/20/30 Hz and a bypass for Off; verify tests show ≥11 dB attenuation at 10 Hz (20 Hz, 12 dB/oct), ≥23 dB (20 Hz, 24 dB/oct), and <1 dB loss at 45 Hz
- [x] 2.4 Implement `FilterChain` (low-pass → test-signal insertion → subsonic high-pass → gain) with per-block cutoff and subsonic-frequency smoothing (~20 ms), per-sample gain ramp, and a 20 ms crossfade for slope and subsonic-slope changes; verify tests that a fast 20→200 Hz sweep and a 12→48 dB/oct switch produce no sample-to-sample jump above a set threshold
- [x] 2.5 Implement `Limiter` (instant attack, ~50 ms release, hard clamp at -1 dBFS, engaged/clipped flags); verify tests that a +3 dBFS input never exceeds -1 dBFS and that the flags report correctly with the limiter on and off
- [x] 2.6 Implement the variable-ratio linear-interpolation `Resampler`; verify a test that after the low-pass, images of a 60 Hz tone resampled 48→44.1 kHz are below -60 dB
- [x] 2.7 Implement `DriftController` (PI on smoothed fill error, ±1000 ppm clamp, lock detection, setpoint glide, freeze while capture is idle) and `Reserve` (3 ms start, +1 ms per underrun, max 6 ms, reset); verify a simulation with clocks 50 ppm apart for 4 simulated hours keeps latency within ±1 ms and locks within 10 s, that latency is unchanged across a 60 s idle gap, that the reserve grows 3→6 ms over repeated underruns without output discontinuity, and that a forced overrun discards with a fade and does not grow the reserve
- [x] 2.8 Implement `TestSignals`: a hold-to-play 40 Hz pulse with 10 ms fades, and a stepped sweep (20→200 Hz, 5 Hz steps, 0.5 s each, phase-continuous between steps, fades at the ends, current-frequency output); verify unit tests for the fades, the step timing (37 steps, 18.5 s), phase continuity at step boundaries, and the reported frequency
- [x] 2.9 Implement the output channel writer (mono to channels 1–2, zero all others, for any channel count); verify a unit test with 2- and 8-channel buffers

## 3. Shaker output (WASAPI exclusive)

- [ ] 3.1 Implement `DeviceEnumerator` for active render endpoints (ID, friendly name, default flag, channel count) with an `IMMNotificationClient` that forwards events to a callback; verify a `--list-devices` debug switch prints the devices and reacts to plugging in a USB device
- [ ] 3.2 Define the `OutputDevice` interface and implement `WasapiExclusiveOutput` (event-driven, native-format probing float32/int32/int24/int16, aligned buffer of 32/64/128/256 samples, MMCSS "Pro Audio", channel count, callback timestamp for the watchdog); verify `--test-tone <deviceId>` plays a clean 40 Hz tone on outputs 1–2 at each buffer size with no crackle
- [ ] 3.3 Map open failures (device in use, unsupported format, buffer size refused) to error codes with messages for the user, and treat "device not present" as a missing-device result (not an error); verify by opening a device another app holds exclusively (gives the "in use" error) and an unplugged device (gives the missing result)

## 4. Capture

- [ ] 4.1 Implement `LoopbackCapture` for a render endpoint (event-driven, mono downmix of all channels including LFE, conversion to float, writes into `SpscRing`, idle flag on timeout, zeros for silent packets); verify a debug mode that prints capture peak levels while music plays and shows idle when playback stops
- [ ] 4.2 Add "Windows default" resolution and re-open on `OnDefaultDeviceChanged`, plus fixed-device capture by endpoint ID with a missing result when it's unplugged; verify that switching the Windows default moves capture in default mode and doesn't in fixed mode
- [ ] 4.3 Spike: activate process loopback (include tree) on a known PID and record findings (delivered format, measured latency compared with System mix, behavior when the app plays to a non-default device) in design.md; verify the spike captures a browser tab's audio
- [ ] 4.4 Implement `ProcessCapture` (Windows build ≥ 19041 check, 48 kHz float stereo request, same ring and idle behavior as loopback); verify that capturing a music player excludes a game's audio playing at the same time
- [ ] 4.5 Implement `SessionWatcher`: list apps with audio sessions (exe, display name, playing), detect when the target exe appears or disappears, and re-activate on a new PID; verify that closing and relaunching the target app resumes capture without user action

## 5. Engine and lifecycle

- [x] 5.1 Implement the `Engine` control thread with a command queue and the state machine (Stopped/Starting/Running/Waiting/Switching/Error, plus the Check output flag); verify unit tests of the state transitions using fake capture and output devices, including the missing → Waiting and failing → Error split
- [ ] 5.2 Wire capture → ring → resampler → FilterChain → limiter → channel writer, with parameters through atomics and telemetry snapshots (latency, drift, fill, dropouts, lock, limiter flag), connect `Reserve` to underruns, and count dropouts (underruns, overruns, capture discontinuities, not the post-silence catch-up) with per-cause counts in the debug log; verify a debug console build streams System mix bass to the shaker and prints telemetry, that a forced underrun, overrun and capture discontinuity each add one dropout while a silence gap adds none, and audit the audio threads for allocations or locks
- [ ] 5.3 Implement changes while running: source swap (open new capture, then fade and swap, ≤100 ms), output swap (fade, reopen, ≤1 s), and smooth parameter changes; verify each change while bass is playing produces no click and the status passes through Switching back to Running
- [ ] 5.4 Implement Waiting for a missing app, capture device or shaker device, and automatic resume on session or device notifications; verify that a closed game, an unplugged fixed capture device, and a switched-off shaker DAC each give Waiting and resume Running when they return
- [ ] 5.5 Implement the error retry loop (every 3 s and on device-arrived events) and the output stall watchdog (more than 500 ms without a callback triggers a restart); verify that holding the shaker in exclusive mode with another app gives Error, and releasing it recovers within 3 s
- [x] 5.6 Implement the feedback check (resolved System mix capture endpoint equals shaker endpoint → refuse to start, stop if already running, Check output flag); verify that selecting the same device for both blocks Start, and that App mode never triggers it
- [ ] 5.7 Implement test mode: `pulseStart`/`pulseStop` (10 s hard limit) and `sweep` (start/stop), mixed into the live output when running, or opening only the output device when stopped (header stays Stopped, device released after the test), one test at a time, and a short-lived `testMsg` when the output can't be opened; verify that a pulse and a sweep play with the engine stopped and running, and that an unplugged shaker gives the message instead of an Error

## 6. Analysis

- [x] 6.1 Add the analysis taps (separate SPSC rings for input and output, dropping blocks when behind) and an analysis thread computing peak levels and a 64-band log spectrum (10 Hz–2 kHz, Hann-windowed FFT) at 30 Hz; verify unit tests where a 50 Hz tone peaks in the right band, a 1 kHz tone is absent from the output spectrum at an 80 Hz cutoff, and test signals appear only in the output spectrum
- [ ] 6.2 Add no-signal detection (input below -60 dBFS for 2 s while Running) and add `inDb`, `outDb`, `limiterHit`, `noSignal`, `spectrum.in/out`, `sweepHz` and the `test` flag to telemetry; verify the debug console shows plausible values while music plays, and that `noSignal` turns on 2 s after pausing playback

## 7. UI host

- [x] 7.1 Copy the approved mockup (local copy: `openspec/changes/build-graviton-fx/mockup/index.html`, identical to https://claude.ai/artifact/PEqc2ovPpXaRoeVMEJtWXV version 128) into `ui/` as `index.html`, `app.css` and `app.js`, keeping the mock host and Simulate controls for browser-only use and excluding them from the Release build; replace the Google Fonts links with bundled `.woff2` files in `ui/fonts/` (with their OFL license files); verify that opening `ui/index.html` in a browser behaves like the mockup and makes no network requests
- [x] 7.2 Implement `WebViewHost` in the main window: WebView2 user data folder at `%LOCALAPPDATA%\GravitonFX\WebView2`, virtual host mapping in Debug, embedded resources served through `WebResourceRequested` in Release, dark background before first paint, and F12 opening Edge DevTools in Debug builds only; verify that the Release exe shows the dashboard with no `ui/` folder next to it and ignores F12
- [x] 7.3 Implement the message bridge (`AppController`): route UI commands (`start`, `stop`, `set`, `pulseStart`, `pulseStop`, `sweep`, `refreshProcesses`, `resetSettings`, preset commands) to the engine and host, push `state` on every change, `devices` on enumeration changes, and `telemetry` at 30 Hz; verify that Start/Stop, the knobs, chart dragging, the device and app pickers, Pulse and Sweep drive the real engine, and that the meters, well, history, spectrum, sweep marker and health show real data
- [x] 7.4 Hide or suspend the WebView and stop telemetry while minimized or hidden; verify with Task Manager that GPU and CPU use of the WebView processes drop to about zero while the engine keeps running
- [x] 7.5 Check keyboard navigation and reduced-motion behavior in the WebView; verify every control is reachable with Tab, arrow keys, Space and Enter (including holding Pulse with Space), and that animations stop with Windows "Animation effects" off

## 8. Settings and presets

- [x] 8.1 Implement `Settings` (versioned JSON in `%APPDATA%\GravitonFX`, debounced atomic save through a temp file and replace, fallback to `.bak` with field-level merge, the defaults from `app-settings`); verify unit tests for round trip, a corrupted file, a missing folder and first-run defaults
- [x] 8.2 Restore all saved settings on launch (including subsonic frequency and slope; not the reserve) and apply them to the engine; verify that configuring everything, quitting and relaunching restores the identical state
- [x] 8.3 Implement preset logic in the host (locked Default, create, save, revert, rename, duplicate, delete, unique names ignoring case, delete-active falls back to Default, presets hold only cutoff/slope/gain) behind the preset commands; verify unit tests for each rule and that the dropdown and Settings list work end to end
- [x] 8.4 Implement `resetSettings` (confirmed in the UI, keeps presets, activates Default); verify from the Settings About section

## 9. System integration

- [x] 9.1 Add the tray icon (tooltip with state; Open, Start/Stop and Quit menu; click to show) and minimize-to-tray; verify that the engine keeps running while the window is hidden and Quit stops the engine and saves settings
- [x] 9.2 Implement the "Close button hides to tray" behavior; verify that close hides with the option on and quits with it off
- [x] 9.3 Implement "Start with Windows" (the `HKCU\...\Run` value with `--autostart`), "Start in the tray with Windows" and "Start engine on launch"; verify after sign-out and sign-in that the app starts in the tray and streams to the shaker with no window shown, or waits if the shaker is off
- [x] 9.4 Implement single-instance behavior (a named mutex plus a message to show the existing window); verify that launching twice shows the first window and doesn't start a second process
- [x] 9.6 Implement `UpdateChecker` (WinHTTP on a background thread, `releases/latest`, semantic version compare against `GFX_VERSION`, 10 s after launch then every 24 h when enabled, Check now), the tray notification once per version via `notifiedVersion`, the Settings dot and About status through `state.update`, and `openUrl` restricted to `kRepoUrl` and its subpages; verify with a test tag on the repo (or a local mock server) that 0.1.0 reports a newer version once, stays silent offline, ignores prereleases, makes no request with the setting off, and that `openUrl` refuses other URLs
- [x] 9.5 Add the concept A icon: copy the approved SVGs from `openspec/changes/build-graviton-fx/icons/` (app-full, app-small, and the four tray states) to `assets/icon/`, `tools/make_icons.ps1` generating `app.ico` (16–256 px) and the four tray-state icons, embedded through the `.rc` file, with the tray icon swapped on every engine state change; verify that the icon is crisp in Explorer, the taskbar and Alt-Tab at 100%, 150% and 200% scaling, and that the tray icon shows amber, grey, blue-dot and red-dot as the engine moves between Running, Stopped, Waiting and Error

## 10. ASIO output

- [x] 10.1 Integrate the ASIO SDK host code (CMake option `GFX_WITH_ASIO`, SDK path), enumerate ASIO drivers with their output channel counts and list them when the driver is ASIO; verify the build works with the option on and off, and that the driver list shows the installed drivers
- [x] 10.2 Implement `AsioOutput` (one driver loaded at a time, native sample type, requested buffer size or "buffer size refused" error, shared render path, channels 1–2 only); verify that the test pulse, the sweep and full streaming work through an ASIO driver (for example FlexASIO) at 64 samples, and that a multi-output interface shows the "outputs 1–2 of N" remark
- [ ] 10.3 Map ASIO failures (driver won't load, already in use) to engine errors with retry; verify that opening a driver while another app holds it shows the ASIO error and recovers when that app closes
  - Note (2026-11-21): the FiiO ASIO driver on the dev PC accepts several apps at once (`gfxprobe --hold-asio` held it and Graviton still played), so it can't produce "in use". The engine's retry and recovery are covered by unit tests; the ASIO error mapping still needs a driver that allows one app at a time.

## 11. Integration checks

- [ ] 11.1 Soak test: run for 4 hours of continuous game audio through a USB shaker DAC; verify from a telemetry log that latency stays within ±1 ms of its settled value, the reserve stays at 3 ms, and Dropouts stays at 0
  - Note (2026-11-22): first 4-hour runs with `gfxprobe --run` (game-like audio, speakers at volume 0). ASIO (FiiO, 64 samples): 9 dropouts in four events in the first 90 minutes, reserve 3 → 6 ms, latency 20 → 23 ms, clean after that. WASAPI (JadeAudio, 256 samples): 3 dropouts in the first second (reserve 6 ms), then 4 hours clean at 28.5–29.3 ms. Three later 15-second WASAPI starts had no dropouts. `gfxprobe` now prints each dropout's cause (under, over, gap) for the next run.
  - Fixed (2026-11-22): the start-up dropouts were underruns. Capture packets arrived exactly every 10 ms; the fill's low point drifts over several packets because 256-sample blocks and 480-frame packets don't line up, which the old target didn't allow for (see D5). Before: 20 of 52 starts at 256 samples had 1–2 underruns. After: 20 of 20 clean, reserve 3 ms, latency 28.5 ms (about 2.5 ms more than a clean start before). A unit test covers 32–512 samples at 40 packet timings.
- [ ] 11.2 Measure end-to-end latency (test click on the main output and the shaker output recorded together on a two-channel interface) for System mix and App mode; verify it is about 15 ms at 64 samples, and record both results in README.md
- [ ] 11.3 Go through every scenario in `specs/engine-lifecycle`, `specs/audio-capture` and `specs/shaker-output` by hand (default change, unplug and replug of capture and shaker, app restart, device in use, feedback, silence gap, test signals while stopped); verify each one matches the spec and note any deviations
- [ ] 11.4 Spike: test exclude-mode process loopback (all apps except Graviton) with audio on non-default devices, and record the result under Open Questions in design.md; verify the finding is written down

## 12. Packaging and first release

- [x] 12.1 Write `installer/GravitonFX.iss` (per user, no admin, `{localappdata}\Programs\Graviton FX`, Start menu and optional desktop shortcut, silent WebView2 bootstrapper when missing, upgrade over an existing install with the same `AppId` (a running copy is closed by posting it `WM_ENDSESSION`, since a normal close only hides it to the tray) and `[InstallDelete]` for files dropped by the new version, uninstall removing the whole install folder, shortcuts, the Run value and `%LOCALAPPDATA%\GravitonFX`, and asking about `%APPDATA%\GravitonFX`); verify on a clean standard user account: install, upgrade over a running copy without uninstalling (presets kept, one Apps entry), and uninstall with both answers, then check that no Graviton FX files, folders, shortcuts or registry values remain (except settings after No)
- [x] 12.2 Produce the portable zip and add the start-up check that corrects the Run value when the executable has moved; verify the unzipped exe runs from any folder and Start with Windows still works after moving it
- [x] 12.3 Capture `docs/screenshot.png` from the running app (dashboard mid-session, about 1440×900) and put it in the README; verify it shows on the GitHub repository page
- [x] 12.4 Verify the ASIO SDK licensing terms for GPL distribution and record the result in design.md D11; if they don't fit, set release builds to `GFX_WITH_ASIO=OFF` and update the README; verify the decision is written down
- [ ] 12.5 Write `RELEASING.md` (version bump in CMake, Release build, installer, zip, tag, `gh release create` with both files and notes) and publish `v0.1.0` that way; verify the release page shows both downloads and that a build reporting an older version (for example 0.0.9) notifies about 0.1.0

## Workflow follow-up

- Run `openspec validate build-graviton-fx --strict` before implementation starts.
- Archive the change with `openspec archive build-graviton-fx` after all tasks are done, which turns the specs into the project's baseline.

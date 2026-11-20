# Design

## Context

The project folder is empty, so everything in this change is new code. See proposal.md for motivation and the specs for required behavior. The constraints that shape this design are:

- **Latency comes first.** The audio path must be real-time safe, meaning no allocations, locks, logging, file access or UI calls on audio threads, and latency must stay stable over sessions of several hours.
- **Two independent clocks.** Capture follows the clock of the captured device (or of the audio engine, for App capture). Output follows the clock of the shaker device. These clocks always drift apart.
- **Loopback capture is tied to shared mode.** WASAPI loopback delivers audio in the engine's shared-mode periods, typically about 10 ms, so the largest part of the latency budget is fixed. The parts we control are the safety reserve and the output buffer.
- **Simple, readable code that builds at every stage.** Prefer plain Win32/COM with WIL over large frameworks.
- **Few user-facing knobs.** The user explicitly removed options for latency target, output channels and keeping capture awake. The engine handles these automatically.
- **The UI is already designed.** The approved interactive mockup (https://claude.ai/artifact/PEqc2ovPpXaRoeVMEJtWXV, version 76) is a single HTML file. It already uses the final message contract, with a bridge that switches between a mock host and `window.chrome.webview`.

Expected latency budget:

```
 loopback capture period      ~10 ms   fixed by Windows shared mode
 DSP (IIR biquads)            ~0 ms    a few samples of phase delay
 safety reserve               3-6 ms   automatic: starts at 3, +1 per underrun
 output buffer                0.7-5 ms 32-256 samples; WASAPI exclusive is double-buffered
 ------------------------------------
 typical end to end           ~15 ms
```

## Goals / Non-Goals

**Goals:**
- A process layout where the engine never depends on the UI being responsive.
- One DSP and drift path shared by all source types and both output drivers.
- Device and source changes handled entirely by a control thread. Audio threads only see swapped pointers and new parameter values.
- An engine designed around an output object with its own resampler, drift state and channel routing, so supporting several shaker outputs later is an extension, not a rewrite.

**Non-Goals:**
- Lowering the loopback capture period below what shared mode provides.
- An auto-updater that downloads and installs new versions by itself (the app only notifies and opens the release page), code signing, and a winget listing.
- Cross-platform support.
- Multiple shaker outputs, an "All apps" source, an LFE-only source, a hint about audio on other devices, or per-game automatic presets (see proposal Impact).

## Decisions

### D1. One process, four kinds of threads

```
 +------------------------------------------------------------------------+
 | GravitonFX.exe                                                         |
 |                                                                        |
 |  [capture thread] --SPSC ring--> [output thread]   real-time (MMCSS    |
 |        |                  \          ^              "Pro Audio")       |
 |        |                   \         | params: atomics                 |
 |        |  analysis taps (SPSC)\      | telemetry: atomics              |
 |        v                      v      |                                 |
 |  [analysis thread] <------------  [control thread]  device open/close, |
 |   FFT + levels, 30 Hz            command queue,     retries, watchdog, |
 |        |                          device + session notifications       |
 |        v                                ^                              |
 |  [UI thread: Win32 + WebView2 + tray] --+  commands (queue), state     |
 +------------------------------------------------------------------------+
```

- **The capture and output threads** are the only real-time threads. They are event-driven, registered with MMCSS "Pro Audio", and preallocate everything at open time.
- **The control thread** owns all device objects. It runs a command queue: start, stop, set the source, set the output, test signals, retry, and device and session events. Everything that can block or allocate happens here.
- **The analysis thread** reads copies of the input and output samples through separate SPSC rings, then computes peak levels, no-signal detection and the spectrum about 30 times a second. If it falls behind, it drops blocks, so it can never delay audio.
- **The UI thread** runs the Win32 message loop, WebView2, the tray icon and settings I/O. It sends commands to the control thread and reads state and telemetry snapshots.

*Alternatives considered:* a single audio thread that pulls capture synchronously inside the output callback. Rejected, because the two device clocks and loopback's packet behavior make blocking inside the output callback unsafe. A separate engine process was also rejected as unnecessary complexity.

### D2. Capture

- **System mix**: `IAudioClient` in shared mode with `AUDCLNT_STREAMFLAGS_LOOPBACK | EVENTCALLBACK` on the chosen render endpoint. "Windows default" is resolved through `IMMDeviceEnumerator`, and an `IMMNotificationClient` sends `OnDefaultDeviceChanged` to the control thread, which re-opens capture. A fixed device is stored by its endpoint ID string.
- **App**: `ActivateAudioInterfaceAsync` with `AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK` in `PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE`. We request 48 kHz float stereo, and Windows converts the app's audio to it (including downmixing surround). That's why the UI hides the format line in App mode. Availability is checked at runtime (Windows build ≥ 19041). The app is stored by executable name; the control thread watches for that executable (polling audio sessions every 2 s plus `IAudioSessionNotification`) and re-activates capture on the new PID.
- **Lists**: only active render endpoints and processes with audio sessions are listed. The current selection is added by name when it's missing. There's no device history, so nothing beyond the saved selection is persisted.
- **Silence**: loopback sends no packets when nothing plays. The capture thread uses a short wait timeout. When no data arrives, it publishes a "capture idle" flag rather than writing anything, and the output side handles the gap (D5). Packets flagged `AUDCLNT_BUFFERFLAGS_SILENT` are written as zeros. Nothing is ever played on the captured device.
- **Downmix and conversion** happen on the capture thread, so the ring always carries mono float at the capture rate. All channels, including LFE, are summed.

### D3. DSP chain (output thread)

```
 ring (mono, capture rate) -> resampler (D5) -> low-pass -> (+ test signal) -> subsonic HPF -> gain -> limiter -> outputs 1+2
```

- **a. Low-pass filter.** A Butterworth low-pass built from cascaded biquads (order = slope / 6, so 1 to 4 sections) in transposed direct form II with double-precision state. Running it after the resampler means its coefficients are always for the output rate.
- **b. Subsonic high-pass.** A Butterworth high-pass of order 2 (12 dB/oct, one biquad) or 4 (24 dB/oct, two biquads), at Off/10/15/20/30 Hz, default 20 Hz at 12 dB/oct. It sits after the test-signal insertion point, so it protects the hardware from test signals too. The gentle default keeps added delay and ringing in the 20–35 Hz range low, which suits games. The steep option is there for heavy movie content or a knocking shaker.
- **c. Smoothing.** The control thread publishes target values through atomics. The output thread moves the cutoff and subsonic frequency toward their targets over about 20 ms, recomputing coefficients once per block, and ramps gain per sample. Changes of slope or subsonic slope change the number of filter sections, so the output thread keeps two preallocated filter instances and crossfades from the old one to the new one over 20 ms.
- **d. Limiter.** No lookahead, because lookahead adds latency. It uses an instant-attack, ~50 ms-release gain computer, followed by a hard clamp at -1 dBFS that guarantees the ceiling. Each block reports whether the limiter engaged or the signal clipped, for telemetry.
- **e. Test signals**, generated on the output thread and added after the low-pass:
  - **The pulse** is a 40 Hz sine with a 10 ms fade in and out. It plays from `pulseStart` until `pulseStop`, with a hard 10 s limit enforced by the control thread.
  - **The sweep** steps 20→120 Hz in 5 Hz steps of 0.5 s (21 steps, 10.5 s), with 10 ms fades at the ends. The oscillator's phase stays continuous when the frequency changes between steps, so step changes don't click.
  - **Only one test plays at a time.** The current sweep frequency goes to telemetry as `sweepHz`.
- **f. Output channels.** The mono result is written to device channels 1 and 2; all other channels are zero-filled. Per-shaker routing is a future extension of this stage.

*Alternatives considered:* FIR filters, rejected because their latency is too high at 20 Hz cutoffs. Placing the subsonic filter before the test-signal insertion was also rejected, because then test signals would bypass the hardware protection.

### D4. Output drivers behind one interface

`OutputDevice` has two implementations, `WasapiExclusiveOutput` and `AsioOutput`. Both:

- open at the device's native rate and sample format (WASAPI: `IsFormatSupported` probing in the order float32, int32, int24-in-32, int16; ASIO: the driver's current rate and sample type);
- report their channel count, so the UI can show "stereo / 5.1 / 7.1 / N ch" and the "outputs 1–2 of N" remark;
- call one shared `render(float* mono, frames)` function, the DSP chain from D3, then convert to the device format and write channels 1–2;
- report the callback time to the watchdog for stall detection.

WASAPI exclusive uses event-driven mode with the buffer period computed from the chosen sample count (aligned per `AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED`). ASIO uses the Steinberg SDK host code. The control thread loads exactly one ASIO driver at a time.

### D5. Ring buffer, drift control and the automatic reserve

- **The ring buffer** is a lock-free single-producer, single-consumer buffer of mono floats, sized for 200 ms and preallocated.
- **The automatic reserve** is the audio kept buffered at the low point of the capture packet cycle, just before the next packet arrives. It starts at 3 ms when the engine starts. Each underrun raises it by 1 ms, up to 6 ms. It isn't saved and resets on Start. There's no user setting.
- **The fill is a sawtooth.** Capture adds ~10 ms packets and output drains small blocks, so the fill (measured after each read) steps down from `reserve + packet − block` to `reserve`. Its average, `reserve + (packet − block) / 2`, is the controller's target.
- **The resampler** uses a variable ratio and linear interpolation. The signal is band-limited to 200 Hz, so interpolation errors fall far above the cutoff and get filtered out. Ratio = `captureRate / outputRate × (1 + trim)`.
- **A PI controller** on the fill, smoothed with a 0.5 s time constant (which reduces the sawtooth ripple to about 0.03 ms), produces `trim`, clamped to ±1000 ppm. "Locked" means the smoothed error stays within ±0.25 ms for 2 s. Telemetry reports `trim` in ppm as "clock drift".
- **One output-side class (`InputStage`)** owns the resampler, the controller and the reserve, and runs three states:
  - **Priming** plays silence until the ring holds `reserve + packet`, drops any excess (the planned catch-up, not a dropout), and starts playing in the same block with a short fade-in.
  - **Playing** resamples at the trimmed ratio.
  - **Starved** means the ring ran dry. If the capture side reported silence meanwhile, it was a pause. Otherwise it was an underrun.
- **Capture idle (silence).** The ring drains, the stage waits in Starved with the controller frozen (the learned trim is kept), and primes again when audio returns, so latency after silence equals latency before.
- **Underruns.** The dropout is counted and the reserve grows, then the stage primes at the new level during the same gap. So growing the reserve adds no further interruption.
- **Overruns.** If the ring is full when the capture thread writes (for example after an output stall), the capture side drops that audio and raises a counter. The output fades out over 2.5 ms, clears the ring, and primes from fresh, continuous audio, because the dropped audio left a splice inside the ring. The reserve does not grow, because there was too much buffered, not too little.
- **Excess.** If the fill grows 40 ms beyond the prime level without an overrun, the output fades out, drops back to the prime level and fades in.
- **Dropouts.** One user-facing counter. It counts underruns, overruns and capture packets flagged `AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY`, but not the planned catch-up after silence. The three causes are still counted separately inside the engine and written to the debug log, for diagnosis.

*Alternatives considered:* a user-selectable latency target. It was removed after review because users read "2 ms" as the total latency, and the 4 ms range of difference is hard to feel. Drift correction by dropping or repeating samples was also rejected, because it produces tactile ticks.

### D6. Lifecycle, changes while running, and test mode

```
 Stopped --start--> Starting --ok--> Running <--source/device back-- Waiting
    ^                  |              |   ^                            ^
    |                  | fail         |   +--- done ---+               |
    |                  v              v                |   app/capture/shaker missing
    +-----stop----- Error <--fail-- Switching ---------+
                     | retry 3 s / device-arrived event
                     +--> Starting
 (any) --feedback detected--> Stopped + Check output
```

- **Missing vs failing.** A missing app, capture device or shaker device means **Waiting**; it's resolved by session and device notifications. In-use, unsupported format, buffer refused, ASIO failure, stall and unsupported per-app capture mean **Error**, with automatic retry every 3 s and on `OnDeviceStateChanged` / `OnDeviceAdded`.
- **A source change** opens the new capture *before* closing the old one, then fades the ring input out and in around a pointer swap. Output keeps running.
- **An output change** fades out over 10 ms, stops and releases the device, opens the new one, starts it and fades in.
- **The watchdog** checks the output callback timestamp every 100 ms. More than 500 ms without a callback while Running is treated as a stall and restarts the output.
- **The feedback check** compares the resolved System mix capture endpoint ID with the shaker endpoint ID. For ASIO it compares by the underlying device when it can be determined. App mode is never checked, because it can't loop back.
- **Test mode (engine stopped).** `pulseStart` or `sweep` makes the control thread open only the output device, without capture, run the DSP with a silent input plus the test signal, and release the device when the test ends. The header state stays Stopped. If opening fails, a short-lived `testMsg` (about 4 s) goes to the UI instead of entering Error.

### D7. UI host and message bridge

- **WebView2** is hosted in a plain Win32 window. The UI files (`ui/index.html`, `app.css`, `app.js`, split from the mockup) are embedded as resources and served through `SetVirtualHostNameToFolderMapping` during development, or a `WebResourceRequested` handler in release builds, at `https://app.graviton/`.
- **Messages** are JSON in both directions: `PostWebMessageAsJson` and `window.chrome.webview.postMessage`. The update and link commands (`checkUpdates`, `openUrl`) and `state.update` are described in D10.
  - **From the host:**
    - `state`: a full snapshot on every change, including `phase`, `errorMsg`, `testing`, `pulsing`, `sweeping`, `testMsg`, `captureSel`, `process`, `shakerDev`, the presets and all settings.
    - `devices`: on enumeration changes; active endpoints, running audio apps and shaker devices.
    - `telemetry` at 30 Hz: `inDb`, `outDb`, `latencyMs`, `driftPpm`, `locked`, `fill`, `dropouts`, `limiterHit`, `noSignal`, `spectrum.in[64]`, `spectrum.out[64]`, `sweepHz`, and `test`, which is true for output-only test telemetry so the UI leaves engine health alone.
  - **From the UI:** `start`, `stop`, `set{key,value}`, `pulseStart`, `pulseStop`, `sweep`, `refreshProcesses`, `resetSettings`, `presetSelect`, `presetRevert`, `presetSave`, `presetCreate`, `presetDuplicate`, `presetRename` and `presetDelete`.
- **Ownership:** the host owns settings and presets. The UI only renders `state` and sends intents. The mockup's mock host and its "Simulate" controls become browser-only test scaffolding, active only when `window.chrome.webview` is absent and not shipped in the Release UI.
- **When hidden or minimized**, the controller is made invisible (`put_IsVisible(FALSE)` and `TrySuspend`), and telemetry posting stops.

### D8. Settings and system integration

- **Settings file:** `settings.json` with a `version` field, written with nlohmann::json. Saves are debounced by 300 ms on the UI thread and written atomically (write a `.tmp` file, then `ReplaceFileW`). On a parse failure, the bad file is renamed to `.bak` and the app falls back to defaults, merging any fields it could still read. The path isn't shown in the UI.
- **Start with Windows:** the `HKCU\...\Run` value `GravitonFX` = `"<exe>" --autostart`, where `--autostart` combined with "start minimized" means the window isn't created visibly.
- **Tray:** `Shell_NotifyIcon` with an Open / Start-Stop / Quit menu, and a tooltip showing the state. The tray icon is swapped with `NIM_MODIFY` whenever the engine state changes (running, stopped, waiting, error).
- **Icons:** concept A ("well and ball"), chosen on the icon sheet at https://claude.ai/artifact/N5EmvG6ZsjYu8vfWNjf85W.
  - **Sources:** two SVGs in `assets/icon/`, a full drawing and a simplified one for 24 px and below.
  - **Generated files:** a small script (`tools/make_icons.py`, run by hand when the artwork changes) renders `app.ico` (16, 20, 24, 32, 48, 64, 128, 256 px, with the simplified drawing at 24 px and below) and four tile-less tray icons (`tray_running.ico`, `tray_stopped.ico`, `tray_waiting.ico`, `tray_error.ico`, at 16, 20, 24 and 32 px). The `.ico` files are committed.
  - **In the build:** they're embedded through the `.rc` resource file, so the build itself needs no image tools. The app icon is set on the window class and the executable; tray icons are loaded with `LoadIconMetric` at the system's small-icon size.
- **Single instance:** a named mutex. A second launch finds the first instance's window and sends it a message to show itself.

### D9. Project layout and build

```
 CMakeLists.txt, CMakePresets.json, vcpkg.json (webview2, wil, nlohmann-json, doctest)
 LICENSE (GPL-3.0), README.md, docs/screenshot.png, .gitignore
 src/
   app/      main.cpp, MainWindow, Tray, SingleInstance, Autostart
   host/     WebViewHost, MessageBridge, Settings, Presets
   engine/   Engine (control thread + state machine), Commands, Telemetry, TestMode
   capture/  LoopbackCapture, ProcessCapture, DeviceEnumerator, SessionWatcher
   dsp/      Biquad, Butterworth (LP/HP), FilterChain, Limiter, Resampler, DriftController,
             Reserve, TestSignals (pulse, stepped sweep), SpscRing
   output/   OutputDevice, WasapiExclusiveOutput, AsioOutput
   analysis/ Analyzer (FFT + bands + levels + no-signal)
 ui/         index.html, app.css, app.js (from the mockup), fonts/ (OFL .woff2 files + licenses)
 installer/  GravitonFX.iss (Inno Setup), WebView2 bootstrapper download step
 assets/icon/  app-full.svg, app-small.svg, app.ico, tray_*.ico (generated, committed)
 tools/      make_icons.py
 third_party/asiosdk/   (downloaded separately, listed in .gitignore, never committed)
 tests/      dsp + ring + drift + reserve + test signals + settings + presets unit tests (doctest)
```

- **Toolchain:** MSVC with C++20, the static runtime, and warnings at /W4. It works with **Build Tools for Visual Studio** (2022 17.8+ or newer; the project owner's setup) or full Visual Studio, using the "Desktop development with C++" workload: MSVC, Windows SDK, C++ CMake tools (with Ninja) and vcpkg.
- **CMakePresets.json** defines `debug` and `release` presets (Ninja generator, vcpkg toolchain from `VCPKG_ROOT`). The commands are the same from "Developer PowerShell for VS" (`cmake --preset release`, `cmake --build --preset release`, `ctest --preset release`) and from VS Code's CMake Tools with the MSVC kit.
- **Debugging:** C++ with VS Code's `cppvsdbg`. In Debug builds the UI can be inspected with Edge DevTools (F12 opens `OpenDevToolsWindow`); DevTools are disabled in Release builds.
- **Fonts:** the UI's fonts are bundled as `.woff2` files under `ui/fonts/` with their OFL license files, and referenced with `@font-face`. The mockup's Google Fonts links are removed, so the UI makes no web requests.
- **The DSP, ring, drift, reserve, test-signal, settings and preset modules** have no Windows dependencies, so they can be unit-tested headless.

### D10. Update check

- **The checker** is an `UpdateChecker` on its own background thread (never an audio thread), using **WinHTTP** (built into Windows, no new dependency).
- **The request** is `GET https://api.github.com/repos/deuter0n/GravitonFX/releases/latest`, with headers `User-Agent: GravitonFX/<version>` and `Accept: application/vnd.github+json`, and a 15 s timeout. This endpoint already skips drafts and prereleases.
- **Comparison:** parse `tag_name` (strip the leading `v`) and compare major.minor.patch with the version compiled into the app (`GFX_VERSION`, set from CMake).
- **Schedule:** a single check about 10 s after launch, then every 24 h, only when `checkUpdates` is on. Check now runs immediately. One request at a time, and 60 unauthenticated requests per hour is far above what's needed.
- **Results** go into `state.update` (`status`: idle, checking, uptodate, available, error; `latest`; `current`). The last version the user was notified about (`notifiedVersion`) is saved in settings, so the Windows notification (`Shell_NotifyIcon` with `NIF_INFO`) appears once per version. Clicking it opens the release page.
- **Links:** one constant, `kRepoUrl = "https://github.com/deuter0n/GravitonFX"`, used by the checker, the About link and `openUrl`. `openUrl` opens a URL with `ShellExecuteW` only if it is `kRepoUrl` or starts with `kRepoUrl + "/"`; anything else is ignored.

### D11. Licensing

- **License:** GPL-3.0-or-later, in `LICENSE`. Every source file starts with `// SPDX-License-Identifier: GPL-3.0-or-later` and a copyright line.
- **Dependencies:**

| Component | License | With GPL-3.0 |
|---|---|---|
| WIL, nlohmann-json, doctest | MIT | compatible |
| Windows APIs (WASAPI, WinHTTP, Shell) | system libraries | covered by GPL's system-library exception |
| WebView2 SDK and runtime | Microsoft | the runtime is an OS component; check the SDK's license text once at setup |
| Steinberg ASIO SDK | Steinberg, with a GPLv3 option | compatible through that option; **verify the current terms before the first release**, and never commit the SDK |
| Chakra Petch, Inter, JetBrains Mono | SIL Open Font License 1.1 | bundling allowed; ship their license files |

- **Trademark:** "ASIO" is a Steinberg trademark. The UI and README use the name plainly, without implying endorsement.

### D12. Packaging and releases

- **Installer:** Inno Setup (`installer/GravitonFX.iss`), with `PrivilegesRequired=lowest` so it installs per user to `{localappdata}\Programs\Graviton FX`.
  - **Shortcuts:** a Start menu shortcut, plus an optional desktop shortcut.
  - **WebView2:** the installer checks the WebView2 runtime's registry key, and if it's missing, runs Microsoft's Evergreen bootstrapper silently.
  - **Upgrades:** `AppMutex` uses the app's single-instance mutex name, so the installer asks to close a running copy. The same `AppId` makes a newer installer upgrade in place; settings live in `%APPDATA%` and are untouched.
  - **Uninstall:** removes the program files, the shortcuts and the `HKCU\...\Run` value, then asks whether to delete `%APPDATA%\GravitonFX`.
- **Portable zip:** `GravitonFX-<version>-portable.zip` holds only `GravitonFX.exe`. On every start the app checks that its Run value, if present, points at its own path, and corrects it if the folder moved.
- **Releases:** a release is cut by building the Release preset, running the installer script, zipping the portable build, tagging `v<version>`, and creating a GitHub Release with both files attached (`gh release create`). These steps are documented in `RELEASING.md`. The version comes from one place (`project(... VERSION x.y.z)` in CMake) and feeds the app, the installer and the file names.
- **Signing:** builds are unsigned for now, and the README explains the SmartScreen prompt. Signing (for example SignPath's free program for open-source projects) and a winget listing are possible later.

## Risks / Trade-offs

- [The shared-mode loopback period (~10 ms) dominates latency and is controlled by Windows] → Accept it. Show the real end-to-end latency in the Engine health panel and keep every other part of the budget minimal.
- [Process loopback has sparse documentation: its latency and its behavior when the target app plays to a non-default device are unverified; Windows' conversion to 48 kHz may add about 1 ms] → Spike task 4.3 measures it before ProcessCapture is built. If it's noticeably slower, note it next to the App option.
- [Gentle subsonic slope lets some sub-20 Hz energy through on movie content] → The Steep option, the limiter as a final safety, and this is documented in the setting's description.
- [The automatic reserve only grows during a session, so one bad moment raises latency until the next Start] → Acceptable (max +3 ms). It could shrink back slowly after a long clean period in a later version.
- [ASIO SDK licensing for public GPL builds] → Rely on Steinberg's GPLv3 option and verify its current terms before the first release (D11). If it doesn't fit, release builds are made with `GFX_WITH_ASIO=OFF` (WASAPI exclusive only) and the README explains how to build with ASIO yourself.
- [Unsigned builds trigger SmartScreen] → Accepted for now; the README explains "More info › Run anyway". Signing can be added later.
- [GitHub API changes or rate limits] → Failures are silent for automatic checks and reported plainly for Check now. The app keeps working with no network at all.
- [Only one process can use a given ASIO driver] → Reported as an Error ("ASIO driver couldn't start, another audio app may be using it"), with automatic retry.
- [Linear-interpolation resampler aliasing] → Applied to a band-limited signal before the ≤200 Hz low-pass. A unit test verifies that images above the cutoff are below -60 dB after the filter.
- [Biquad coefficient modulation artifacts during fast sweeps or chart dragging] → Per-block smoothing plus TDF-II double precision, covered by a sweep test that checks for discontinuities.
- [A missed pointer release could leave the pulse playing] → A 10 s hard limit in the control thread, plus releases on blur and pointer-cancel in the UI.
- [WebView2 memory (~100 MB)] → Accepted. The WebView is suspended when hidden and has no effect on audio threads.

## Migration Plan

This is a new app, so nothing needs migrating. The plan is to build it in stages, with each stage building and running (see tasks.md). The settings `version` field leaves room for future migrations.

## Open Questions

- **Does exclude-mode process loopback** (all apps except Graviton) capture audio playing on non-default devices? Task 11.4 answers this. It only matters for a possible future "All apps" source.
- **Should the reserve shrink back** after a long period without underruns? This is deferred; it changes only the D5 internals and doesn't affect any spec.

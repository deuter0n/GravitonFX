# CLAUDE.md

Notes for AI coding assistants working on Graviton FX. The README covers what the app is and how to build it; this file covers how to work on it.

## The project

A Windows C++20 app: captures system or app audio (WASAPI loopback), low-pass filters it and plays it on a separate shaker device (WASAPI exclusive or ASIO). Lowest latency first; the audio path is real-time safe (no locks, allocations or waits). The UI is a WebView2 page. Settings are JSON in `%APPDATA%\GravitonFX`.

The specs live in OpenSpec: `openspec/specs/*/spec.md` describe what the app does now. v0.1.0 was built from the change archived at `openspec/changes/archive/2026-11-23-build-graviton-fx/`, whose `design.md` still explains the design decisions. New work starts as a new change in `openspec/changes/` (proposal, delta specs, design, tasks).

## Build and test

Use the Developer PowerShell environment (Build Tools for Visual Studio 2026). From a plain PowerShell:

```powershell
$vs = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\18\BuildTools"
Import-Module "$vs\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64"
cmake --build --preset debug; cmake --build --preset release
.\build\debug\tests\gfx_tests.exe
```

- A "'vswhere.exe' is not recognized" line from `Enter-VsDevShell` is harmless.
- Build both presets after every change; /W4 must stay free of warnings.
- The Release link fails with LNK1104 if `build\release\GravitonFX.exe` is running. Ask before closing the user's running copy.
- ASIO is built when the SDK is in `third_party/asiosdk` (git-ignored, never commit it); `cmake --preset release -B build/noasio -DGFX_WITH_ASIO=OFF` checks the build without it. `gfxprobe --list-asio` lists drivers. The dev PC has no ASIO driver installed unless the user adds one.
- `perl` (from Git for Windows) runs `tools/split_mockup.pl`; run it from Git Bash, because it isn't on PowerShell's PATH. There is no Python on the dev machine; don't add Python tools.

## How to work here

- **Keep the code simple and readable**, and make sure every step builds before moving on.
- **Update the specs and docs with every change**: the active change's delta specs, `design.md` and `tasks.md` (or the main specs in `openspec/specs/` for a small fix with no change), the README when user-facing, and this file when the workflow changes. Run `openspec validate --specs`, plus `openspec validate <change> --strict` when a change is active; each requirement's text must stay under 500 characters (split it, or move detail into scenarios).
- **Tick a task** in the change's `tasks.md` only when its verification has actually been done. Hardware checks (a real shaker, sign-out and sign-in) are done by the user.
- **Never play audio on the user's devices unattended.** Don't press Start, Pulse or Sweep, or run `gfxprobe --test-tone` or `--run`, unless the user asked for it.
- **Git:** commit with the existing author settings (a GitHub noreply address). Push only when the user says so.
- **Messages to the user:** short, simple and plain.

## The UI

- The mockup `mockup/index.html` is the single source of the UI. Edit it, then regenerate `ui/` with `perl tools/split_mockup.pl mockup/index.html ui`. Never edit `ui/` by hand.
- Parts of the mockup that exist only in the browser preview (the tray demo, the Simulate buttons, the mock host) go to `mock.js`, which the app never loads. Code in `app.js` that touches them must check first (`if ($("id")) ...`); the split script stops with an error otherwise, because an exception in `applyState` silently stops the rest of the dashboard from updating in the app.
- The approved mockup is also published as an artifact: https://claude.ai/artifact/PEqc2ovPpXaRoeVMEJtWXV. After a UI change:
  1. Copy the mockup to the publish path, as a separate step.
  2. Verify the copy (`cmp`, plus a grep for the new text).
  3. Publish to the same URL.
  4. Name the published version (from the publish result) in the commit message. The archived change's references to version 132 are history; leave them.
- The layout must fit a 16:9 screen with no vertical scrollbar: 720 CSS px tall, which is 1080p at 125 %. The minimum window is 1180 x 720 CSS px.
- Keep the UI minimal and consistent: few settings, plain words, one meaning per color. Error and status texts are short ("what happened. what to do."), with no device names (the selector already shows them).
- Animated views redraw at most 60 times a second and stop completely when stopped and settled. Keep that when adding animation.

## Releases

- `tools\package.ps1` builds `dist\GravitonFX-<version>-portable.zip` (and the installer when Inno Setup 7 is installed). `RELEASING.md` has the full steps. Publishing a release is outward-facing: only when the user asks.

## Icons

- The SVG sources are in `assets/icon/`. The archived change keeps a copy of the v0.1.0 set as history; don't update it.
- Regenerate the `.ico` files with `powershell -ExecutionPolicy Bypass -File tools\make_icons.ps1` (it uses headless Edge) and commit them. CMake rebuilds `app.rc` when an `.ico` changes.
- Every icon uses one layout, the ball sitting inside the rings seen from above, with no tile, centered in its square: three rings above 24 px, two at 24 px and below, and in the tray. The app icon and the header logo shade the ball only; tray icons are flat. The header logo and the tray demo in the mockup draw the same shapes at half scale; keep them in step with the SVGs.

## Testing tips

- **Driving the real app:** set `WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS=--remote-debugging-port=9333` before launching, then use the Chrome DevTools Protocol (e.g. from Node) to evaluate in the page. Send UI commands as objects, like the page does: `chrome.webview.postMessage({cmd:'windowMinimize'})`. The host ignores strings. If Edge already listens on that port (on 127.0.0.1), pick another one: the WebView listens on `[::1]`. The page pauses while the window is hidden or covered, so ask the user to keep it visible; commands sent meanwhile run when it shows again.
- **Headless Edge** throttles timers and animation frames, so it can't measure frame rates or the still-when-stopped logic. Measure those in the real app.
- **The user's real settings file** (`%APPDATA%\GravitonFX\settings.json`) is what the app uses. Back it up before a test that changes settings, and restore it afterwards.
- **Builds inside the repository** never write the `HKCU\...\Run` value. To test autostart, run a copy from outside the repository, and remove the value afterwards.
- **Update check:** Debug builds read `GFX_UPDATE_TEST_URL` to use a local mock server instead of GitHub.
- **PowerShell P/Invoke:** `$null` becomes `""` for string parameters; pass `[NullString]::Value` (for example to `FindWindowW`).

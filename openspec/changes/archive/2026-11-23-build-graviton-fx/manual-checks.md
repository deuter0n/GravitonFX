# Manual checks (task 11.3)

Every scenario in `specs/engine-lifecycle`, `specs/audio-capture` and `specs/shaker-output`, as steps to do in the real app. Mark each one: ✅ matches the spec, ❌ deviation (write what happened), or – can't test here.

Devices on the dev PC: the speakers (Cirrus Logic, Windows default), the JadeAudio JA11 (WASAPI shaker), the FiiO (ASIO shaker).

**Who does it:** **You** means you have to touch hardware or the desktop. **Claude** means it can be driven from the console. Every step plays audio on the shaker, so only do them while you're at the PC.

Done on 2026-11-22/23 with the Release build, driven over DevTools, with a looping game-like test file and the speakers muted. Results marked ✅ after a fix were checked again in the fixed build.

## A. Start, stop and the basics (Claude)

| # | Do | Expect | Result |
|---|---|---|---|
| A1 | Press Start (System mix, JadeAudio) | STARTING, then RUNNING; Settling, then Locked within 10 s | ✅ Locked after 9.6 s |
| A2 | Press Stop | Output fades out, STOPPED, device released | ✅ after fix 1 |
| A3 | Stop and Start again after a dropout | Reserve is back at 3 ms | – no dropout could be caused; covered by unit tests |
| A4 | Hold Pulse while stopped | Shaker vibrates, status stays STOPPED | ✅ |
| A5 | Sweep while stopped | Same: plays, status stays STOPPED | ✅ after fix 4 |
| A6 | Running, hide the window to the tray | Output continues without a break | ✅ 0 dropouts |
| A7 | Running, watch the health panel | Latency, drift, fill, dropouts, lock update smoothly (≥ 25/s) | ✅ 31.6/s after fix 2 |
| A8 | Rate and format tags | Source: "48 kHz", "32-bit float", "2 ch"; Shaker: its own tags; Rate "48 native" | ✅ Shaker "48 kHz", "32-bit", "2 ch" |
| A9 | Buffer slider at 64 on a 48 kHz device | Reads "+1.33 ms"; with ASIO the slider also has 8 and 16 | ✅ and 16 → WASAPI changes to 32 |

## B. Source (Claude, some You)

| # | Do | Expect | Result |
|---|---|---|---|
| B1 | System mix, play audio from two apps | Both reach the shaker | – not checked separately (System mix captures the whole device) |
| B2 | App mode, pick one app, play another too | Only the picked app reaches the shaker | ✅ silent app picked: "No signal" while the other played |
| B3 | App mode for the first time, nothing chosen | An app playing audio gets selected, or "Pick an app" + "Pick an app to capture." | ✅ the playing app was selected |
| B4 | Start a new app playing audio, press refresh | It appears in the app list | ✅ |
| B5 | App mode, close the app while Running | WAITING, "Not running. Capture starts when it opens."; back to RUNNING when it plays again | ✅ after fix 3 |
| B6 | App running but silent | Status stays RUNNING | ✅ |
| B7 | All audio stops for 1 minute, then resumes | Silence on the shaker, stays RUNNING, same latency after, Dropouts unchanged | ✅ 28.6 ms before and after, 0 dropouts |
| B8 | **You:** "Windows default", change the default output in Windows while Running | Capture moves to the new default, no restart | ✅ followed it; the new default was the shaker, so CHECK OUTPUT, cleared to STOPPED when changed back |
| B9 | **You:** fixed device, change the Windows default | Capture stays on the fixed device | ✅ |
| B10 | Open the capture device list | "Windows default" + active outputs; hover shows "Current default: …" | ✅ |
| B11 | **You:** unplug the fixed capture device while Running, then plug it back | "Not connected. Capture resumes when it's back.", then resumes; stays listed until another is picked | – the only other output is the shaker |
| B12 | No problem with the source | No message under the source selector | ✅ |

## C. Shaker output (mostly You)

| # | Do | Expect | Result |
|---|---|---|---|
| C1 | Switch the driver to ASIO | List shows the FiiO, first available one selected | ✅ Locked after 10 s |
| C2 | **You:** unplug the JadeAudio while Running, then plug it back | Stays listed, "Not connected. Output starts when it's back." with a blue dot; output starts again by itself | ✅ |
| C3 | **You:** JadeAudio switched off, start the app (or the engine) | WAITING, output starts when it's switched on | ✅ |
| C4 | Hold the JadeAudio in exclusive mode with another app, press Start | ERROR, "In use by another app. Close it or pick another device." | ✅ held by `gfxprobe --run` |
| C5 | Release it again | Back to RUNNING within 3 s, no click needed | ✅ about 2.5 s |
| C6 | Change the shaker device while Running | SWITCHING, old released, new opens, Settling, then Locked | ✅ via the driver switch to the FiiO (the other WASAPI device is the laptop speaker) |
| C7 | Change the capture device while Running | SWITCHING, short fade, back to RUNNING | – the only other capture device is the shaker; source mode switches stayed RUNNING |
| C8 | Pick the same device for capture and shaker | CHECK OUTPUT, feedback warning, Start disabled | ✅ |
| C9 | App mode with the shaker = the Windows default output | No feedback warning, runs | ✅ (no warning in App mode) |
| C10 | Running, Windows plays a notification sound | Not heard on the shaker | – the default is another device; exclusivity shown by C4 |
| C11 | Pick a buffer size the device refuses (JadeAudio: 64) | ERROR, "Buffer size not supported. Pick a larger one." under the device selector, knob turns red | ✅ |
| C12 | **You:** one shaker on the right amp channel only | Gets the full signal | ✅ checked with earphones on the JadeAudio: Pulse equally loud left and right |

## D. Hard to produce here (note why, or skip)

| # | Scenario | Why | Result |
|---|---|---|---|
| D1 | Output stall watchdog ("Stopped responding.") | Needs a driver that hangs; covered by unit tests | – |
| D2 | UI blocked for 2 s, no underruns | Can be done with DevTools (a busy loop in the page) | ✅ 0 dropouts |
| D3 | 44.1 kHz-only shaker, "48 → 44.1" | No such device here | – |
| D4 | 8-output ASIO interface, "out 1–2" | No such device here | – |
| D5 | Windows older than 10 2004 | Not available | – |
| D6 | Overrun counted, reserve doesn't grow | Covered by unit tests | – |
| D7 | ASIO driver in use (10.3) | The FiiO driver allows several apps at once | – |

## Deviations found

All fixed in commit `5621758` and checked again in the app:

1. **A2, C4, C11, B5:** after Stop, an error or a wait, the health panel showed old values ("LOCKED 8.0 ms") instead of IDLE/ERROR/WAITING and "--". The last telemetry arrived after the state change and painted over the reset. The UI now applies engine telemetry only while running.
2. **A7:** the health panel updated 21 times a second (spec: at least 25). Windows rounded the 33 ms timer up to 46.9 ms; it's 30 ms now (31.25 ms, 32/s).
3. **B5:** when the captured app closed, the status went to WAITING but "Not running. Capture starts when it opens." didn't show: the app list was only refreshed on device changes. It's now refreshed when the engine waits or resumes in App mode.
4. **A5 (found by feel):** each sweep step made the level jump about 0.3 dB for 50 ms (the subsonic filter overshoots when the frequency jumps). Each step now glides to its frequency over 100 ms.

Not changed: the limiter crackles on loud bass hits (no look-ahead, so it flattens the first peaks). Look-ahead would cut it, but costs latency; the user chose latency.

/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 deuter0n
   Generated from the approved mockup by tools/split_mockup.pl.

   Browser preview only: a simulated host with fake devices and audio, the
   "Simulate" buttons and a tray demo, so the UI can be designed and tested
   without the C++ engine. The app never loads this file. */
"use strict";
(() => {
  if (window.chrome && window.chrome.webview) return;  // running inside the app

  const $ = (id) => document.getElementById(id);
  const setText = (id, txt) => { const el = $(id); if (el.textContent !== txt) el.textContent = txt; };

  const stage = document.querySelector(".stage");
  stage.insertAdjacentHTML("beforeend", `<p class="note"><b>Interactive mockup.</b> The audio is simulated. Try the knobs, presets, Start/Stop, Settings and closing to the tray.</p>`);
  stage.insertAdjacentHTML("beforeend", `<div class="demo" aria-label="Simulate devices (mockup only)">
    <span>Simulate</span>
    <button class="btn-sm" id="demoCap">Unplug capture device</button>
    <button class="btn-sm" id="demoApp">Close selected app</button>
    <button class="btn-sm" id="demoOut">Unplug shaker device</button>
    <button class="btn-sm" id="demoMute">Stop all sound</button>
    <button class="btn-sm" id="demoRelease">New release on GitHub</button>  </div>`);
  document.body.insertAdjacentHTML("beforeend", `<div class="tray" id="tray" hidden>
  <svg viewBox="0 0 32 32" aria-hidden="true">
    <ellipse cx="16" cy="17.5" rx="14.5" ry="9.25" fill="none" stroke="#ff9a3c" stroke-opacity=".6" stroke-width="3"/>
    <ellipse cx="16" cy="19.5" rx="7.5" ry="4" fill="none" stroke="#ff9a3c" stroke-opacity=".9" stroke-width="3"/>
    <circle cx="16" cy="10.5" r="6.75" fill="#ff9a3c"/>
  </svg>
  <p>Graviton FX is in the tray<span id="trayState">Engine stopped</span></p>
  <button id="restoreBtn">Open</button>
</div>`);

  function toTray() { $("app").classList.add("to-tray"); $("tray").hidden = false; $("restoreBtn").focus(); }
  function fromTray() { $("app").classList.remove("to-tray"); $("tray").hidden = true; $("powerBtn").focus(); }
  $("restoreBtn").addEventListener("click", fromTray);
  window.mockToTray = toTray;  // (the tray demo above already wires its Open button)

  window.mockRender = (s) => {
    setText("demoCap", s.demo.capLabel); $("demoCap").disabled = !s.demo.capEnabled;
    setText("demoApp", s.demo.appLabel); $("demoApp").disabled = !s.demo.appEnabled;
    setText("demoOut", s.demo.outLabel);
    setText("demoMute", s.demo.muteLabel);
    setText("demoRelease", s.demo.releaseLabel);
  };

  window.createMockHost = () => {
    const host = createMockHost();
    const send = host.send;
    $("demoCap").addEventListener("click", () => send({ cmd: "demoCapture" }));
    $("demoApp").addEventListener("click", () => send({ cmd: "demoApp" }));
    $("demoOut").addEventListener("click", () => send({ cmd: "demoShaker" }));
    $("demoMute").addEventListener("click", () => send({ cmd: "demoSilence" }));
    $("demoRelease").addEventListener("click", () => send({ cmd: "demoRelease" }));
    return host;
  };

function createMockHost() {
  const listeners = [];
  const emit = (m) => listeners.forEach((fn) => fn(m));

  const devices = {
    // Output devices System mix can tap. Loopback always delivers the engine's shared-mode format.
    renderEndpoints: [
      { id: "ep-realtek", name: "Speakers (Realtek(R) Audio)", endpoint: "realtek", rate: 48000, fmt: "32-bit float · 2 ch" },
      { id: "ep-headset", name: "Headphones (USB Headset)", endpoint: "headset", rate: 48000, fmt: "32-bit float · 2 ch" },
      { id: "ep-rig", name: "Rig Speakers (USB Audio)", endpoint: "rig", rate: 48000, fmt: "32-bit float · 2 ch" },
      { id: "ep-usbdac", name: "Speakers (USB Audio DAC)", endpoint: "usbdac", rate: 44100, fmt: "32-bit float · 2 ch" },
      { id: "ep-iface", name: "Line Out (Audio Interface)", endpoint: "iface", rate: 48000, fmt: "32-bit float · 2 ch" },
    ],
    defaultEp: "ep-realtek",
    // Apps currently holding an audio session; `w` shapes the simulated content
    processes: [
      { exe: "RaceSim.exe", name: "Race Sim", playing: true, w: { kick: 0.25, engine: 1, kerb: 1, mids: 0.6, highs: 1 } },
      { exe: "Spotify.exe", name: "Spotify", playing: true, w: { kick: 1, engine: 0, kerb: 0, mids: 1, highs: 1 } },
      { exe: "chrome.exe", name: "Google Chrome", playing: true, w: { kick: 0.5, engine: 0, kerb: 0, mids: 1, highs: 0.8 } },
      { exe: "Discord.exe", name: "Discord", playing: false, w: { kick: 0, engine: 0, kerb: 0, mids: 0, highs: 0 } },
    ],
    wasapi: [
      { id: "out-usbdac", name: "Speakers (USB Audio DAC)", endpoint: "usbdac", rate: 44100, fmt: "24-bit · 2 ch" },
      { id: "out-lineout", name: "Line Out (Audio Interface)", endpoint: "iface", rate: 48000, fmt: "24-bit · 2 ch" },
      { id: "out-realtek", name: "Speakers (Realtek(R) Audio)", endpoint: "realtek", rate: 48000, fmt: "24-bit · 2 ch" },
    ],
    asio: [
      { id: "asio-iface", name: "Audio Interface ASIO Driver", endpoint: "iface", rate: 48000, fmt: "24-bit · 8 ch", ch: 8 },
      { id: "asio-flex", name: "FlexASIO", endpoint: "flex", rate: 48000, fmt: "32-bit float · 2 ch" },
    ],
  };

  // presence flags drive the "Simulate" buttons (unplug / close app)
  [...devices.renderEndpoints, ...devices.wasapi, ...devices.asio].forEach((d) => { d.present = true; });
  devices.processes.forEach((p) => { p.running = true; });

  const DEFAULTS = {
    sourceMode: "system",        // "system" | "process"
    captureDevice: "default",    // "default" (follow Windows) or a saved endpoint id
    processExe: "RaceSim.exe",   // remembered by executable name, not PID
    driver: "wasapi",
    shakerId: "out-usbdac",
    buffer: 64,
    startWithWindows: true,
    startMinimized: false,
    autoStart: false,
    closeToTray: true,
    checkUpdates: true,          // look for a new GitHub release after launch and daily
    limiter: true,
    subsonic: 20,                // high-pass in Hz, 0 = off; 20 Hz = bottom of a typical 20-200 Hz shaker
    subsonicSlope: 12,           // 12 (gentle, default for games) or 24 dB/oct (steep)
  };
  const s = {
    running: false,
    ...DEFAULTS,
    cutoff: 80, slope: 24, gain: 0,
    presets: [{ id: "default", name: "Default", builtIn: true, cutoff: 80, slope: 24, gain: 0 }],
    activePresetId: "default",
  };

  // The real host keeps presets in settings.json; the mock keeps them in this browser.
  const STORE = "gfx-mock-presets";
  try {
    const saved = JSON.parse(localStorage.getItem(STORE) || "null");
    if (saved && Array.isArray(saved.presets)) {
      s.presets = [s.presets[0], ...saved.presets.filter((p) => !p.builtIn)];
      const act = s.presets.find((p) => p.id === saved.activePresetId) || s.presets[0];
      s.activePresetId = act.id;
      s.cutoff = act.cutoff; s.slope = act.slope; s.gain = act.gain;
    }
  } catch (e) { /* storage unavailable: start with Default only */ }
  const persist = () => {
    try { localStorage.setItem(STORE, JSON.stringify({ presets: s.presets, activePresetId: s.activePresetId })); } catch (e) {}
  };
  const preset = (id) => s.presets.find((p) => p.id === id);
  const applyPreset = (p) => { s.activePresetId = p.id; s.cutoff = p.cutoff; s.slope = p.slope; s.gain = p.gain; };

  let startedAt = 0, underruns = 0, pulseOn = false, pulseHoldTimer = 0;
  let allSilent = false;   // mockup-only: "Stop all sound"
  // update check (the real host asks api.github.com/repos/deuter0n/GravitonFX/releases/latest)
  const APP_VERSION = "0.1.0";
  let latestRelease = "0.1.0";                       // mockup-only: what "GitHub" reports
  const upd = { status: "idle", latest: null };      // idle | checking | uptodate | available | error
  const newer = (a, b) => {                          // semantic version compare: is a newer than b?
    const pa = a.split(".").map(Number), pb = b.split(".").map(Number);
    for (let i = 0; i < 3; i++) if ((pa[i] || 0) !== (pb[i] || 0)) return (pa[i] || 0) > (pb[i] || 0);
    return false;
  };
  function runUpdateCheck() {
    upd.status = "checking";
    emit(stateMsg());
    setTimeout(() => {
      upd.latest = latestRelease;
      upd.status = newer(latestRelease, APP_VERSION) ? "available" : "uptodate";
      emit(stateMsg());
    }, 900);
  }
  // test pulse while stopped: the output opens briefly on its own
  let testUntil = 0, testOnly = false, testMsg = null, testPanel = "shaker", testMsgT = 0;
  // stepped sweep test: 20, 25, 30 … 200 Hz, each held for 0.5 s (37 steps, 18.5 s)
  // (the real engine keeps the sine's phase continuous between steps, so steps don't click)
  let sweepStart = -100, sweepTimer = 0;
  const SWEEP_STEP_HZ = 5, SWEEP_STEP_S = 0.5;
  const SWEEP_STEPS = (200 - 20) / SWEEP_STEP_HZ + 1;
  const SWEEP_DUR = SWEEP_STEPS * SWEEP_STEP_S;
  const sweepPos = (tSec) => tSec - sweepStart;
  const sweepActive = (tSec) => sweepPos(tSec) >= 0 && sweepPos(tSec) < SWEEP_DUR;
  const sweepHzAt = (tSec) => {
    const step = Math.min(SWEEP_STEPS - 1, Math.max(0, Math.floor(sweepPos(tSec) / SWEEP_STEP_S)));
    return 20 + step * SWEEP_STEP_HZ;
  };

  // Engine phases while running: null = streaming, "starting" / "switching" = (re)opening, "error" = open failed.
  // Timings are a little longer than the real engine so the states are visible.
  let phase = null, errorMsg = null, errorPanel = "shaker", switchTimer = 0, retryTimer = 0;
  const HEAVY_OUTPUT = ["shakerId", "driver", "buffer"];
  const HEAVY_SOURCE = ["sourceMode", "captureDevice", "processExe"];
  // simulated open failures: another app holds the line-out (WASAPI), and the ASIO driver refuses 8 samples
  const openFailure = () =>
    s.driver === "wasapi" && s.shakerId === "out-lineout" ? { msg: "In use by another app. Close it or pick another device.", panel: "shaker" }
    : s.driver === "asio" && s.buffer === 8 ? { msg: "Buffer size not supported. Pick a larger one.", panel: "buffer" }
    : null;
  // can the shaker output be opened on its own for a test signal (engine stopped)?
  const testBlocked = () => !shaker().present ? { msg: "Not connected.", panel: "shaker" } : openFailure();

  function beginSwitch(ms, kind) {
    clearTimeout(switchTimer); clearTimeout(retryTimer);
    phase = kind || "switching"; errorMsg = null;
    switchTimer = setTimeout(() => {
      if (!s.running) return;
      const fail = openFailure();
      if (fail) {
        phase = "error";
        errorMsg = fail.msg; errorPanel = fail.panel;
        retryTimer = setTimeout(() => { if (s.running && phase === "error") beginSwitch(450); }, 3000);
      } else {
        phase = null;
        startedAt = performance.now();   // new clocks: drift control settles again
      }
      emit(stateMsg());
    }, ms);
  }
  function endSwitch() { clearTimeout(switchTimer); clearTimeout(retryTimer); phase = null; errorMsg = null; }

  const find = (list, id) => devices[list].find((d) => d.id === id);
  const procEntry = () => devices.processes.find((p) => p.exe === s.processExe);
  const proc = () => { const p = procEntry(); return p && p.running ? p : null; };
  // process loopback hands us whatever format we ask for; we ask for 48 kHz float stereo
  const selectedEp = () => devices.renderEndpoints.find((e) =>
    e.id === (s.captureDevice === "default" ? devices.defaultEp : s.captureDevice));
  const captureEp = () => { const e = selectedEp(); return e && e.present ? e : null; };
  const capture = () => {
    if (s.sourceMode === "process") return { name: s.processExe, rate: 48000, fmt: "32-bit float · 2 ch" };
    const e = selectedEp();
    return captureEp() || { name: e ? e.name : "Saved device", missing: true, rate: 48000, fmt: "32-bit float · 2 ch" };
  };
  const shaker = () => find(s.driver, s.shakerId);
  // only System mix can feed back: one app's stream never contains our own output
  const feedback = () => {
    const ep = s.sourceMode === "system" && captureEp();
    return !!ep && shaker().present && ep.endpoint === shaker().endpoint;
  };

  // running, but something selected isn't there: app not started / a device unplugged
  const sourceMissing = () => s.sourceMode === "process" ? !proc() : !captureEp();
  const waiting = () => sourceMissing() || !shaker().present;

  function demoInfo() {
    const unplugged = devices.renderEndpoints.find((e) => !e.present);
    const pe = procEntry();
    const sh = shaker();
    return {
      capLabel: unplugged ? `Plug in ${unplugged.name}` : "Unplug capture device",
      capEnabled: s.sourceMode === "system" || !!unplugged,
      appLabel: pe && !pe.running ? `Relaunch ${pe.name}` : "Close selected app",
      appEnabled: s.sourceMode === "process" && !!pe,
      outLabel: sh.present ? "Unplug shaker device" : `Plug in ${sh.name}`,
      muteLabel: allSilent ? "Play sound again" : "Stop all sound",
      releaseLabel: latestRelease === APP_VERSION ? "New release on GitHub" : "Remove the new release",
    };
  }

  function stateMsg() {
    const pe = procEntry();
    const se = selectedEp();
    return {
      type: "state", ...s, presets: s.presets.map((x) => ({ ...x })),
      feedback: feedback(), captureDev: capture(), shakerDev: { ...shaker() },
      phase: phase || (s.running && waiting() ? "waiting" : null), errorMsg,
      errorPanel, testPanel, retrying: phase === "error",
      testing: !s.running && performance.now() < testUntil,
      sweeping: sweepActive(performance.now() / 1000),
      pulsing: pulseOn,
      testMsg: testMsg && performance.now() - testMsgT < 4000 ? testMsg : null,
      // the current selection, even when it's missing, so the UI can still name it
      captureSel: { id: s.captureDevice, name: se ? se.name : "Saved device", present: !!captureEp() },
      process: pe ? { exe: pe.exe, name: pe.name, running: pe.running, playing: pe.playing } : { exe: s.processExe, name: s.processExe, running: false },
      demo: demoInfo(),
      update: { status: upd.status, latest: upd.latest, current: APP_VERSION },
    };
  }
  function devicesMsg() {
    // only what's connected or running right now; no device history
    return {
      type: "devices",
      renderEndpoints: devices.renderEndpoints.filter((e) => e.present).map(({ id, name, endpoint }) => ({ id, name, endpoint })),
      defaultId: devices.defaultEp,
      processes: devices.processes.filter((p) => p.running).map(({ exe, name, playing }) => ({ exe, name, playing })),
      shaker: devices[s.driver].filter((d) => d.present),
    };
  }

  function send(m) {
    switch (m.cmd) {
      case "start":
        if (feedback()) break;
        s.running = true; startedAt = performance.now(); underruns = 0; cushionMs = 3;
        beginSwitch(350, "starting");
        break;
      case "stop":
        s.running = false;
        sweepStart = -100;
        pulseOn = false; clearTimeout(pulseHoldTimer);
        endSwitch();
        break;
      case "sweep": {
        const now = performance.now();
        if (sweepActive(now / 1000)) {                 // pressing again stops it
          sweepStart = -100;
          if (!s.running) testUntil = 0;
          break;
        }
        if (!s.running) {
          const fail = testBlocked();
          if (fail) { testMsg = fail.msg; testPanel = fail.panel; testMsgT = now; setTimeout(() => emit(stateMsg()), 4050); break; }
          testUntil = now + SWEEP_DUR * 1000 + 100;    // open only the shaker output for the sweep
        }
        testMsg = null;
        sweepStart = now / 1000;
        clearTimeout(sweepTimer);
        sweepTimer = setTimeout(() => emit(stateMsg()), SWEEP_DUR * 1000 + 150);
        break;
      }
      case "set":
        s[m.key] = m.value;
        if (m.key === "driver") { s.shakerId = (devices[s.driver].find((d) => d.present) || devices[s.driver][0]).id; emit(devicesMsg()); }
        if (["shakerId", "sourceMode", "captureDevice"].includes(m.key) && feedback()) s.running = false;
        if (!s.running) endSwitch();
        else if (HEAVY_OUTPUT.includes(m.key)) beginSwitch(450);
        else if (HEAVY_SOURCE.includes(m.key)) beginSwitch(250);
        break;
      case "refreshProcesses":
        emit(devicesMsg());
        break;
      // --- mockup-only simulation of devices and apps coming and going ---
      case "demoCapture": {
        const unplugged = devices.renderEndpoints.find((e) => !e.present);
        if (unplugged) {
          unplugged.present = true;
          if (s.running && captureEp() === unplugged) beginSwitch(250);
        } else {
          const e = selectedEp();
          if (!e) break;
          e.present = false;
          if (s.captureDevice === "default") {
            // Windows picks a new default; capture follows it
            const next = devices.renderEndpoints.find((x) => x.present && x.endpoint !== shaker().endpoint);
            if (next) devices.defaultEp = next.id;
            if (s.running) beginSwitch(250);
          } else {
            endSwitch();
          }
        }
        emit(devicesMsg());
        break;
      }
      case "demoApp": {
        const pe = procEntry();
        if (!pe) break;
        pe.running = !pe.running;
        if (pe.running && s.running) beginSwitch(250); else if (!pe.running) endSwitch();
        emit(devicesMsg());
        break;
      }
      case "checkUpdates":
        runUpdateCheck();
        break;
      case "openUrl":
        break;   // the real host opens only the repo URL or one of its release pages, in the browser
      case "demoRelease":
        latestRelease = latestRelease === APP_VERSION ? "0.2.0" : APP_VERSION;
        if (s.checkUpdates) runUpdateCheck();         // as if the daily check found it
        break;
      case "demoSilence":
        allSilent = !allSilent;
        break;
      case "demoShaker": {
        const d = shaker();
        d.present = !d.present;
        if (d.present && s.running) beginSwitch(450); else if (!d.present) endSwitch();
        emit(devicesMsg());
        break;
      }
      // pulse: plays while the button is held, stops on release (10 s safety limit)
      case "pulseStart": {
        const now = performance.now();
        if (pulseOn || sweepActive(now / 1000)) break;
        if (!s.running) {
          const fail = testBlocked();     // engine stopped: open only the shaker output
          if (fail) { testMsg = fail.msg; testPanel = fail.panel; testMsgT = now; setTimeout(() => emit(stateMsg()), 4050); break; }
          testUntil = now + 10000;
        }
        testMsg = null;
        pulseOn = true;
        clearTimeout(pulseHoldTimer);
        pulseHoldTimer = setTimeout(() => {
          pulseOn = false;
          if (!s.running) testUntil = 0;
          emit(stateMsg());
        }, 10000);
        break;
      }
      case "pulseStop":
        if (!pulseOn) break;
        pulseOn = false;
        clearTimeout(pulseHoldTimer);
        if (!s.running) testUntil = 0;    // release the device again
        break;
      case "resetSettings":
        Object.assign(s, DEFAULTS);
        applyPreset(s.presets[0]);
        emit(devicesMsg());
        if (feedback()) s.running = false;
        if (s.running) beginSwitch(450); else endSwitch();
        break;
      case "presetSelect": {
        const p = preset(m.id);
        if (p) applyPreset(p);
        break;
      }
      case "presetRevert":
        applyPreset(preset(s.activePresetId));
        break;
      case "presetSave": {
        const p = preset(s.activePresetId);
        if (p && !p.builtIn) { p.cutoff = s.cutoff; p.slope = s.slope; p.gain = s.gain; }
        break;
      }
      case "presetCreate": {
        const p = { id: "p" + Date.now().toString(36), name: m.name, cutoff: s.cutoff, slope: s.slope, gain: s.gain };
        s.presets.push(p);
        s.activePresetId = p.id;
        break;
      }
      case "presetDuplicate": {
        const src = preset(m.id);
        if (!src) break;
        let name = `${src.name} copy`, n = 2;
        while (s.presets.some((p) => p.name.toLowerCase() === name.toLowerCase())) name = `${src.name} copy ${n++}`;
        s.presets.push({ id: "p" + Date.now().toString(36), name: name.slice(0, 24), cutoff: src.cutoff, slope: src.slope, gain: src.gain });
        break;
      }
      case "presetRename": {
        const p = preset(m.id);
        if (p && !p.builtIn) p.name = m.name;
        break;
      }
      case "presetDelete": {
        const p = preset(m.id);
        if (!p || p.builtIn) break;
        s.presets = s.presets.filter((x) => x !== p);
        if (s.activePresetId === p.id) applyPreset(s.presets[0]);
        break;
      }
    }
    if (m.cmd.startsWith("preset") || m.cmd === "resetSettings" || ["cutoff", "slope", "gain"].includes(m.key)) persist();
    emit(stateMsg());
  }

  // --- simulated programme material: each source has a frequency and an envelope ---
  function sources(t) {
    const beat = t % 0.5, bar = Math.floor(t / 0.5);
    const kick = (bar % 8 === 7 ? 0.3 : 0.95) * Math.exp(-beat * 9);
    const rpm = 0.5 + 0.5 * Math.sin(t * 0.35);
    const kerb = (t % 3.7 < 0.35) ? 0.8 * (0.5 + 0.5 * Math.sin(t * 2 * Math.PI * 14)) : 0;
    const pulse = pulseOn ? 0.9 : 0;
    const p = s.sourceMode === "process" ? proc() : null;
    const none = { kick: 0, engine: 0, kerb: 0, mids: 0, highs: 0 };
    const w = (allSilent || testOnly) ? none
            : s.sourceMode === "system" ? { kick: 1, engine: 1, kerb: 1, mids: 1, highs: 1 }
            : p ? p.w : none;
    return [
      { f: 55, a: kick * w.kick },                                          // kick drum
      { f: 28 + 30 * rpm, a: (0.32 + 0.12 * Math.sin(t * 0.9)) * w.engine }, // engine rumble
      { f: 38, a: kerb * w.kerb },                                          // kerb strikes
      { f: 40, a: pulse, gen: true },                                       // test pulse (generated, not captured)
      { f: sweepHzAt(t), gen: true,                                          // sweep test (generated)
        a: sweepActive(t) ? 0.75 * Math.min(1, sweepPos(t) / 0.2, (SWEEP_DUR - sweepPos(t)) / 0.2) : 0 },
      { f: 260, a: (0.42 + 0.1 * Math.sin(t * 3.1)) * w.mids },             // voices / mids
      { f: 1400, a: (0.38 + 0.12 * Math.random()) * w.highs },              // highs
    ];
  }
  const butter = (f, fc, slope) => 1 / Math.sqrt(1 + Math.pow(f / fc, 2 * (slope / 6)));
  // subsonic high-pass (Butterworth, 12 or 24 dB/oct); applies to everything sent to the shaker
  const hpf = (f) => s.subsonic ? 1 / Math.sqrt(1 + Math.pow(s.subsonic / f, s.subsonicSlope / 3)) : 1;
  // automatic reserve: starts at 3 ms, grows 1 ms per underrun up to 6 ms, resets on Start
  let cushionMs = 3;
  // 64 log-spaced analysis bands, 10 Hz to 2 kHz (the real host gets these from an FFT)
  const NB = 64;
  const BAND_F = Array.from({ length: NB }, (_, i) => 10 * Math.pow(200, i / (NB - 1)));

  setInterval(() => {
    const now = performance.now();
    testOnly = !s.running && now < testUntil;       // stopped, but the test pulse is playing
    if (!testOnly && (!s.running || phase || waiting())) return;   // no audio while (re)opening, failed, or waiting
    const t = now / 1000;
    const el = (now - startedAt) / 1000;
    const g = Math.pow(10, s.gain / 20);
    let inSq = 0, outSq = 0;
    const srcs = sources(t);
    for (const src of srcs) {
      if (!src.gen) inSq += src.a * src.a;          // the pulse is generated, never captured
      const o = src.a * (src.gen ? 1 : butter(src.f, s.cutoff, s.slope)) * hpf(src.f) * g;
      outSq += o * o;
    }
    const inDb = 20 * Math.log10(Math.sqrt(inSq) * 0.55 + 1e-6);
    let outDb = 20 * Math.log10(Math.sqrt(outSq) * 0.62 + 1e-6);
    if (s.limiter) outDb = Math.min(outDb, -1);

    const cap = capture(), out = shaker();
    const bufMs = (s.buffer / out.rate) * 1000 * (s.driver === "asio" ? 1 : 2);
    const fill = 0.5 + 0.035 * Math.sin(t * 1.3) + (Math.random() - 0.5) * 0.02 + 0.25 * Math.exp(-el / 1.5);
    const latency = 10 + fill * 2 * cushionMs + bufMs + (Math.random() - 0.5) * 0.15;
    const driftTarget = cap.rate === out.rate ? 18.4 : 31.6;
    const drift = driftTarget + (220 - driftTarget) * Math.exp(-el / 2.5) + (Math.random() - 0.5) * 1.6;
    let risk = 0;
    if (s.buffer === 32 && s.driver === "wasapi") risk += 0.004;
    risk *= Math.pow(3 / cushionMs, 3);            // a bigger reserve makes crackles much rarer
    if (Math.random() < risk) {
      underruns++;
      cushionMs = Math.min(6, cushionMs + 1);       // grow the reserve automatically
    }

    // spectrum: each source is a bump in log-frequency over a faint noise floor
    const spIn = new Array(NB), spOut = new Array(NB);
    for (let b = 0; b < NB; b++) {
      const f = BAND_F[b];
      let p = 2e-6 * (60 / f + 0.3), pg = 0;
      for (const src of srcs) {
        if (!src.a) continue;
        const d = Math.log2(f / src.f);
        const bw = src.f > 1000 ? 0.45 : src.f > 200 ? 0.12 : 0.04;
        const e = src.a * src.a * Math.exp(-d * d / bw);
        if (src.gen) pg += e; else p += e;
      }
      p *= 1 + (Math.random() - 0.5) * 0.6;
      const di = 10 * Math.log10(p) - 4;
      const bf = butter(f, s.cutoff, s.slope);
      const hp = hpf(f);
      let dout = 10 * Math.log10((p * bf * bf + pg) * hp * hp * g * g + 1e-12) - 4;
      if (s.limiter) dout = Math.min(dout, -1);
      spIn[b] = Math.round(di * 10) / 10;
      spOut[b] = Math.round(dout * 10) / 10;
    }

    emit({
      type: "telemetry",
      test: testOnly,   // output-only test signal: levels are real, engine health is not
      sweepHz: sweepActive(t) ? sweepHzAt(t) : 0,
      spectrum: { in: spIn, out: spOut },
      inDb, outDb,
      latencyMs: latency,
      driftPpm: drift,
      locked: el > 6,
      fill: Math.min(1, fill),
      dropouts: underruns,   // the mock only simulates underruns; the engine also counts overruns + capture discontinuities
    });
  }, 33);

  setTimeout(() => { emit(devicesMsg()); emit(stateMsg()); }, 0);
  // check for updates shortly after launch (the real host waits ~10 s, then repeats daily)
  setTimeout(() => { if (s.checkUpdates) runUpdateCheck(); }, 2500);
  return { send, on: (fn) => listeners.push(fn) };
}

})();

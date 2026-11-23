/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 deuter0n
   Generated from the approved mockup by tools/split_mockup.pl. */
"use strict";
/* =====================================================================
   Bridge: in the real app this is WebView2 (window.chrome.webview).
   In a browser it falls back to a simulated host so the UI can be designed
   and tested without the C++ engine.
   ===================================================================== */
if (window.chrome && window.chrome.webview) document.documentElement.classList.add("in-app");

const bridge = (() => {
  const wv = window.chrome && window.chrome.webview;
  if (wv) {
    return {
      send: (msg) => wv.postMessage(msg),
      on: (fn) => wv.addEventListener("message", (e) => fn(e.data)),
    };
  }
  return window.createMockHost();  // mock.js, browser preview only
})();


/* =====================================================================
   UI
   ===================================================================== */
const $ = (id) => document.getElementById(id);
const css = getComputedStyle(document.documentElement);
const C = {
  accent: css.getPropertyValue("--accent").trim(),
  cool: css.getPropertyValue("--cool").trim(),
  line: css.getPropertyValue("--line").trim(),
  faint: css.getPropertyValue("--faint").trim(),
  muted: css.getPropertyValue("--muted").trim(),
  text: css.getPropertyValue("--text").trim(),
};
// follows Windows "Animation effects", also when it changes while the app runs
const motionQuery = window.matchMedia("(prefers-reduced-motion: reduce)");
let reduceMotion = motionQuery.matches;
motionQuery.addEventListener("change", () => { reduceMotion = motionQuery.matches; redraw = true; });

let state = null;
const live = { inDb: -90, outDb: -90, inPeak: -90, outPeak: -90, inHoldT: 0, outHoldT: 0, clipT: 0 };
let lastTele = null;

const fmtRate = (r) => (r / 1000).toFixed(r % 1000 ? 1 : 0) + " kHz";
const setText = (id, txt) => { const el = $(id); if (el.textContent !== txt) el.textContent = txt; };
const setHTML = (id, html) => { const el = $(id); if (el.innerHTML !== html) el.innerHTML = html; };
const esc = (s) => String(s).replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));

/* ---------- views ---------- */
function showView(v) {
  $("viewDash").hidden = v !== "dash";
  $("viewSettings").hidden = v !== "settings";
  $("settingsBtn").setAttribute("aria-pressed", v === "settings");
  closeMenu();
  if (v === "dash") requestAnimationFrame(drawCurve);
}

/* ---------- knobs ---------- */
function makeKnob(el, cfg) {
  el.innerHTML = `
    <svg viewBox="0 0 100 100" aria-hidden="true">
      <path class="trk" fill="none" stroke="${C.line}" stroke-width="6" stroke-linecap="round"/>
      <path class="val" fill="none" stroke="${C.accent}" stroke-width="6" stroke-linecap="round" style="filter:drop-shadow(0 0 6px rgba(255,154,60,.5))"/>
      <circle cx="50" cy="50" r="27" fill="#1e2430" stroke="#2a3141"/>
      <line class="ptr" x1="50" y1="50" x2="50" y2="29" stroke="${C.text}" stroke-width="3" stroke-linecap="round"/>
    </svg>
    <div class="k-val"></div><div class="k-lbl">${cfg.label}</div>`;
  const trk = el.querySelector(".trk"), val = el.querySelector(".val"), ptr = el.querySelector(".ptr"), out = el.querySelector(".k-val");
  const polar = (a, r) => [50 + r * Math.sin(a * Math.PI / 180), 50 - r * Math.cos(a * Math.PI / 180)];
  const arc = (a0, a1, r) => {
    const [x0, y0] = polar(a0, r), [x1, y1] = polar(a1, r);
    return `M${x0.toFixed(2)} ${y0.toFixed(2)} A${r} ${r} 0 ${a1 - a0 > 180 ? 1 : 0} 1 ${x1.toFixed(2)} ${y1.toFixed(2)}`;
  };
  trk.setAttribute("d", arc(-135, 135, 40));
  el.setAttribute("aria-valuemin", cfg.min);
  el.setAttribute("aria-valuemax", cfg.max);

  let norm = 0;
  const knob = {
    set(v) {
      norm = Math.min(1, Math.max(0, cfg.toNorm(v)));
      const a = -135 + 270 * norm;
      const from = cfg.bipolar ? 0 : -135;
      const lo = Math.min(from, a), hi = Math.max(from, a);
      val.setAttribute("d", hi - lo < 0.5 ? "" : arc(lo, hi, 40));
      ptr.setAttribute("transform", `rotate(${a} 50 50)`);
      out.innerHTML = cfg.format(v);
      el.setAttribute("aria-valuenow", v);
      el.setAttribute("aria-valuetext", cfg.format(v).replace(/<[^>]+>/g, " "));
    },
  };
  const commit = (n) => {
    n = Math.min(1, Math.max(0, n));
    const v = cfg.fromNorm(n);
    knob.set(v);
    cfg.onChange(v);
  };
  let drag = null;
  el.addEventListener("pointerdown", (e) => { el.setPointerCapture(e.pointerId); drag = { y: e.clientY, n: norm }; el.focus(); });
  el.addEventListener("pointermove", (e) => {
    if (!drag) return;
    const k = e.shiftKey ? 800 : 200;
    commit(drag.n + (drag.y - e.clientY) / k);
  });
  el.addEventListener("pointerup", () => { drag = null; });
  el.addEventListener("pointercancel", () => { drag = null; });
  el.addEventListener("wheel", (e) => { e.preventDefault(); commit(norm - Math.sign(e.deltaY) * 0.02); }, { passive: false });
  el.addEventListener("dblclick", () => commit(cfg.toNorm(cfg.def)));
  el.addEventListener("keydown", (e) => {
    const step = e.shiftKey ? 0.005 : 0.02;
    if (e.key === "ArrowUp" || e.key === "ArrowRight") { commit(norm + step); e.preventDefault(); }
    if (e.key === "ArrowDown" || e.key === "ArrowLeft") { commit(norm - step); e.preventDefault(); }
  });
  return knob;
}

const knobCutoff = makeKnob($("kCutoff"), {
  label: "Cutoff", min: 20, max: 200, def: 80,
  toNorm: (v) => Math.log(v / 20) / Math.log(10),
  fromNorm: (n) => Math.round(20 * Math.pow(10, n)),
  format: (v) => `${v}<small>Hz</small>`,
  onChange: (v) => setParam("cutoff", v),
});
const knobGain = makeKnob($("kGain"), {
  label: "Gain", min: -12, max: 12, def: 0, bipolar: true,
  toNorm: (v) => (v + 12) / 24,
  fromNorm: (n) => Math.round((n * 24 - 12) * 2) / 2,
  format: (v) => `${v > 0 ? "+" : ""}${v.toFixed(1)}<small>dB</small>`,
  onChange: (v) => setParam("gain", v),
});

function setParam(key, value) {
  if (state) state[key] = value;   // optimistic, so the curve follows the drag
  drawCurve();
  renderPicker();
  bridge.send({ cmd: "set", key, value });
}

/* ---------- segmented controls ---------- */
function wireSeg(id, onPick) {
  const seg = $(id);
  seg.addEventListener("click", (e) => {
    const b = e.target.closest("button");
    if (b) onPick(b.dataset.v);
  });
  seg.addEventListener("keydown", (e) => {
    if (!["ArrowLeft", "ArrowRight"].includes(e.key)) return;
    const btns = [...seg.querySelectorAll("button")];
    const i = btns.findIndex((b) => b.getAttribute("aria-checked") === "true");
    const n = btns[(i + (e.key === "ArrowRight" ? 1 : btns.length - 1)) % btns.length];
    n.focus(); n.click(); e.preventDefault();
  });
}
function paintSeg(id, value) {
  const seg = $(id);
  const btns = [...seg.querySelectorAll("button")];
  btns.forEach((b, i) => {
    const on = String(value) === b.dataset.v;
    b.setAttribute("aria-checked", on);
    b.tabIndex = on ? 0 : -1;
    if (on) seg.style.setProperty("--i", i);
  });
  seg.style.setProperty("--n", btns.length);
}
$("settingsBtn").addEventListener("click", () => showView($("viewSettings").hidden ? "settings" : "dash"));
$("backBtn").addEventListener("click", () => { showView("dash"); $("settingsBtn").focus(); });
document.addEventListener("keydown", (e) => {
  if (e.key !== "Escape" || e.defaultPrevented || menuOpen || $("viewSettings").hidden) return;
  if (document.activeElement && document.activeElement.tagName === "INPUT" && document.activeElement.type === "text") return;
  showView("dash"); $("settingsBtn").focus();
});
wireSeg("driverSeg", (v) => setParam("driver", v));
wireSeg("slopeSeg", (v) => setParam("slope", Number(v)));

/* ---------- inputs ---------- */
wireSeg("sourceSeg", (v) => setParam("sourceMode", v));
$("captureSel").addEventListener("change", (e) => bridge.send({ cmd: "set", key: "captureDevice", value: e.target.value }));
// links: the host opens only the repo URL and its release pages in the browser
const REPO_URL = "https://github.com/deuter0n/GravitonFX";
function openLink(btn, url) {
  bridge.send({ cmd: "openUrl", url });
  if (window.chrome && window.chrome.webview) return;
  const old = btn.textContent;
  btn.textContent = "Opens in your browser";
  setTimeout(() => { btn.textContent = old; }, 1800);
}
// the GitHub link: the app opens it in the default browser; the preview just follows it
$("repoLink").addEventListener("click", (e) => {
  if (!(window.chrome && window.chrome.webview)) return;
  e.preventDefault();
  bridge.send({ cmd: "openUrl", url: REPO_URL });
});
$("updView").addEventListener("click", () => openLink($("updView"), `${REPO_URL}/releases/latest`));
$("updCheck").addEventListener("click", () => bridge.send({ cmd: "checkUpdates" }));$("procSel").addEventListener("change", (e) => bridge.send({ cmd: "set", key: "processExe", value: e.target.value }));
$("procRefresh").addEventListener("click", () => {
  const b = $("procRefresh");
  b.classList.remove("spin"); void b.offsetWidth; b.classList.add("spin");
  setTimeout(() => b.classList.remove("spin"), 450);
  bridge.send({ cmd: "refreshProcesses" });
});
$("shakerSel").addEventListener("change", (e) => bridge.send({ cmd: "set", key: "shakerId", value: e.target.value }));
// Buffer slider: 8 and 16 samples only with ASIO (WASAPI devices practically
// never accept them). Dragging previews the size; it applies on release, since
// each change reopens the shaker device.
const bufferSizes = (driver) => (driver === "asio" ? [8, 16, 32, 64, 128, 256, 512] : [32, 64, 128, 256, 512]);
let bufferRate = 48000, bufferDragging = false;
$("bufferRange").addEventListener("pointerdown", () => { bufferDragging = true; });
for (const ev of ["pointerup", "pointercancel", "blur"]) $("bufferRange").addEventListener(ev, () => { bufferDragging = false; });
function showBuffer(size) {
  setText("bufferVal", `+${((size / bufferRate) * 1000).toFixed(2)} ms`);  // the delay it adds; the stops show the sample count
  document.querySelectorAll("#bufferTicks span").forEach((t) => t.classList.toggle("on", Number(t.textContent) === size));
}
$("bufferRange").addEventListener("input", (e) => showBuffer(bufferSizes(state.driver)[Number(e.target.value)]));
$("bufferRange").addEventListener("change", (e) =>
  bridge.send({ cmd: "set", key: "buffer", value: bufferSizes(state.driver)[Number(e.target.value)] }));
document.querySelectorAll("input[data-key]").forEach((el) =>
  el.addEventListener("change", () => bridge.send({ cmd: "set", key: el.dataset.key, value: el.checked })));
$("powerBtn").addEventListener("click", () => bridge.send({ cmd: state && state.running ? "stop" : "start" }));
// pulse: press and hold to play, release to stop (mouse, touch, Space or Enter)
(function wirePulseHold() {
  const tb = $("testBtn");
  let holding = false;
  const down = (e) => {
    if (tb.disabled || holding) return;
    holding = true;
    if (e && e.pointerId != null) tb.setPointerCapture(e.pointerId);
    bridge.send({ cmd: "pulseStart" });
  };
  const up = () => {
    if (!holding) return;
    holding = false;
    bridge.send({ cmd: "pulseStop" });
  };
  tb.addEventListener("pointerdown", down);
  tb.addEventListener("pointerup", up);
  tb.addEventListener("pointercancel", up);
  tb.addEventListener("lostpointercapture", up);
  tb.addEventListener("keydown", (e) => {
    if ((e.key === " " || e.key === "Enter") && !e.repeat) { e.preventDefault(); down(); }
  });
  tb.addEventListener("keyup", (e) => {
    if (e.key === " " || e.key === "Enter") { e.preventDefault(); up(); }
  });
  tb.addEventListener("blur", up);
})();
$("sweepBtn").addEventListener("click", () => bridge.send({ cmd: "sweep" }));
wireSeg("subsonicSeg", (v) => setParam("subsonic", Number(v)));
wireSeg("subsonicSlopeSeg", (v) => setParam("subsonicSlope", Number(v)));

let resetT = 0;
$("resetBtn").addEventListener("click", () => {
  const b = $("resetBtn");
  if (!b.classList.contains("armed")) {
    b.classList.add("armed"); b.textContent = "Confirm reset";
    clearTimeout(resetT);
    resetT = setTimeout(() => { b.classList.remove("armed"); b.textContent = "Reset"; }, 3000);
    return;
  }
  clearTimeout(resetT);
  b.classList.remove("armed"); b.textContent = "Reset";
  bridge.send({ cmd: "resetSettings" });
});


/* ---------- window buttons ---------- */
// In the app the host minimizes to the tray or closes (per "Close button hides
// to tray"); in the browser preview, mock.js shows a tray demo.
const inApp = !!(window.chrome && window.chrome.webview);
$("minBtn").addEventListener("click", () => { closeMenu(); inApp ? bridge.send({ cmd: "windowMinimize" }) : window.mockToTray(); });
$("closeBtn").addEventListener("click", () => { closeMenu(); inApp ? bridge.send({ cmd: "windowClose" }) : window.mockToTray(); });

/* ---------- presets: shared helpers ---------- */
const LOCK = `<svg class="lock" viewBox="0 0 12 12" aria-label="Built-in"><rect x="2.5" y="5.5" width="7" height="5" rx="1" fill="currentColor"/><path d="M4 5.5V4a2 2 0 0 1 4 0v1.5" fill="none" stroke="currentColor" stroke-width="1.3"/></svg>`;
const activePreset = () => state && state.presets.find((p) => p.id === state.activePresetId);
const isDirty = () => {
  const p = activePreset();
  return !!p && (p.cutoff !== state.cutoff || p.slope !== state.slope || p.gain !== state.gain);
};
const fmtPreset = (p) => `${p.cutoff} Hz · ${p.slope} dB/oct · ${p.gain > 0 ? "+" : ""}${p.gain.toFixed(1)} dB`;
const nameClash = (name, exceptId) => state.presets.some((p) => p.name.toLowerCase() === name.toLowerCase() && p.id !== exceptId);

/* ---------- preset picker (dashboard) ---------- */
let menuOpen = false, menuNew = false, menuErr = "", menuKey = "", pendingName = "";

function renderPicker() {
  if (!state) return;
  const act = activePreset(), dirty = isDirty();
  setText("ppName", act ? act.name : "—");
  $("ppBtn").classList.toggle("is-dirty", dirty);
  $("ppBtn").setAttribute("aria-expanded", menuOpen);
  $("ppBtn").title = dirty ? "Changed since last save" : "";
  $("pRevert").hidden = !dirty;
  $("pSave").hidden = !dirty;
  // icon buttons: the name is the tooltip; Default can only be saved as a new preset ("+")
  const saveName = act && act.builtIn ? "Save as new" : "Save";
  $("pSave").title = saveName;
  $("pSave").setAttribute("aria-label", saveName);
  $("pSave").classList.toggle("is-new", !!(act && act.builtIn));

  const menu = $("ppMenu");
  menu.hidden = !menuOpen;
  if (!menuOpen) return;
  const key = JSON.stringify([state.presets, state.activePresetId, menuNew, menuErr]);
  if (key === menuKey) return;
  menuKey = key;
  const CHECK = `<svg class="ck" viewBox="0 0 12 12" aria-hidden="true"><path d="M2 6.5l2.5 2.5L10 3.5" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/></svg>`;
  menu.innerHTML =
    state.presets.map((p) => `
      <button class="pp-item" role="menuitemradio" aria-checked="${p.id === state.activePresetId}" data-id="${esc(p.id)}">
        ${CHECK}<span class="nm">${p.builtIn ? LOCK : ""}<span>${esc(p.name)}</span></span><span class="v">${p.cutoff} Hz</span>
      </button>`).join("") +
    `<div class="pp-sep" role="separator"></div>` +
    (menuNew
      ? `<div class="pp-new"><input class="name-input" id="ppNewName" maxlength="24" placeholder="Name, e.g. Sim racing" aria-label="New preset name">
           <span class="pp-tip${menuErr ? " err" : ""}">${menuErr ? esc(menuErr) : "Enter to save · Esc to cancel"}</span></div>`
      : `<button class="pp-item act" role="menuitem" data-act="new"><svg viewBox="0 0 12 12" aria-hidden="true"><path d="M6 2v8M2 6h8" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"/></svg><span>Save current as new preset</span></button>`) +
    `<button class="pp-item act" role="menuitem" data-act="manage"><svg viewBox="0 0 12 12" aria-hidden="true"><path d="M2 3h8M2 6h8M2 9h5" stroke="currentColor" stroke-width="1.5" stroke-linecap="round"/></svg><span>Manage presets…</span></button>`;
  if (menuNew) { const i = $("ppNewName"); i.value = pendingName; i.focus(); }
}
function openMenu(withNew) {
  menuOpen = true; menuNew = !!withNew; menuErr = ""; menuKey = ""; pendingName = "";
  renderPicker();
  if (!withNew) { const f = $("ppMenu").querySelector('[aria-checked="true"]'); if (f) f.focus(); }
}
function closeMenu() {
  if (!menuOpen) return;
  menuOpen = false; menuNew = false; menuErr = ""; menuKey = "";
  renderPicker();
}
function commitNew() {
  const name = pendingName.trim();
  if (!name) { menuNew = false; menuErr = ""; renderPicker(); return; }
  if (nameClash(name)) { menuErr = `"${name}" already exists. Pick another name.`; renderPicker(); return; }
  closeMenu();
  bridge.send({ cmd: "presetCreate", name });
  $("ppBtn").focus();
}

$("ppBtn").addEventListener("click", () => (menuOpen ? closeMenu() : openMenu(false)));
$("pRevert").addEventListener("click", () => bridge.send({ cmd: "presetRevert" }));
$("pSave").addEventListener("click", () => {
  if (activePreset().builtIn) openMenu(true);
  else bridge.send({ cmd: "presetSave" });
});
$("ppMenu").addEventListener("click", (e) => {
  const b = e.target.closest("button");
  if (!b) return;
  if (b.dataset.id) {
    if (b.dataset.id !== state.activePresetId) bridge.send({ cmd: "presetSelect", id: b.dataset.id });
    closeMenu(); $("ppBtn").focus();
  } else if (b.dataset.act === "new") {
    menuNew = true; pendingName = ""; renderPicker();
  } else if (b.dataset.act === "manage") {
    closeMenu(); showView("settings");
  }
});
$("ppMenu").addEventListener("input", (e) => { if (e.target.id === "ppNewName") pendingName = e.target.value; });
$("ppMenu").addEventListener("keydown", (e) => {
  if (e.target.id === "ppNewName") {
    if (e.key === "Enter") { e.preventDefault(); commitNew(); }
    if (e.key === "Escape") { e.preventDefault(); e.stopPropagation(); menuNew = false; menuErr = ""; renderPicker(); }
    return;
  }
  if (e.key === "ArrowDown" || e.key === "ArrowUp") {
    const items = [...$("ppMenu").querySelectorAll(".pp-item")];
    const i = items.indexOf(document.activeElement);
    const n = items[(i + (e.key === "ArrowDown" ? 1 : items.length - 1)) % items.length];
    if (n) n.focus();
    e.preventDefault();
  }
});
document.addEventListener("pointerdown", (e) => { if (menuOpen && !$("presetPicker").contains(e.target)) closeMenu(); });
document.addEventListener("keydown", (e) => { if (e.key === "Escape" && menuOpen) { closeMenu(); $("ppBtn").focus(); } });

/* ---------- preset list (settings) ---------- */
let renameId = null, renameVal = "", listErr = "", armedId = null, armT = 0, listKey = "";

function renderPresetList() {
  if (!state) return;
  setText("presetCount", `${state.presets.length} saved`);
  const key = JSON.stringify([state.presets, state.activePresetId, renameId, armedId, listErr]);
  if (key === listKey) return;
  listKey = key;
  $("plist").innerHTML = state.presets.map((p) => {
    const active = p.id === state.activePresetId;
    const name = renameId === p.id
      ? `<input class="name-input" id="renameInput" maxlength="24" aria-label="Preset name">`
      : `${p.builtIn ? LOCK : ""}<span class="nm">${esc(p.name)}</span>${active ? '<span class="chip ok">ACTIVE</span>' : ""}`;
    const acts = [];
    // icon buttons; the name is the tooltip. Delete asks for a second click.
    const btn = (act, tip, svg, cls = "") =>
      `<button class="btn-sm icon${cls}" data-act="${act}" title="${tip}" aria-label="${tip}"><svg viewBox="0 0 16 16" aria-hidden="true">${svg}</svg></button>`;
    const S = `fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"`;
    if (!active) acts.push(btn("use", "Use", `<path d="M3 8.5l3.2 3.2L13 4.8" ${S}/>`));
    if (!p.builtIn) acts.push(btn("rename", "Rename", `<path d="M10.5 2.8l2.7 2.7L6 12.7l-3.4.7.7-3.4z" ${S}/>`));
    acts.push(btn("dup", "Duplicate", `<rect x="5.5" y="5.5" width="8" height="8" rx="1.5" ${S}/><path d="M10.5 3.5v-.5a1 1 0 0 0-1-1h-6a1 1 0 0 0-1 1v6a1 1 0 0 0 1 1h.5" ${S}/>`));
    if (!p.builtIn) {
      const armed = armedId === p.id;
      acts.push(btn("del", armed ? "Click again to delete" : "Delete",
        `<path d="M3 4.5h10M6.5 4.5V3h3v1.5M4.5 4.5l.7 8.5h5.6l.7-8.5" ${S}/>`, armed ? " danger armed" : " danger"));
    }
    return `<div class="prow" data-id="${esc(p.id)}"><div class="pname">${name}</div><div class="pvals">${fmtPreset(p)}</div><div class="pacts">${acts.join("")}</div></div>`;
  }).join("");
  const hint = $("plistHint");
  hint.textContent = listErr || (renameId
    ? "Enter to save · Esc to cancel"
    : "Default is built in and can't be changed. Create presets from the Filter panel on the dashboard, or duplicate one here.");
  hint.classList.toggle("err", !!listErr);
  const ri = $("renameInput");
  if (ri) { ri.value = renameVal; ri.focus(); }
}
function commitRename() {
  if (!renameId) return;
  const id = renameId, name = renameVal.trim();
  const p = state.presets.find((x) => x.id === id);
  if (!name || !p || name === p.name) { renameId = null; renderPresetList(); return; }
  if (nameClash(name, id)) {
    listErr = `"${name}" already exists. Pick another name.`;
    renderPresetList();
    setTimeout(() => { listErr = ""; renderPresetList(); }, 2500);
    return;
  }
  renameId = null; listErr = "";
  bridge.send({ cmd: "presetRename", id, name });
}
$("plist").addEventListener("click", (e) => {
  const b = e.target.closest("button[data-act]");
  if (!b) return;
  const id = b.closest(".prow").dataset.id;
  const p = state.presets.find((x) => x.id === id);
  switch (b.dataset.act) {
    case "use": bridge.send({ cmd: "presetSelect", id }); break;
    case "rename": renameId = id; renameVal = p.name; listErr = ""; renderPresetList(); break;
    case "dup": bridge.send({ cmd: "presetDuplicate", id }); break;
    case "del":
      clearTimeout(armT);
      if (armedId === id) { armedId = null; bridge.send({ cmd: "presetDelete", id }); }
      else {
        armedId = id; renderPresetList();
        armT = setTimeout(() => { armedId = null; renderPresetList(); }, 3000);
      }
      break;
  }
});
$("plist").addEventListener("input", (e) => { if (e.target.id === "renameInput") renameVal = e.target.value; });
$("plist").addEventListener("keydown", (e) => {
  if (e.target.id !== "renameInput") return;
  if (e.key === "Enter") { e.preventDefault(); commitRename(); }
  if (e.key === "Escape") { e.preventDefault(); renameId = null; listErr = ""; renderPresetList(); }
});
$("plist").addEventListener("focusout", (e) => {
  if (e.target.id === "renameInput" && renameId)
    setTimeout(() => { if (renameId && document.activeElement !== $("renameInput")) commitRename(); }, 0);
});

/* ---------- messages from host ---------- */
bridge.on((m) => {
  if (m.type === "devices") {
    knownShaker = m.shaker;
    knownProcesses = m.processes;
    knownEndpoints = m.renderEndpoints;
    defaultEndpointId = m.defaultId;
    if (state) applyState(state);
  } else if (m.type === "state") {
    applyState(m);
  } else if (m.type === "telemetry") {
    // Engine health only while running: the last update after a stop, error
    // or wait arrives after the state change and would paint over its reset.
    const running = state && state.running && !state.phase;
    if (m.test || running) lastTele = m;
    if (!m.test && running) applyTelemetry(m);
  }
});

let knownProcesses = [], knownEndpoints = [], knownShaker = [], defaultEndpointId = null;

function renderSource(s) {
  paintSeg("sourceSeg", s.sourceMode);
  const isProc = s.sourceMode === "process";
  $("srcSystem").hidden = isProc;
  $("srcProcess").hidden = !isProc;

  // System mix device list: "Windows default" first, then every output.
  // Picking the shaker's own device is allowed; the feedback warning covers it.
  const def = knownEndpoints.find((e) => e.id === defaultEndpointId);
  // list = connected devices, plus the current selection only if it's missing
  // "Windows default" alone; the tooltip says which device that is right now
  const defTip = def ? `Current default: ${def.name}` : "No default output device";
  const capOpts = [`<option value="default" title="${esc(defTip)}">Windows default</option>`]
    .concat(knownEndpoints.map((e) => `<option value="${esc(e.id)}">${esc(e.name)}</option>`));
  const capMissing = s.captureDevice !== "default" && !s.captureSel.present;
  if (capMissing) capOpts.splice(1, 0, `<option value="${esc(s.captureDevice)}">${esc(s.captureSel.name)}</option>`);
  setHTML("captureSel", capOpts.join(""));
  $("captureSel").value = s.captureDevice;
  $("captureSel").title = s.captureDevice === "default" ? defTip : "";

  // app list: running apps, plus the saved one if it isn't running right now
  const opts = knownProcesses.map((p) =>
    `<option value="${esc(p.exe)}">${esc(p.name)} · ${esc(p.exe)}${p.playing ? "" : " (silent)"}</option>`);
  // the selected app stays listed while it isn't running; the status line explains it
  if (!s.processExe)
    opts.unshift(`<option value="" disabled>Pick an app</option>`);
  else if (!s.process.running)
    opts.unshift(`<option value="${esc(s.processExe)}">${esc(s.process.name)} · ${esc(s.processExe)}</option>`);
  setHTML("procSel", opts.join(""));
  $("procSel").value = s.processExe;
}

function applyState(s) {
  state = s;
  renderSource(s);
  // shaker list follows the same rule: connected devices + the selection if it's missing
  const shOpts = knownShaker.map((d) => `<option value="${esc(d.id)}">${esc(d.name)}</option>`);
  const shMissing = !s.shakerDev.present;
  if (shMissing) shOpts.unshift(`<option value="${esc(s.shakerId)}">${esc(s.shakerDev.name)}</option>`);
  setHTML("shakerSel", shOpts.join(""));
  $("shakerSel").value = s.shakerId;


  // mock host only: simulate button labels
  if (s.demo && window.mockRender) window.mockRender(s);

  // updates: dot on the Settings button + status in Settings > About
  const u = s.update;
  setText("aboutVer", `Graviton FX ${u.current} · GPL-3.0-or-later`);
  const avail = u.status === "available";
  $("updDot").hidden = !avail;
  $("settingsBtn").title = avail ? `Settings · update ${u.latest} available` : "Settings";
  $("updView").hidden = !avail;
  $("updCheck").disabled = u.status === "checking";
  setText("updCheck", u.status === "checking" ? "Checking…" : "Check now");
  $("updText").classList.toggle("upd-new", avail);
  setText("updText",
    avail ? `Update available: ${u.latest}. You have ${u.current}.`
    : u.status === "uptodate" ? `You're up to date (${u.current}).`
    : u.status === "checking" ? "Checking GitHub for a new release…"
    : u.status === "error" ? "Couldn't reach GitHub. Try again later."
    : `Graviton FX ${u.current}. Not checked yet.`);
  document.querySelectorAll("input[data-key]").forEach((el) => { el.checked = !!s[el.dataset.key]; });
  paintSeg("driverSeg", s.driver);
  paintSeg("slopeSeg", s.slope);
  paintSeg("subsonicSeg", s.subsonic);
  paintSeg("subsonicSlopeSeg", s.subsonicSlope);
  // slope only matters while the subsonic filter is on
  $("subsonicSlopeSeg").querySelectorAll("button").forEach((b) => { b.disabled = !s.subsonic; });
  $("subsonicSlopeSeg").classList.toggle("off", !s.subsonic);
  knobCutoff.set(s.cutoff);
  knobGain.set(s.gain);

  const cap = s.captureDev, out = s.shakerDev;
  // format as tags: rate, sample format, channels (+ "out 1-2" on devices with more outputs)
  const tagHtml = (items) => items.map(([text, tip]) => `<span class="tag" title="${esc(tip)}">${esc(text)}</span>`).join("");
  const fmtTags = (rate, fmt) => {
    const [bits, layout] = fmt.split(" · ");
    const items = [[fmtRate(rate), "Sample rate"]];
    if (layout) items.push([bits, "Sample format"], [layout, "Channels"]);
    else items.push([bits, "Channels"]);   // format not known yet: only the layout
    return items;
  };
  setHTML("captureFmt", tagHtml(fmtTags(cap.rate, cap.fmt)));
  // One app: the format is just what Graviton asks Windows for, so it isn't shown
  $("captureFmt").hidden = s.sourceMode === "process";
  // devices with more than 2 outputs: the shaker signal always uses outputs 1-2 (an "out 1–2" tag)
  const outTags = fmtTags(out.rate, out.fmt);
  if (out.ch > 2) outTags.push(["out 1–2", `Shaker signal on outputs 1–2 of ${out.ch}`]);
  // format not readable while stopped (another app holds the device): no tags,
  // rather than a guess; the row keeps its height so nothing below moves
  setHTML("shakerFmt", out.known === false ? "" : tagHtml(outTags));
  // buffer slider: stops for this driver, the size and its time at the shaker's rate
  bufferRate = out.rate;
  const sizes = bufferSizes(s.driver);
  const range = $("bufferRange");
  range.max = String(sizes.length - 1);
  // stop labels under the thumb positions (the 16 px thumb is inset by half its width)
  setHTML("bufferTicks", sizes.map((n, i) =>
    `<span style="left:calc(8px + (100% - 16px) * ${i / (sizes.length - 1)})">${n}</span>`).join(""));
  const idx = Math.max(0, sizes.indexOf(s.buffer));
  if (!bufferDragging) range.value = String(idx);  // never yank the thumb mid-drag
  range.setAttribute("aria-valuetext", `${sizes[idx]} samples`);
  if (!bufferDragging) showBuffer(sizes[idx]);
  const k = (r) => String(+(r / 1000).toFixed(1));
  setHTML("hRate", cap.rate === out.rate ? `${k(cap.rate)}<small>native</small>` : `${k(cap.rate)}<small>→</small>${k(out.rate)}`);

  setText("hitKeyText", s.limiter ? "Limiter active" : "Clipping");
  $("hitKeyText").style.color = s.limiter ? "" : "var(--bad)";
  $("hitKey").title = s.limiter
    ? "The limiter held the shaker signal at -1 dBFS. Frequent marks mean the gain is too high."
    : "The shaker signal went over 0 dBFS and was clipped. Lower the gain or turn on the limiter in Settings.";

  const st = !s.running ? (s.feedback ? "blocked" : "stopped")
           : s.phase === "error" ? "error"
           : s.phase === "waiting" ? "waiting"
           : s.phase ? "switching"
           : "running";
  $("status").dataset.state = st;
  setText("statusText", {
    running: "RUNNING", error: "ERROR", blocked: "CHECK OUTPUT", stopped: "STOPPED", waiting: "WAITING",
    switching: s.phase === "starting" ? "STARTING" : "SWITCHING",
  }[st]);
  $("powerBtn").setAttribute("aria-pressed", s.running);
  setText("powerText", s.running ? "Stop" : "Start");
  $("powerBtn").disabled = !s.running && s.feedback;
  // test pulse works while running or stopped (it opens the output on its own), not mid-switch or in error
  const canTest = st === "running" || ((st === "stopped" || st === "blocked") && s.shakerDev.present);
  // both tests behave the same: the active button turns amber and reads "Pulsing · 40 Hz" or "Sweeping · 45 Hz";
  // the other test is disabled meanwhile
  $("testBtn").disabled = s.pulsing ? false : (!canTest || s.sweeping);
  $("testBtn").classList.toggle("on", !!s.pulsing);
  setText("testTxt", s.pulsing ? "Pulsing · 40 Hz" : "Pulse Test · 40 Hz");
  $("sweepBtn").disabled = s.sweeping ? false : (!canTest || s.pulsing);
  $("sweepBtn").classList.toggle("on", !!s.sweeping);
  if (!s.sweeping) setText("sweepTxt", "Sweep Test · 20 – 200 Hz");
  // One message line in the header: errors first, then the feedback warning,
  // then whatever the engine is waiting for. The selector it's about gets a
  // matching outline. Cards never grow, so the layout height never changes.
  const sourceSel = s.sourceMode === "process" ? "procSel" : "captureSel";
  const outPanel = st === "error" ? s.errorPanel : s.testPanel;
  const errAt = outPanel === "source" ? sourceSel : "shakerSel";
  const outMsg = st === "error" ? s.errorMsg : s.testMsg;
  const capMissing = s.sourceMode === "system" && s.captureDevice !== "default" && !s.captureSel.present;
  const appMissing = s.sourceMode === "process" && !s.process.running;
  // `mark` is the control that gets the outline: the buffer slider for a refused buffer size
  const msg = outMsg ? { kind: "err", at: errAt, mark: outPanel === "buffer" ? "bufferRange" : errAt, text: outMsg, retry: st === "error" && s.retrying }
    : s.feedback ? { kind: "err", at: "shakerSel", text: "Also the capture device. Pick another, or capture one app." }
    : shMissing ? { kind: "wait", at: "shakerSel", text: "Not connected. Output starts when it's back." }
    : capMissing ? { kind: "wait", at: "captureSel", text: "Not connected. Capture resumes when it's back." }
    : appMissing ? { kind: "wait", at: "procSel", text: "Not running. Capture starts when it opens." }
    : null;
  // the message sits under its selector, in place of that selector's format tags
  const slots = { captureSel: "captureMsg", procSel: "procMsg", shakerSel: "shakerMsg" };
  for (const [sel, slot] of Object.entries(slots)) {
    const on = !!msg && msg.at === sel;
    $(slot).hidden = !on;
    if (!on) continue;
    $(slot).dataset.kind = msg.kind;
    $(slot).title = msg.text;
    setText(slot + "Text", msg.text);
  }
  $("captureFmt").hidden = s.sourceMode === "process" || (!!msg && msg.at === "captureSel");
  $("shakerFmt").hidden = !!msg && msg.at === "shakerSel";
  for (const id of ["captureSel", "procSel", "shakerSel", "bufferRange"]) {
    const marked = !!msg && (msg.mark || msg.at) === id;
    $(id).classList.toggle("attn-wait", marked && msg.kind === "wait");
    $(id).classList.toggle("attn-err", marked && msg.kind === "err");
  }
  // the tray demo exists only in the browser mockup; the app has a real tray icon
  if ($("trayState")) setText("trayState", { running: "Engine running", switching: "Switching devices", waiting: "Waiting for the audio source", error: "Output error, retrying", blocked: "Engine stopped", stopped: "Engine stopped" }[st]);
  // the header explains engine states; this indicator only reports incoming audio
  if (st !== "running") { setText("sigAside", "Off"); $("sigAside").dataset.s = "idle"; }

  if (st !== "running") {
    lastTele = null;
    setHTML("hLat", "--<small>ms</small>");
    setHTML("hDrift", "--<small>ppm</small>");
    $("fillBar").style.width = "0%";
    setText("fillPct", "--");
    const chip = $("lockChip");
    if (st === "switching") { chip.className = "chip busy"; chip.textContent = "SETTLING"; }
    else if (st === "error") { chip.className = "chip bad"; chip.textContent = "ERROR"; }
    else if (st === "waiting") { chip.className = "chip"; chip.textContent = "WAITING"; }
    else { chip.className = "chip"; chip.textContent = "IDLE"; }
  }
  drawCurve();
  renderPicker();
  renderPresetList();
}

function applyTelemetry(t) {
  setHTML("hLat", `${t.latencyMs.toFixed(1)}<small>ms</small>`);
  setHTML("hDrift", `${t.driftPpm >= 0 ? "+" : ""}${t.driftPpm.toFixed(1)}<small>ppm</small>`);
  setText("hUnder", String(t.dropouts));
  $("hUnder").style.color = t.dropouts ? "var(--bad)" : "";
  $("fillBar").style.width = (t.fill * 100).toFixed(1) + "%";
  setText("fillPct", `${Math.round(t.fill * 100)}%`);
  const chip = $("lockChip");
  chip.className = "chip " + (t.locked ? "ok" : "busy");
  chip.textContent = t.locked ? "LOCKED" : "SETTLING";
}

/* ---------- canvases ---------- */
function fit(c) {
  const r = c.getBoundingClientRect(), d = window.devicePixelRatio || 1;
  const w = Math.max(1, Math.round(r.width * d)), h = Math.max(1, Math.round(r.height * d));
  if (c.width !== w || c.height !== h) { c.width = w; c.height = h; }
  const ctx = c.getContext("2d");
  ctx.setTransform(d, 0, 0, d, 0, 0);
  return { ctx, w: r.width, h: r.height };
}

// filter response curve + live spectrum behind it
const F_MIN = 10, F_MAX = 2000, DB_MIN = -48, DB_MAX = 12;
const SPEC_N = 64;                      // must match the host's band layout
const SPEC_F = Array.from({ length: SPEC_N }, (_, i) => F_MIN * Math.pow(F_MAX / F_MIN, i / (SPEC_N - 1)));
const SPEC_LO = -72, SPEC_HI = 0;       // spectrum has its own (unlabelled) dBFS scale
const specIn = new Float32Array(SPEC_N).fill(-120), specOut = new Float32Array(SPEC_N).fill(-120);
const specPk = new Float32Array(SPEC_N).fill(-120), specPkT = new Float32Array(SPEC_N);
let curveGeo = null, curveDrag = null, curveHover = false;

// smooth a polyline with quadratic curves through the midpoints
function smoothPath(ctx, pts, move = true) {
  move ? ctx.moveTo(pts[0][0], pts[0][1]) : ctx.lineTo(pts[0][0], pts[0][1]);
  for (let i = 1; i < pts.length - 1; i++) {
    ctx.quadraticCurveTo(pts[i][0], pts[i][1], (pts[i][0] + pts[i + 1][0]) / 2, (pts[i][1] + pts[i + 1][1]) / 2);
  }
  const last = pts[pts.length - 1];
  ctx.lineTo(last[0], last[1]);
}
// one look for every data series in both charts: blue = system audio, amber = shaker output
const SERIES = {
  in:  { f0: "rgba(111,140,255,0.20)", f1: "rgba(111,140,255,0.02)", line: "rgba(150,170,255,0.8)", lw: 1.2, glow: null },
  out: { f0: "rgba(255,154,60,0.36)",  f1: "rgba(255,154,60,0.03)",  line: "#ff9a3c",              lw: 1.8, glow: "rgba(255,154,60,0.6)" },
};
function drawSeries(ctx, pts, top, bot, kind) {
  const s = SERIES[kind];
  const g = ctx.createLinearGradient(0, top, 0, bot);
  g.addColorStop(0, s.f0); g.addColorStop(1, s.f1);
  ctx.beginPath(); ctx.moveTo(pts[0][0], bot); smoothPath(ctx, pts, false); ctx.lineTo(pts[pts.length - 1][0], bot); ctx.closePath();
  ctx.fillStyle = g; ctx.fill();
  ctx.beginPath(); smoothPath(ctx, pts);
  ctx.lineJoin = "round"; ctx.strokeStyle = s.line; ctx.lineWidth = s.lw;
  if (s.glow) { ctx.shadowColor = s.glow; ctx.shadowBlur = 8; }
  ctx.stroke(); ctx.shadowBlur = 0;
}
const GRID = { major: "rgba(255,255,255,0.045)", zero: "rgba(255,255,255,0.12)" };

const curveX = (f) => { const { pad, w } = curveGeo; return pad.l + (Math.log(f / F_MIN) / Math.log(F_MAX / F_MIN)) * (w - pad.l - pad.r); };
const curveY = (db) => { const { pad, h } = curveGeo; return pad.t + (DB_MAX - db) / (DB_MAX - DB_MIN) * (h - pad.t - pad.b); };

function drawCurve() {
  if (!state || $("viewDash").hidden) return;
  const { ctx, w, h } = fit($("curve"));
  // top band: SUB / BASS / MIDS; bottom: frequencies, then the legend (as tall as it needs; it may wrap)
  const legend = $("curve").parentElement.querySelector(".graph-legend.at-bottom");
  const pad = { l: 38, r: 14, t: 24, b: 30 + (legend ? legend.offsetHeight : 16) };
  curveGeo = { pad, w, h };
  const X = curveX, Y = curveY;
  const top = pad.t, bot = h - pad.b, left = pad.l, right = w - pad.r;
  ctx.clearRect(0, 0, w, h);

  // frequency regions: SUB / BASS / MIDS labels only; the plot background is
  // one plain color, like the Signal card's graphs
  ctx.save();
  ctx.font = "600 9.5px 'Chakra Petch', 'Segoe UI', sans-serif";
  if ("letterSpacing" in ctx) ctx.letterSpacing = "2px";
  ctx.textAlign = "center";
  [[F_MIN, 60, "SUB"], [60, 250, "BASS"], [250, F_MAX, "MIDS"]].forEach(([a, b, name]) => {
    ctx.fillStyle = "rgba(139,148,168,0.6)";
    ctx.fillText(name, (X(a) + X(b)) / 2, top - 9);
  });
  ctx.restore();

  // grid: minor log lines, major frequency lines, dB lines
  ctx.lineWidth = 1;
  ctx.strokeStyle = "rgba(255,255,255,0.022)";
  ctx.beginPath();
  for (const dec of [10, 100, 1000]) for (let k = 2; k <= 9; k++) {
    const f = dec * k; if (f <= F_MIN || f >= F_MAX) continue;
    ctx.moveTo(X(f), top); ctx.lineTo(X(f), bot);
  }
  ctx.stroke();
  ctx.font = "10px 'JetBrains Mono', Consolas, monospace";
  ctx.fillStyle = C.faint;
  for (const f of [20, 50, 100, 200, 500, 1000, 2000]) {
    ctx.strokeStyle = GRID.major;
    ctx.beginPath(); ctx.moveTo(X(f), top); ctx.lineTo(X(f), bot); ctx.stroke();
    ctx.textAlign = f === 2000 ? "right" : "center";
    ctx.fillText(f === 2000 ? "2k Hz" : f >= 1000 ? f / 1000 + "k" : String(f), f === 2000 ? right : X(f), bot + 15);
  }
  ctx.textAlign = "right";
  for (const db of [12, 0, -12, -24, -36, -48]) {
    ctx.strokeStyle = db === 0 ? GRID.zero : GRID.major;
    ctx.beginPath(); ctx.moveTo(left, Y(db)); ctx.lineTo(right, Y(db)); ctx.stroke();
    ctx.fillText((db > 0 ? "+" : "") + db, left - 7, Y(db) + 3);
  }

  // live spectrum: input behind, shaker output in front, shaker peak hold dotted
  const Ys = (db) => top + (SPEC_HI - Math.max(SPEC_LO, Math.min(SPEC_HI, db))) / (SPEC_HI - SPEC_LO) * (bot - top);
  const specPts = (arr) => Array.from({ length: SPEC_N }, (_, b) => [X(SPEC_F[b]), Ys(arr[b])]);
  if (specIn.some((v) => v > SPEC_LO)) drawSeries(ctx, specPts(specIn), top, bot, "in");
  if (specOut.some((v) => v > SPEC_LO)) drawSeries(ctx, specPts(specOut), top, bot, "out");
  if (specPk.some((v) => v > SPEC_LO)) {
    ctx.beginPath(); smoothPath(ctx, specPts(specPk));
    ctx.setLineDash([1.5, 3]); ctx.strokeStyle = "rgba(255,214,170,0.6)"; ctx.lineWidth = 1; ctx.stroke(); ctx.setLineDash([]);
  }

  // sweep test marker: where the sweep is right now
  if (state.sweeping && lastTele && lastTele.sweepHz) {
    const xs = X(lastTele.sweepHz);
    ctx.strokeStyle = "rgba(255,154,60,0.8)"; ctx.lineWidth = 1.5;
    ctx.beginPath(); ctx.moveTo(xs, top); ctx.lineTo(xs, bot); ctx.stroke();
    ctx.font = "500 10.5px 'JetBrains Mono', Consolas, monospace";
    ctx.fillStyle = C.accent; ctx.textAlign = xs > right - 70 ? "right" : "left";
    ctx.fillText(`sweep ${Math.round(lastTele.sweepHz)} Hz`, xs + (xs > right - 70 ? -6 : 6), top + 12);
  }

  // filter response: a neutral line (amber is reserved for the shaker signal)
  const n = state.slope / 6;
  const pts = [];
  for (let i = 0; i <= 240; i++) {
    const f = F_MIN * Math.pow(F_MAX / F_MIN, i / 240);
    const lp = 1 / Math.sqrt(1 + Math.pow(f / state.cutoff, 2 * n));
    const hp = state.subsonic ? 1 / Math.sqrt(1 + Math.pow(state.subsonic / f, state.subsonicSlope / 3)) : 1;   // subsonic filter
    pts.push([X(f), Y(Math.max(DB_MIN, 20 * Math.log10(lp * hp) + state.gain))]);
  }
  ctx.beginPath(); pts.forEach(([x, y], i) => (i ? ctx.lineTo(x, y) : ctx.moveTo(x, y)));
  ctx.lineJoin = "round";
  ctx.strokeStyle = "rgba(231,234,241,0.85)"; ctx.lineWidth = 1.6;
  ctx.stroke();

  // cutoff handle + label pill
  const xc = X(state.cutoff), yc = Y(-3 + state.gain);
  const active = curveHover || curveDrag;
  ctx.setLineDash([3, 4]);
  ctx.strokeStyle = `rgba(231,234,241,${active ? 0.45 : 0.22})`; ctx.lineWidth = 1;
  ctx.beginPath(); ctx.moveTo(xc, top); ctx.lineTo(xc, bot); ctx.stroke();
  ctx.setLineDash([]);
  const hr = active ? 10 : 8;
  ctx.fillStyle = "rgba(231,234,241,0.12)";
  ctx.beginPath(); ctx.arc(xc, yc, hr, 0, Math.PI * 2); ctx.fill();
  ctx.strokeStyle = C.text; ctx.lineWidth = 1.4; ctx.stroke();
  ctx.fillStyle = C.text;
  ctx.beginPath(); ctx.arc(xc, yc, 3, 0, Math.PI * 2); ctx.fill();

  const label = `${state.cutoff} Hz` + (state.gain ? `  ${state.gain > 0 ? "+" : ""}${state.gain.toFixed(1)} dB` : "");
  ctx.font = "500 11px 'JetBrains Mono', Consolas, monospace";
  const tw = ctx.measureText(label).width, pw = tw + 16, ph = 20;
  let px = xc + 14, py = yc - ph - 8;
  if (px + pw > right) px = xc - 14 - pw;
  if (py < top) py = yc + 12;
  ctx.fillStyle = "rgba(17,21,29,0.92)";
  ctx.strokeStyle = "rgba(231,234,241,0.22)"; ctx.lineWidth = 1;
  ctx.beginPath();
  if (ctx.roundRect) ctx.roundRect(px, py, pw, ph, 6); else ctx.rect(px, py, pw, ph);
  ctx.fill(); ctx.stroke();
  ctx.fillStyle = C.text; ctx.textAlign = "left";
  ctx.fillText(label, px + 8, py + 14);
}

// drag the chart: anywhere = cutoff, the handle = cutoff + gain
(function wireCurveDrag() {
  const el = $("curve");
  const local = (e) => { const r = el.getBoundingClientRect(); return [e.clientX - r.left, e.clientY - r.top]; };
  const nearHandle = (px, py) => curveGeo && state && Math.hypot(px - curveX(state.cutoff), py - curveY(-3 + state.gain)) < 20;
  const apply = (px, py) => {
    const { pad, w, h } = curveGeo;
    const fx = Math.min(1, Math.max(0, (px - pad.l) / (w - pad.l - pad.r)));
    const cutoff = Math.round(Math.min(200, Math.max(20, F_MIN * Math.pow(F_MAX / F_MIN, fx))));
    if (cutoff !== state.cutoff) { knobCutoff.set(cutoff); setParam("cutoff", cutoff); }
    if (curveDrag.both) {
      const db = DB_MAX - ((py - pad.t) / (h - pad.t - pad.b)) * (DB_MAX - DB_MIN);
      const gain = Math.round(Math.min(12, Math.max(-12, db + 3)) * 2) / 2;
      if (gain !== state.gain) { knobGain.set(gain); setParam("gain", gain); }
    }
  };
  el.addEventListener("pointerdown", (e) => {
    if (!state || !curveGeo) return;
    const [px, py] = local(e);
    curveDrag = { both: nearHandle(px, py) };
    el.setPointerCapture(e.pointerId);
    el.style.cursor = curveDrag.both ? "grabbing" : "ew-resize";
    apply(px, py);
  });
  el.addEventListener("pointermove", (e) => {
    const [px, py] = local(e);
    if (curveDrag) { apply(px, py); return; }
    curveHover = !!nearHandle(px, py);
    el.style.cursor = curveHover ? "grab" : "ew-resize";
  });
  const end = () => { curveDrag = null; el.style.cursor = curveHover ? "grab" : "ew-resize"; };
  el.addEventListener("pointerup", end);
  el.addEventListener("pointercancel", end);
  el.addEventListener("pointerleave", () => { if (!curveDrag) curveHover = false; });
})();


const dbToNorm = (db) => Math.min(1, Math.max(0, (db + 48) / 48));
const HIST = 150;
// level history in dBFS (-90 = silence)
const histIn = new Float32Array(HIST).fill(-90), histOut = new Float32Array(HIST).fill(-90);
let histT = 0, lastFrame = performance.now(), silentFor = 0;
let histFrac = 0, histHeadIn = -90, histHeadOut = -90;   // smooth scrolling: progress to the next sample, and its value so far
let loudT = 0;       // when anything last came in or went out
let redraw = true;   // a canvas changed size: draw once even while still

// Redraws at most 60 times a second, in step with the display (a 120 Hz display
// draws every other refresh). When stopped and quiet, the well fades to still and
// nothing redraws; the page then only checks 10 times a second whether something
// started. Both save CPU and battery.
function frame(now) {
  const running = state && (state.running || state.testing);
  const quiet = !running && live.inDb <= -89 && live.outDb <= -89;
  if (quiet && now - loudT > HIST * 1000 / 30 + 500 && wellStill() && !redraw) {
    setTimeout(() => frame(performance.now()), 100);
    return;
  }
  if (now - lastFrame < 1000 / 60 - 2) {
    requestAnimationFrame(frame);
    return;
  }
  redraw = false;
  const dt = Math.min(0.1, (now - lastFrame) / 1000);
  lastFrame = now;
  const tIn = running && lastTele ? lastTele.inDb : -90;
  const tOut = running && lastTele ? lastTele.outDb : -90;

  // ballistics: fast attack, ~24 dB/s release; 1 s peak hold
  live.inDb = tIn > live.inDb ? tIn : Math.max(tIn, live.inDb - 24 * dt);
  live.outDb = tOut > live.outDb ? tOut : Math.max(tOut, live.outDb - 24 * dt);
  if (live.inDb >= live.inPeak) { live.inPeak = live.inDb; live.inHoldT = now; } else if (now - live.inHoldT > 1000) live.inPeak -= 30 * dt;
  if (live.outDb >= live.outPeak) { live.outPeak = live.outDb; live.outHoldT = now; } else if (now - live.outHoldT > 1000) live.outPeak -= 30 * dt;
  if (tOut > -0.5) live.clipT = now;
  if (tIn > -90 || tOut > -90) loudT = now;

  const drive = dbToNorm(live.outDb);
  histT += dt;
  while (histT > 1 / 30) {
    histT -= 1 / 30;
    histIn.copyWithin(0, 1); histOut.copyWithin(0, 1);
    histIn[HIST - 1] = tIn;
    histOut[HIST - 1] = tOut;
  }
  histFrac = histT * 30;
  histHeadIn = tIn; histHeadOut = tOut;

  // spectrum ballistics: quick rise, ~45 dB/s fall
  const sp = running && lastTele && lastTele.spectrum;
  for (let b = 0; b < SPEC_N; b++) {
    const ti = sp ? sp.in[b] : -120, to = sp ? sp.out[b] : -120;
    specIn[b] = ti > specIn[b] ? specIn[b] + (ti - specIn[b]) * 0.6 : Math.max(ti, specIn[b] - 45 * dt);
    specOut[b] = to > specOut[b] ? specOut[b] + (to - specOut[b]) * 0.6 : Math.max(to, specOut[b] - 45 * dt);
    // shaker spectrum peak hold: 0.9 s hold, then ~18 dB/s fall
    if (specOut[b] >= specPk[b]) { specPk[b] = specOut[b]; specPkT[b] = now; }
    else if (now - specPkT[b] > 900) specPk[b] = Math.max(-120, specPk[b] - 18 * dt);
  }

  // "no input": streaming fine, but nothing has come in for 2 s (same rule for both source modes)
  const streaming = state && state.running && !state.phase && lastTele;
  silentFor = streaming && lastTele.inDb < -60 ? silentFor + dt : 0;
  const noInput = streaming && silentFor > 2;
  if (state && state.sweeping) {
    // while active, the button reads "Sweeping · <current step> Hz"
    setText("sweepTxt", `Sweeping · ${lastTele && lastTele.sweepHz ? Math.round(lastTele.sweepHz) : 20} Hz`);
  }
  // input state under "Shaker drive": one word, same form every time
  if (streaming) {
    setText("sigAside", noInput ? "No signal" : "Active");
    $("sigAside").dataset.s = noInput ? "noinput" : "live";
  }

  if (!$("viewDash").hidden && !$("app").classList.contains("to-tray")) {
    drawCurve();
    // live levels at the end of the level history's legend
    const lvl = (db) => (db < -60 ? "-inf" : db.toFixed(0));
    setHTML("envRead", `<b class="in">IN ${lvl(live.inDb)}</b> · <b class="out">OUT ${lvl(live.outDb)}</b> dBFS`);
    setHTML("drivePct", `${Math.round(drive * 100)}<small>%</small>`);
    setText("driveDb", live.outDb < -60 ? "-inf dBFS" : `${live.outDb.toFixed(1)} dBFS`);
    drawWell(now / 1000, drive, quiet);
    drawEnv();
  }
  requestAnimationFrame(frame);
}

// ---------- the gravity well (hero) ----------
// A perspective grid running to a horizon, pulled down by the shaker drive.
const well = { t: 0, depth: 0, v: 0, prev: 0, punch: 0, amb: 1 };   // amb: idle motion, fades out when stopped
const wellStill = () => Math.abs(well.depth) < 1e-3 && Math.abs(well.v) < 1e-3 && well.punch < 1e-3 && well.amb < 1e-3;

// perspective grid (raised camera) with the v1 motion: the dip follows the drive directly
// and a fine, fast ripple runs across it
function drawWell(t, drive, quiet) {
  const { ctx, w, h } = fit($("well"));
  ctx.clearRect(0, 0, w, h);
  const dt = Math.min(0.05, Math.max(0, t - (well.t || t)));
  well.t = t;
  well.amb += ((quiet ? 0 : 1) - well.amb) * Math.min(1, dt * 2);

  // smoothing + transient detection (a fast rise over the slow average = a hit)
  if (reduceMotion || dt === 0) {
    well.depth += (drive - well.depth) * 0.35;
  } else {
    // punch: a sudden rise in drive (a hit) kicks the spring downward and pops the ball
    const rise = drive - well.prev;
    if (rise > 0.03) {
      well.v += rise * 55;
      well.punch = Math.max(well.punch, Math.min(1, rise * 5));
    }
    // stiff, lightly damped spring: snaps down, overshoots, bounces back
    well.v += ((drive - well.depth) * 320 - well.v * 17) * dt;
    well.depth += well.v * dt;
    well.punch *= Math.exp(-dt * 9);
  }
  well.prev = drive;

  // world -> screen
  const z0 = 3.0, zNear = 1.15, zFar = 10, camH = 1.75, SIG2 = 0.85;   // higher camera looks down into the well
  const dep = Math.max(0, well.depth);
  const D = 0.1 + 1.9 * well.depth;                                      // depth tracks the drive (can overshoot)
  const breathe = reduceMotion ? 0 : (Math.sin(t * 1.4) * 0.5 + 0.5) * well.amb;
  const wobble = reduceMotion ? 0 : 0.1 * dep;                           // fine, fast ripple while bass plays
  const swell = reduceMotion ? 0 : 0.03 * well.amb;                      // slow ambient wave, still when stopped
  const horizon = h * 0.1;
  const f = (h * 0.98 - horizon) * zNear / camH;
  const P = (x, y, z) => [w / 2 + x * f / z, horizon + (camH - y) * f / z];
  const surf = (x, z) => {
    const dz = z - z0, r2 = x * x + dz * dz;
    let y = -D * Math.exp(-r2 / SIG2);
    if (wobble) y += wobble * Math.sin(Math.sqrt(r2) * 6.1 - t * 11) * Math.exp(-r2 / 3.2);
    if (swell) y += swell * Math.sin(x * 0.9 + t * 0.7) * Math.cos(dz * 0.8 - t * 0.5);
    return y;
  };

  // mass position (used as the light source)
  // mostly steady size: grows a little with the drive and pops slightly on hits
  const ro = (0.18 + 0.07 * dep) * (1 + 0.12 * well.punch);
  const [mx, my] = P(0, -D * 0.9 + ro, z0);
  const mr = ro * f / z0;

  // 1. grid, lit from the mass: hot amber near it, cool blue at the edges
  const light = ctx.createRadialGradient(mx, my, 0, mx, my, Math.max(w, h) * 0.62);
  light.addColorStop(0, `rgba(255,180,110,${Math.min(1, 0.24 + 0.14 * dep).toFixed(3)})`);
  light.addColorStop(0.25, `rgba(255,154,60,${Math.min(1, 0.12 + 0.08 * dep).toFixed(3)})`);
  light.addColorStop(0.65, "rgba(120,130,190,0.07)");
  light.addColorStop(1, "rgba(111,140,255,0.035)");
  ctx.strokeStyle = light;
  ctx.lineWidth = 0.8;   // fine, soft grid lines
  ctx.lineJoin = "round";
  const STEP = 0.26;   // denser grid
  ctx.beginPath();
  for (let z = zFar; z >= zNear - 1e-6; z -= STEP) {          // rows
    const span = (w / 2) * z / f + STEP;
    const n = 64;
    for (let i = 0; i <= n; i++) {
      const x = -span + (2 * span * i) / n;
      const [sx, sy] = P(x, surf(x, z), z);
      i ? ctx.lineTo(sx, sy) : ctx.moveTo(sx, sy);
    }
  }
  const X = (w / 2) * zFar / f;
  for (let x = -Math.ceil(X / STEP) * STEP; x <= X + 1e-6; x += STEP) {   // columns: continuous lines, the canvas clips them
    const n = 48;
    for (let i = 0; i <= n; i++) {
      const z = zNear + ((zFar - zNear) * i) / n;
      const [sx, sy] = P(x, surf(x, z), z);
      i ? ctx.lineTo(sx, sy) : ctx.moveTo(sx, sy);
    }
  }
  ctx.stroke();

  // 2. fade the far grid into the dark
  ctx.globalCompositeOperation = "destination-out";
  const fog = ctx.createLinearGradient(0, horizon - 4, 0, horizon + (h - horizon) * 0.42);
  fog.addColorStop(0, "rgba(0,0,0,1)");
  fog.addColorStop(1, "rgba(0,0,0,0)");
  ctx.fillStyle = fog;
  ctx.fillRect(0, 0, w, horizon + (h - horizon) * 0.42);
  // and soften the left and right edges so no line ends abruptly at the panel border
  const edge = Math.min(70, w * 0.08);
  const lf = ctx.createLinearGradient(0, 0, edge, 0);
  lf.addColorStop(0, "rgba(0,0,0,1)"); lf.addColorStop(1, "rgba(0,0,0,0)");
  ctx.fillStyle = lf; ctx.fillRect(0, 0, edge, h);
  const rf = ctx.createLinearGradient(w - edge, 0, w, 0);
  rf.addColorStop(0, "rgba(0,0,0,0)"); rf.addColorStop(1, "rgba(0,0,0,1)");
  ctx.fillStyle = rf; ctx.fillRect(w - edge, 0, edge, h);

  // 3. a soft light behind the mass
  ctx.globalCompositeOperation = "destination-over";
  const glow = ctx.createRadialGradient(mx, my, 0, mx, my, w * 0.38);
  glow.addColorStop(0, `rgba(255,154,60,${Math.min(1, 0.05 + 0.07 * dep).toFixed(3)})`);
  glow.addColorStop(1, "rgba(255,154,60,0)");
  ctx.fillStyle = glow;
  ctx.fillRect(0, 0, w, h);
  ctx.globalCompositeOperation = "source-over";

  // 4. the mass: small halo + core
  const halo = ctx.createRadialGradient(mx, my, mr * 0.6, mx, my, mr * 3.5);
  halo.addColorStop(0, `rgba(255,154,60,${Math.min(1, 0.1 + 0.1 * dep + 0.03 * breathe).toFixed(3)})`);
  halo.addColorStop(1, "rgba(255,120,40,0)");
  ctx.fillStyle = halo;
  ctx.beginPath(); ctx.arc(mx, my, mr * 3.5, 0, Math.PI * 2); ctx.fill();
  // gently shaded sphere: stays close to the accent colour, just enough light to read as round
  const core = ctx.createRadialGradient(mx - mr * 0.35, my - mr * 0.4, mr * 0.1, mx, my, mr);
  core.addColorStop(0, "#ffbd80");
  core.addColorStop(0.55, C.accent);
  core.addColorStop(1, "#e07f2c");
  ctx.fillStyle = core;
  ctx.beginPath(); ctx.arc(mx, my, mr, 0, Math.PI * 2); ctx.fill();
}

// level history: one-sided dB plot. Blue = everything captured, amber = what reaches
// the shaker, shaded gap = what the low-pass filter took away.
const ENV_LO = -36, ENV_HI = 0;
function drawEnv() {
  const { ctx, w, h } = fit($("env"));
  ctx.clearRect(0, 0, w, h);
  // top band: title and levels; bottom band: the legend (as tall as it needs; it may wrap)
  const legend = $("env").parentElement.querySelector(".graph-legend.at-bottom");
  const pad = { l: 36, r: 14, t: 32, b: 14 + (legend ? legend.offsetHeight : 16) };
  // scrolls smoothly: every sample sits histFrac of a step further left than its
  // slot, and the newest value so far is the live point at the right edge
  const X = (i) => pad.l + ((i - histFrac) / (HIST - 1)) * (w - pad.l - pad.r);
  const Y = (db) => pad.t + (ENV_HI - Math.max(ENV_LO, Math.min(ENV_HI, db))) / (ENV_HI - ENV_LO) * (h - pad.t - pad.b);
  const yBase = Y(ENV_LO), right = w - pad.r;

  // grid: dB lines + one line per second
  ctx.font = "10px 'JetBrains Mono', Consolas, monospace";
  ctx.textAlign = "right";
  ctx.lineWidth = 1;
  for (const db of [0, -12, -24, -36]) {
    ctx.strokeStyle = db === 0 ? GRID.zero : GRID.major;
    ctx.beginPath(); ctx.moveTo(pad.l, Y(db)); ctx.lineTo(right, Y(db)); ctx.stroke();
    ctx.fillStyle = C.faint;
    ctx.fillText(String(db), pad.l - 7, Y(db) + 3);
  }
  ctx.strokeStyle = "rgba(255,255,255,0.03)";
  ctx.beginPath();
  for (let s = 1; s * 30 < HIST; s++) { const x = X(HIST - 1 - s * 30); ctx.moveTo(x, pad.t); ctx.lineTo(x, yBase); }
  ctx.stroke();

  const ptsIn = Array.from(histIn, (v, i) => [X(i), Y(v)]);
  const ptsOut = Array.from(histOut, (v, i) => [X(i), Y(v)]);
  ptsIn.push([right, Y(histHeadIn)]);
  ptsOut.push([right, Y(histHeadOut)]);
  ctx.save();
  ctx.beginPath(); ctx.rect(pad.l, 0, right - pad.l, h); ctx.clip();

  // same series style as the filter chart: system audio behind, shaker output in front
  drawSeries(ctx, ptsIn, pad.t, yBase, "in");
  drawSeries(ctx, ptsOut, pad.t, yBase, "out");

  // limiter / clipping: glowing red capsules along the top (the ceiling)
  const limiterOn = !state || state.limiter;
  const hitAt = limiterOn ? -1.05 : 0;
  ctx.fillStyle = "#ff5d5d";
  ctx.shadowColor = "rgba(255,93,93,0.8)"; ctx.shadowBlur = 6;
  for (let i = 0; i < HIST; i++) {
    if (histOut[i] <= hitAt) continue;
    let j = i; while (j + 1 < HIST && histOut[j + 1] > hitAt) j++;
    const x0 = X(i) - 1.5, x1 = X(j) + 1.5;
    ctx.beginPath();
    if (ctx.roundRect) ctx.roundRect(x0, 3, Math.max(3, x1 - x0), 4, 2); else ctx.rect(x0, 3, Math.max(3, x1 - x0), 4);
    ctx.fill();
    i = j;
  }
  ctx.shadowBlur = 0;
  ctx.restore();

  // older data fades out on the left
  ctx.globalCompositeOperation = "destination-out";
  const fade = ctx.createLinearGradient(pad.l, 0, pad.l + 80, 0);
  fade.addColorStop(0, "rgba(0,0,0,0.85)");
  fade.addColorStop(1, "rgba(0,0,0,0)");
  ctx.fillStyle = fade;
  ctx.fillRect(pad.l, 0, 80, h);
  ctx.globalCompositeOperation = "source-over";

  // live endpoint with a soft halo
  const ex = right, ey = Y(histHeadOut);
  const halo = ctx.createRadialGradient(ex, ey, 0, ex, ey, 11);
  halo.addColorStop(0, "rgba(255,154,60,0.45)");
  halo.addColorStop(1, "rgba(255,154,60,0)");
  ctx.fillStyle = halo;
  ctx.beginPath(); ctx.arc(ex, ey, 11, 0, Math.PI * 2); ctx.fill();
  ctx.fillStyle = C.accent;
  ctx.beginPath(); ctx.arc(ex, ey, 3.5, 0, Math.PI * 2); ctx.fill();
  ctx.fillStyle = "#fff4e6";
  ctx.beginPath(); ctx.arc(ex, ey, 1.4, 0, Math.PI * 2); ctx.fill();
}

new ResizeObserver(() => drawCurve()).observe($("curve"));
const canvasSize = new ResizeObserver(() => { redraw = true; });
canvasSize.observe($("well")); canvasSize.observe($("env"));
if (document.fonts && document.fonts.ready) document.fonts.ready.then(drawCurve);
frame(performance.now());

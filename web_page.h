/**
 * web_page.h - ESP32 web UI stored in flash.
 * POST handles actions; GET serves "/" and "/status".
 */
#pragma once
#include <pgmspace.h>

const char PAGE_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="pl">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>BLIND//SYS</title>
<style>
:root {
  --bg0:#0a0a0a; --bg1:#111; --bg2:#1a1a1a; --bg3:#222;
  --fg0:#f0f0f0; --fg1:#aaa; --fg2:#555;
  --acc:#e03030; --acc2:#ff5a5a; --acc3:#ff000044;
  --grd:#ffffff08; --brd:#ffffff14;
  --grn:#1db954; --ylw:#f0a500;
  --font:'Courier New',monospace;
  --radius:2px;
}
.light {
  --bg0:#f0ede8; --bg1:#e8e4de; --bg2:#ddd9d2; --bg3:#ccc8c0;
  --fg0:#111; --fg1:#444; --fg2:#999;
  --acc:#c02020; --acc2:#e03030; --acc3:#c0202020;
  --grd:#00000008; --brd:#00000014;
}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg0);color:var(--fg0);font-family:var(--font);font-size:13px;min-height:100vh;overflow-x:hidden}
body::before{content:'';position:fixed;inset:0;background:repeating-linear-gradient(0deg,transparent,transparent 39px,var(--grd) 39px,var(--grd) 40px),repeating-linear-gradient(90deg,transparent,transparent 39px,var(--grd) 39px,var(--grd) 40px);pointer-events:none;z-index:0}
#app{position:relative;z-index:1;max-width:900px;margin:0 auto;padding:16px}

header{display:flex;align-items:center;justify-content:space-between;padding:12px 0 8px;border-bottom:1px solid var(--brd);margin-bottom:16px}
.logo{font-size:18px;font-weight:700;letter-spacing:.2em;color:var(--fg0)}
.logo span{color:var(--acc)}
.header-right{display:flex;align-items:center;gap:12px}
.theme-btn{background:none;border:1px solid var(--brd);color:var(--fg1);padding:4px 10px;cursor:pointer;font-family:var(--font);font-size:11px;letter-spacing:.1em}
.theme-btn:hover{border-color:var(--acc);color:var(--acc)}
.ntp-badge{font-size:10px;letter-spacing:.08em;padding:3px 8px;border:1px solid var(--brd);color:var(--fg2)}

#overlay{position:fixed;inset:0;background:#000d;z-index:100;display:flex;align-items:center;justify-content:center}
.overlay-box{background:var(--bg1);border:1px solid var(--acc);padding:40px 48px;text-align:center;max-width:380px}
.overlay-box h2{font-size:13px;letter-spacing:.25em;color:var(--acc);margin-bottom:8px}
.overlay-box p{color:var(--fg1);margin-bottom:28px;font-size:12px;line-height:1.6}
.overlay-btns{display:flex;gap:16px;justify-content:center}
.btn-primary{background:var(--acc);color:#fff;border:none;padding:12px 28px;font-family:var(--font);font-size:12px;letter-spacing:.15em;cursor:pointer;transition:opacity .15s}
.btn-primary:hover{opacity:.85}
.btn-secondary{background:none;border:1px solid var(--brd);color:var(--fg1);padding:12px 28px;font-family:var(--font);font-size:12px;letter-spacing:.15em;cursor:pointer}
.btn-secondary:hover{border-color:var(--fg1)}

.grid{display:grid;gap:12px}
.grid-2{grid-template-columns:1fr 1fr}
@media(max-width:600px){.grid-2{grid-template-columns:1fr}}

.card{background:var(--bg1);border:1px solid var(--brd);padding:16px 18px}
.card-label{font-size:10px;letter-spacing:.2em;color:var(--acc);margin-bottom:10px;text-transform:uppercase}

.status-row{display:flex;align-items:center;gap:10px;margin-bottom:8px}
.dot{width:8px;height:8px;border-radius:50%;flex-shrink:0}
.dot-green{background:var(--grn);box-shadow:0 0 6px var(--grn)}
.dot-red{background:var(--acc);box-shadow:0 0 6px var(--acc)}
.dot-yellow{background:var(--ylw);box-shadow:0 0 6px var(--ylw)}
.dot-gray{background:var(--fg2)}

.blind-state{font-size:42px;font-weight:700;letter-spacing:.05em;line-height:1;padding:8px 0}
.state-up{color:var(--grn)}
.state-down{color:var(--acc)}
.state-unknown{color:var(--ylw)}

.ctrl-btn{width:100%;padding:18px 8px;background:var(--bg2);border:1px solid var(--brd);color:var(--fg0);font-family:var(--font);font-size:12px;letter-spacing:.2em;cursor:pointer;transition:background .1s,border-color .1s;user-select:none;-webkit-user-select:none;touch-action:none}
.ctrl-btn:hover{background:var(--bg3)}
.ctrl-btn.active-up{background:var(--grn)!important;border-color:var(--grn)!important;color:#000!important}
.ctrl-btn.active-down{background:var(--acc)!important;border-color:var(--acc)!important;color:#fff!important}
.ctrl-btn-full{width:100%;padding:14px;background:var(--bg2);border:1px solid var(--brd);color:var(--fg0);font-family:var(--font);font-size:11px;letter-spacing:.15em;cursor:pointer;transition:background .1s}
.ctrl-btn-full:hover{background:var(--bg3);border-color:var(--fg1)}
.ctrl-btn-full.up{border-color:#1db95433}
.ctrl-btn-full.down{border-color:#e0303033}

.field{display:flex;flex-direction:column;gap:5px;margin-bottom:10px}
.field label{font-size:10px;letter-spacing:.15em;color:var(--fg2)}
.field input[type=number],.field input[type=time]{background:var(--bg2);border:1px solid var(--brd);color:var(--fg0);padding:8px 10px;font-family:var(--font);font-size:13px;width:100%;outline:none}
.field input:focus{border-color:var(--acc)}
.toggle-row{display:flex;align-items:center;justify-content:space-between;margin-bottom:8px}
.toggle-row label{font-size:11px;color:var(--fg1);letter-spacing:.1em}
.toggle{appearance:none;-webkit-appearance:none;width:36px;height:18px;background:var(--bg3);border:1px solid var(--brd);border-radius:9px;cursor:pointer;position:relative;transition:background .2s}
.toggle:checked{background:var(--acc);border-color:var(--acc)}
.toggle::after{content:'';position:absolute;width:12px;height:12px;background:#fff;border-radius:50%;top:2px;left:2px;transition:left .2s}
.toggle:checked::after{left:20px}
.save-btn{width:100%;padding:10px;background:var(--acc);border:none;color:#fff;font-family:var(--font);font-size:11px;letter-spacing:.2em;cursor:pointer;margin-top:8px}
.save-btn:hover{opacity:.85}
.save-btn.green{background:var(--grn);color:#000}

.log-list{list-style:none;max-height:180px;overflow-y:auto;display:flex;flex-direction:column-reverse;gap:3px}
.log-list li{font-size:11px;color:var(--fg1);padding:3px 0;border-bottom:1px solid var(--brd);white-space:nowrap;overflow:hidden;text-overflow:ellipsis;flex-shrink:0}
.log-list li:first-child{color:var(--fg0)}

.sun-row{display:flex;justify-content:space-between;font-size:11px;color:var(--fg1);margin-top:6px}
.sun-row span{color:var(--ylw)}
.delay-grid{display:grid;grid-template-columns:1fr 1fr 1fr;gap:8px;align-items:end}
.divider{border:none;border-top:1px solid var(--brd);margin:12px 0}
.ticker{font-size:11px;color:var(--fg2);margin-top:6px;letter-spacing:.05em}

::-webkit-scrollbar{width:4px}
::-webkit-scrollbar-track{background:transparent}
::-webkit-scrollbar-thumb{background:var(--brd)}
</style>
</head>
<body>

<div id="overlay">
  <div class="overlay-box">
    <h2>// INIT // POSITION_CHECK</h2>
    <p>System uruchomiony.<br>Określ aktualną pozycję żaluzji,<br>aby kontynuować.</p>
    <div class="overlay-btns">
      <button class="btn-secondary" onclick="initPos('up')">▲ PODNIESIONA</button>
      <button class="btn-primary"   onclick="initPos('down')">▼ OPUSZCZONA</button>
    </div>
  </div>
</div>

<div id="app">
  <header>
    <div class="logo">BLIND<span>//</span>SYS</div>
    <div class="header-right">
      <div class="ntp-badge" id="ntp-badge">NTP: --</div>
      <div class="ntp-badge" id="clock">--:--:--</div>
      <button class="theme-btn" onclick="toggleTheme()">◑ MOTYW</button>
    </div>
  </header>

  <div class="grid grid-2" style="margin-bottom:12px">
    <div class="card">
      <div class="card-label">// STATUS SYSTEMU</div>
      <div class="blind-state" id="blind-state-txt">---</div>
      <div class="ticker" id="blind-state-sub">Oczekiwanie na inicjalizację</div>
      <hr class="divider">
      <div class="status-row"><div class="dot dot-green" id="d-wifi"></div><span>WiFi: <span id="s-wifi">--</span></span></div>
      <div class="status-row"><div class="dot dot-gray" id="d-motor"></div><span>Silnik: <span id="s-motor">STOP</span></span></div>
      <div class="status-row"><div class="dot dot-gray" id="d-auto"></div><span>Auto: <span id="s-auto">OFF</span></span></div>
      <div class="status-row"><div class="dot dot-gray" id="d-delay"></div><span>Zaplanowane: <span id="s-delay">–</span></span></div>
      <div class="sun-row">
        <span>🌅 <span id="s-rise">--:--</span></span>
        <span>🌇 <span id="s-set">--:--</span></span>
      </div>
    </div>

    <div class="card">
      <div class="card-label">// STEROWANIE RĘCZNE</div>
      <div style="display:flex;flex-direction:column;gap:10px">
        <button class="ctrl-btn" id="btn-up" oncontextmenu="return false"
          onpointerdown="manualStart('up')" onpointerup="manualEnd('up')"
          onpointercancel="manualEnd('up')" onpointerleave="manualEnd('up')">
          ▲ GÓRA<br><span style="font-size:10px;color:var(--fg2)">(przytrzymaj)</span>
        </button>
        <button class="ctrl-btn" id="btn-down" oncontextmenu="return false"
          onpointerdown="manualStart('down')" onpointerup="manualEnd('down')"
          onpointercancel="manualEnd('down')" onpointerleave="manualEnd('down')">
          ▼ DÓŁ<br><span style="font-size:10px;color:var(--fg2)">(przytrzymaj)</span>
        </button>
        <button class="ctrl-btn-full up"   onclick="fullCmd('up')">▲▲ PEŁNE PODNIESIENIE</button>
        <button class="ctrl-btn-full down" onclick="fullCmd('down')">▼▼ PEŁNE OPUSZCZENIE</button>
      </div>
    </div>
  </div>

  <div class="grid grid-2" style="margin-bottom:12px">
    <div class="card">
      <div class="card-label">// CZASY RUCHU</div>
      <div class="field">
        <label>CZAS PODNOSZENIA (s)</label>
        <input type="number" id="t-up" value="10" min="1" max="120" step="0.1">
      </div>
      <div class="field">
        <label>CZAS OPUSZCZANIA (s)</label>
        <input type="number" id="t-down" value="10" min="1" max="120" step="0.1">
      </div>
      <button class="save-btn green" onclick="saveTiming()">ZAPISZ CZASY</button>
    </div>

    <div class="card">
      <div class="card-label">// HARMONOGRAM</div>
      <div class="toggle-row">
        <label>Auto podnoszenie</label>
        <input type="checkbox" class="toggle" id="chk-rise">
      </div>
      <div class="field"><label>GODZINA PODNOSZENIA</label>
        <input type="time" id="t-rise-time" value="06:00">
      </div>
      <div class="toggle-row">
        <label>Auto opuszczanie</label>
        <input type="checkbox" class="toggle" id="chk-drop">
      </div>
      <div class="field"><label>GODZINA OPUSZCZANIA</label>
        <input type="time" id="t-drop-time" value="21:00">
      </div>
      <button class="save-btn" onclick="saveSchedule()">ZAPISZ HARMONOGRAM</button>
    </div>
  </div>

  <div class="grid grid-2" style="margin-bottom:12px">
    <div class="card">
      <div class="card-label">// WSCHÓD / ZACHÓD SŁOŃCA</div>
      <p style="font-size:11px;color:var(--fg2);margin-bottom:10px">Katowice (50.26°N 19.02°E)</p>
      <div class="toggle-row">
        <label>Podnoszenie @ wschód</label>
        <input type="checkbox" class="toggle" id="chk-sun-rise">
      </div>
      <div class="toggle-row">
        <label>Opuszczanie @ zachód</label>
        <input type="checkbox" class="toggle" id="chk-sun-set">
      </div>
      <div class="sun-row" style="margin-top:12px;font-size:12px">
        <div>Wschód: <span style="color:var(--ylw)" id="sun-rise-disp">--:--</span></div>
        <div>Zachód: <span style="color:var(--acc)" id="sun-set-disp">--:--</span></div>
      </div>
      <button class="save-btn" onclick="saveSun()">ZAPISZ</button>
    </div>

    <div class="card">
      <div class="card-label">// ZAPLANUJ JEDNORAZOWO</div>
      <div class="delay-grid">
        <div class="field">
          <label>OPÓŹNIENIE (min)</label>
          <input type="number" id="delay-min" value="30" min="1" max="240">
        </div>
        <button class="ctrl-btn-full up"   style="height:38px" onclick="setDelay('up')">▲ ZA X MIN</button>
        <button class="ctrl-btn-full down" style="height:38px" onclick="setDelay('down')">▼ ZA X MIN</button>
      </div>
      <div id="delay-status" style="margin-top:10px;font-size:11px;color:var(--fg2)">Brak zaplanowanej akcji</div>
      <button class="save-btn" style="margin-top:8px;background:var(--bg3);border:1px solid var(--brd);color:var(--fg1)" onclick="cancelDelay()">ANULUJ ZAPLANOWANE</button>

      <hr class="divider">
      <div class="card-label" style="margin-bottom:8px">// MOTYW AUTO</div>
      <div class="toggle-row">
        <label>Auto (jasny rano / ciemny wieczór)</label>
        <input type="checkbox" class="toggle" id="chk-theme-auto" onchange="themeAutoToggle(this.checked)">
      </div>
    </div>
  </div>

  <div class="card">
    <div class="card-label">// LOGI SYSTEMU</div>
    <ul class="log-list" id="log-list"></ul>
  </div>
</div>

<script>
'use strict';

// ── State ────────────────────────────────────────────────────
let ready = false;
let themeAuto = false;
let darkMode = true;
let manualActive = null;
let manualTimer = null;
const dirty = new Set(); // holds uncommitted field edits to avoid poll overwrites

const $ = id => document.getElementById(id);

function api(url) {
  return fetch(url, { method: 'POST' }).then(r => {
    if (!r.ok) throw new Error('HTTP ' + r.status);
    return r.json();
  });
}

// Track user edits so live updates don't stomp on them
['input', 'change'].forEach(ev =>
  document.addEventListener(ev, e => { if (e.target.id) dirty.add(e.target.id); }));
function markClean(...ids) { ids.forEach(i => dirty.delete(i)); }
function syncField(id, value) {
  if (dirty.has(id)) return;
  const el = $(id);
  if (el.type === 'checkbox') el.checked = value; else el.value = value;
}

// ── Setup ───────────────────────────────────────────────────
function initPos(pos) {
  api('/init?pos=' + pos).then(d => {
    if (d.ok) { $('overlay').style.display = 'none'; ready = true; poll(); }
  }).catch(() => {});
}

// ── Theme ────────────────────────────────────────────────────
function applyTheme(dark) { darkMode = dark; document.body.classList.toggle('light', !dark); }
function toggleTheme() { themeAuto = false; $('chk-theme-auto').checked = false; applyTheme(!darkMode); }
function themeAutoToggle(on) { themeAuto = on; if (on) applyAutoTheme(); }
function applyAutoTheme() { const h = new Date().getHours(); applyTheme(h < 7 || h >= 20); }

// ── Clock ────────────────────────────────────────────────────
function tickClock() {
  const now = new Date();
  const pad = n => String(n).padStart(2, '0');
  $('clock').textContent = pad(now.getHours()) + ':' + pad(now.getMinutes()) + ':' + pad(now.getSeconds());
  if (themeAuto) applyAutoTheme();
}
setInterval(tickClock, 1000);

// ── Status polling ───────────────────────────────────────────
function poll() { fetch('/status').then(r => r.json()).then(upd).catch(() => {}); }
poll();
setInterval(poll, 2000);

function dot(id, color) { $(id).className = 'dot dot-' + color; }

function buildAutoStr(d) {
  const p = [];
  if (d.autoRise) p.push('↑ ' + d.riseTime);
  if (d.autoDrop) p.push('↓ ' + d.dropTime);
  if (d.sunRise)  p.push('↑ wschód');
  if (d.sunSet)   p.push('↓ zachód');
  return p.length ? p.join(' | ') : 'OFF';
}

function upd(d) {
  if (!d || d.pos === undefined) return;
  if (d.pos !== 'unknown' && !ready) { $('overlay').style.display = 'none'; ready = true; }

  const st = $('
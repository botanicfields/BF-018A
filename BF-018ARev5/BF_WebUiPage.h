// BF-018A Rev.5
// the web UI page: one self-contained HTML file kept in flash
//
// It polls /api/status every second and /api/log every 2 seconds, and draws the time code
// from the device's clock with the same encoding as TcoValue().
//
#pragma once

static const char web_ui_page[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>JJY Simulator</title>
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='%23f4b13d' stroke-width='2' stroke-linecap='round'%3E%3Ccircle cx='12' cy='12' r='2'/%3E%3Cpath d='M8.5 15.5a5 5 0 0 1 0-7M15.5 8.5a5 5 0 0 1 0 7M5.6 18.4a9 9 0 0 1 0-12.8M18.4 5.6a9 9 0 0 1 0 12.8'/%3E%3C/svg%3E">
<style>
:root{--ground:#0f1113;--panel:#161a1d;--inset:#1d2226;--hair:#2a3036;--field:#6b737c;--t1:#ecebe6;--t2:#b3b9bf;--t3:#959ca4;--amber:#f4b13d;--ink:#1b1400;--red:#f47a70;--mono:ui-monospace,SFMono-Regular,Menlo,Consolas,"Liberation Mono",monospace;color-scheme:dark}
*{box-sizing:border-box}
body{margin:0;background:var(--ground);color:var(--t1);font:15px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,"Helvetica Neue",Arial,sans-serif}
button,input,select{font:inherit;color:inherit}
:focus-visible{outline:2px solid var(--amber);outline-offset:2px}
[hidden]{display:none!important}
h2{margin:0;font-size:15px;font-weight:600;line-height:1.3}
code,.mono{font-family:var(--mono)}
.wrap{max-width:1160px;margin:0 auto;padding-left:clamp(16px,3vw,32px);padding-right:clamp(16px,3vw,32px)}
.sub{font-size:13px;color:var(--t3)}
.row{display:flex;flex-wrap:wrap;align-items:center;gap:8px}
header{border-bottom:1px solid var(--hair)}
.top{display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;gap:12px 24px;padding-top:14px;padding-bottom:14px}
.brand{display:flex;align-items:center;gap:12px;min-width:0}
.brand svg{flex:none}
.brand b{display:block;font-size:17px;font-weight:650;line-height:1.25}
.brand small{display:block;font:13px var(--mono);color:var(--t3)}
.pill{display:inline-flex;align-items:center;gap:8px;min-height:34px;padding:4px 14px;border-radius:999px;border:1px solid var(--hair);background:var(--panel);font-size:14px;font-weight:600}
.pill i{width:10px;height:10px;border-radius:50%;border:2px solid var(--t3)}
.pill.tx{border-color:#4a3a1c;background:#1b1710}
.pill.tx i{border:0;background:#4a3a1c}
.pill.tx.on i{background:var(--amber);box-shadow:0 0 10px rgba(244,177,61,.7)}
.pill.bad{border-color:#5a2a26;background:#221514}
.pill.bad i{border:0;background:var(--red)}
main{display:flex;flex-direction:column;gap:20px;padding-top:24px;padding-bottom:56px}
main.stale .fade{opacity:.55}
.banner{display:flex;gap:12px;align-items:flex-start;padding:14px 16px;border-radius:12px;border:1px solid var(--red);background:#2a1715}
.banner svg{flex:none;stroke:var(--red)}
.banner div{display:flex;flex-direction:column;gap:4px;min-width:0}
.banner strong{font-weight:600}
.banner span{font-size:14px;color:#e8cdc9}
.banner.info{border-color:var(--field);background:var(--inset)}
.banner.info svg{stroke:var(--t2)}
.banner.info span{color:var(--t2)}
.card{background:var(--panel);border:1px solid var(--hair);border-radius:12px;padding:16px 20px 18px;display:flex;flex-direction:column;gap:14px;min-width:0}
.head{display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;gap:4px 12px}
.hero{display:flex;flex-wrap:wrap;gap:16px;align-items:stretch}
.clockbox{flex:999 1 420px;min-width:0;display:flex;flex-direction:column;justify-content:center;gap:6px;padding:4px 0}
.eyebrow{font-size:12px;letter-spacing:.08em;text-transform:uppercase;color:var(--t3)}
.clock{font:500 clamp(56px,10vw,104px)/1 var(--mono);letter-spacing:-.02em;font-variant-numeric:tabular-nums}
.date{font-size:18px;color:var(--t2)}
.sentence{margin:6px 0 0;max-width:62ch;color:var(--t2)}
.ledcard{flex:1 1 300px;padding-top:12px}
.switch{display:inline-flex;align-items:center;gap:10px;min-height:44px;padding:0 0 0 8px;border:0;background:transparent;color:var(--t2);font-size:13px;cursor:pointer}
.switch .track{position:relative;display:inline-block;width:42px;height:24px;border-radius:999px;border:1px solid var(--field);background:var(--inset)}
.switch .knob{position:absolute;top:2px;left:2px;width:18px;height:18px;border-radius:50%;background:var(--t2);transition:left .15s}
.switch[aria-checked="true"] .track{background:var(--amber);border-color:var(--amber)}
.switch[aria-checked="true"] .knob{left:20px;background:var(--ink)}
.switch:disabled{cursor:not-allowed;opacity:.5}
.ledrow{display:flex;align-items:center;gap:16px}
.ledrow div{display:flex;flex-direction:column;gap:2px;min-width:0}
.leddot{flex:none;width:56px;height:56px;border-radius:50%;border:1px solid var(--hair);background:#24292e;box-shadow:inset 0 2px 6px rgba(0,0,0,.5)}
.ledlist{display:flex;flex-direction:column;gap:6px;font-size:13px}
.ledlist div{display:grid;grid-template-columns:64px 80px minmax(0,1fr);gap:8px;align-items:center}
.ledlist i{display:inline-block;width:8px;height:8px;margin-right:8px;border-radius:50%;background:#3a4148}
.ledlist span:nth-child(2){color:var(--t2)}
.ledlist span:nth-child(3){color:var(--t3)}
.chip{font:13px var(--mono);color:var(--amber);border:1px solid #4a3a1c;border-radius:999px;padding:3px 10px}
.tc{display:flex;flex-wrap:wrap;gap:20px 28px;align-items:flex-start}
.framecol{flex:999 1 560px;min-width:0;display:flex;flex-direction:column;gap:10px}
.frame{display:flex;flex-direction:column;gap:8px}
.frow{display:flex;flex-wrap:wrap;align-items:center;gap:4px 12px}
.flabel{flex:1 0 96px;display:flex;flex-direction:column;line-height:1.25}
.flabel span{font-size:13px;color:var(--t2)}
.flabel small{font:11px var(--mono);color:var(--t3)}
.cells{flex:999 1 300px;min-width:0;display:grid;grid-template-columns:repeat(10,minmax(0,1fr));gap:4px}
.cell{display:flex;flex-direction:column;gap:3px;min-width:0}
.box{height:34px;border-radius:5px;background:var(--inset);border:1px solid var(--hair);display:flex;flex-direction:column;justify-content:space-between;padding:5px 4px}
.sym{font:12px/1 var(--mono);text-align:center;min-height:12px}
.bar{position:relative;height:5px;border-radius:2px;background:var(--hair);overflow:hidden}
.bar i{position:absolute;left:0;top:0;bottom:0;width:0}
.w{font:11px/1 var(--mono);height:11px;text-align:center;color:var(--t3);white-space:nowrap;overflow:hidden}
.cell.now .box{border:2px solid var(--amber)}
.cell.future{opacity:.4}
.frame.idle .cell{opacity:.5}
.k-m .sym{color:var(--amber)}.k-m .bar i{background:var(--amber)}
.k-1 .sym{color:var(--t1)}.k-1 .bar i{background:#e7e4dc}
.k-0 .sym{color:var(--t3)}.k-0 .bar i{background:#7b838c}
.legend{display:flex;flex-wrap:wrap;gap:6px 20px;padding-top:4px;font-size:12px;color:var(--t2)}
.legend span{display:inline-flex;align-items:center;gap:8px}
.legend b{position:relative;display:inline-block;width:30px;height:5px;border-radius:2px;background:var(--hair);overflow:hidden}
.legend b i{position:absolute;left:0;top:0;bottom:0}
.aside{flex:1 1 250px;min-width:0;display:flex;flex-direction:column;gap:16px}
.second{background:var(--inset);border-radius:10px;padding:14px 16px;display:flex;flex-direction:column;gap:10px}
.secrow{display:flex;align-items:center;gap:14px}
.secrow div{display:flex;flex-direction:column;gap:2px;min-width:0}
.secnum{font:500 40px/1 var(--mono);font-variant-numeric:tabular-nums}
.slots{display:grid;grid-template-columns:repeat(10,minmax(0,1fr));gap:3px}
.slots span{height:16px;border-radius:3px;background:#232a30}
.scale{display:flex;justify-content:space-between;font:11px var(--mono);color:var(--t3)}
dl.kv{margin:0;display:grid;grid-template-columns:auto minmax(0,1fr);gap:8px 16px;align-items:baseline}
dl.kv dt{font-size:14px;color:var(--t3)}
dl.kv dd{margin:0;text-align:right;font:13.5px var(--mono);font-variant-numeric:tabular-nums;overflow-wrap:anywhere}
.grid3{display:grid;grid-template-columns:repeat(auto-fit,minmax(min(280px,100%),1fr));gap:16px}
.grid2{display:grid;grid-template-columns:repeat(auto-fit,minmax(min(420px,100%),1fr));gap:16px}
.bottom{margin-top:auto;display:flex;flex-direction:column;gap:10px}
.stack{display:flex;flex-direction:column;gap:16px}
.btn{min-height:44px;padding:0 16px;border-radius:8px;border:1px solid var(--field);background:transparent;color:var(--t1);font-size:14px;font-weight:500;cursor:pointer}
.btn:disabled{cursor:not-allowed;opacity:.5}
.btn.primary{background:var(--amber);border-color:var(--amber);color:var(--ink);font-weight:600}
.btn.primary:disabled{background:var(--inset);border-color:var(--hair);color:var(--t3);opacity:1}
.btn.danger{color:var(--red)}
.btn.red{background:var(--red);border-color:var(--red);color:#1d0b09;font-weight:600}
.confirm{border:1px solid var(--red);border-radius:10px;padding:12px 14px;background:#221514;display:flex;flex-direction:column;gap:10px;font-size:14px}
form{display:flex;flex-direction:column;gap:18px}
fieldset{margin:0;padding:0;border:0;min-width:0;display:flex;flex-direction:column;gap:8px}
legend,.field label{padding:0;font-size:14px;font-weight:600}
legend{margin-bottom:8px}
.opt{display:flex;align-items:center;gap:12px;min-height:52px;padding:8px 14px;border-radius:10px;border:1px solid var(--hair);cursor:pointer}
.opt:has(input:checked){border-color:var(--amber);background:#1f1a10}
.opt input{flex:none;margin:0;width:18px;height:18px;accent-color:var(--amber)}
.opt span{display:flex;flex-direction:column;line-height:1.3}
.opt small{font-size:13px;color:var(--t3)}
.field{display:flex;flex-direction:column;gap:6px}
.field select,.field input{min-height:44px;padding:0 12px;border-radius:8px;border:1px solid var(--field);background:var(--inset);font-size:15px}
.field input{font:14px var(--mono)}
.field code{color:var(--t2);overflow-wrap:anywhere}
.big{font-size:44px;font-weight:600;line-height:1.05;letter-spacing:-.01em}
.stats{margin:0;display:grid;grid-template-columns:repeat(auto-fit,minmax(min(110px,100%),1fr));gap:12px}
.stats div{display:flex;flex-direction:column;gap:2px}
.stats dt{font-size:12px;color:var(--t3)}
.stats dd{margin:0;font:15px var(--mono)}
details{border-top:1px solid var(--hair)}
summary{cursor:pointer;padding:12px 0 4px;font-size:14px;color:var(--t2)}
table{width:100%;border-collapse:collapse;margin-top:6px;font-size:13px}
th{text-align:right;font-weight:500;color:var(--t3);padding:6px 0 6px 12px;border-bottom:1px solid var(--hair)}
th:first-child{text-align:left;padding-left:0}
td{padding:5px 0 5px 12px;text-align:right;font-family:var(--mono);font-variant-numeric:tabular-nums}
td:first-child{text-align:left;padding-left:0}
tr.mid td{color:var(--amber)}
pre{margin:0;max-height:20em;overflow:auto;background:#0b0d0f;border:1px solid var(--hair);border-radius:8px;padding:12px 14px;font:12.5px/1.6 var(--mono);color:#cdd2d6;white-space:pre}
.toast{position:fixed;left:50%;bottom:24px;transform:translateX(-50%);width:max-content;max-width:calc(100% - 32px);padding:12px 18px;border-radius:10px;background:var(--t1);color:#121416;font-size:14px;font-weight:500;box-shadow:0 10px 30px rgba(0,0,0,.5);opacity:0;visibility:hidden;transition:opacity .2s,visibility .2s;z-index:10}
.toast.show{opacity:1;visibility:visible}
</style>
</head>
<body>
<header>
<div class="wrap top">
<div class="brand">
<svg aria-hidden="true" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="#f4b13d" stroke-width="1.8" stroke-linecap="round"><circle cx="12" cy="12" r="2"/><path d="M8.5 15.5a5 5 0 0 1 0-7M15.5 8.5a5 5 0 0 1 0 7M5.6 18.4a9 9 0 0 1 0-12.8M18.4 5.6a9 9 0 0 1 0 12.8"/></svg>
<div><b>JJY Simulator</b><small id="host">&nbsp;</small></div>
</div>
<output id="pill" class="pill"><i></i><span id="pillText">Connecting</span></output>
</div>
</header>

<main id="main" class="wrap">
<div id="banner" class="banner" hidden>
<svg aria-hidden="true" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M12 3 2.5 20h19z"/><path d="M12 10v4.5"/><path d="M12 17.5v.01"/></svg>
<div><strong id="bannerTitle"></strong><span id="bannerText"></span></div>
</div>

<section class="hero" aria-label="Device clock">
<div class="clockbox fade">
<span class="eyebrow">Device clock · <span id="zone">-</span></span>
<span id="clock" class="clock">--:--:--</span>
<span id="date" class="date">Connecting to the simulator</span>
<p id="sentence" class="sentence"></p>
</div>
<div class="card ledcard fade">
<div class="head">
<h2>Status LED</h2>
<button id="ledSwitch" type="button" role="switch" aria-checked="true" class="switch"><span>LED monitor</span><span class="track" aria-hidden="true"><span class="knob"></span></span></button>
</div>
<div class="ledrow"><span id="ledDot" class="leddot" aria-hidden="true"></span><div><b id="ledHead">-</b><span id="ledSub" class="sub"></span></div></div>
<div class="ledlist">
<div><span><i id="ledR"></i>Red</span><span id="ledRState">-</span><span id="ledRText"></span></div>
<div><span><i id="ledG"></i>Green</span><span id="ledGState">-</span><span id="ledGText"></span></div>
<div><span><i id="ledB"></i>Blue</span><span id="ledBState">-</span><span id="ledBText"></span></div>
</div>
</div>
</section>

<section class="card fade" aria-labelledby="tcTitle">
<div class="head">
<div class="row"><h2 id="tcTitle">Time code</h2><span id="tcCaption" class="sub"></span></div>
<span id="tcChip" class="chip">-</span>
</div>
<div class="tc">
<div class="framecol">
<div id="frame" class="frame idle" aria-hidden="true"></div>
<div class="legend">
<span><b><i style="width:20%;background:#f4b13d"></i></b>Marker (M, P0-P5): carrier on 0.2 s</span>
<span><b><i style="width:50%;background:#e7e4dc"></i></b>Bit 1: on 0.5 s</span>
<span><b><i style="width:80%;background:#7b838c"></i></b>Bit 0: on 0.8 s</span>
</div>
</div>
<aside class="aside" aria-label="This second and decoded values">
<div class="second">
<span class="eyebrow">This second</span>
<div class="secrow"><span id="secNum" class="secnum">--</span><div><span id="secWhat">No time code</span><span id="secCarrier" class="sub"></span></div></div>
<div id="slots" class="slots" aria-hidden="true"></div>
<div class="scale" aria-hidden="true"><span>0</span><span>0.5</span><span>1.0 s</span></div>
</div>
<dl class="kv">
<dt>Minute</dt><dd id="decMin">-</dd>
<dt>Hour</dt><dd id="decHour">-</dd>
<dt>Day of year</dt><dd id="decDay">-</dd>
<dt>Year</dt><dd id="decYear">-</dd>
<dt>Weekday</dt><dd id="decWday">-</dd>
<dt>Parity: hour, minute</dt><dd id="decParity">-</dd>
</dl>
</aside>
</div>
</section>

<div class="grid3 fade">
<section class="card" aria-labelledby="wifiTitle">
<div class="head"><h2 id="wifiTitle">Wi-Fi</h2><span id="wifiCode" class="mono sub"></span></div>
<dl class="kv">
<dt>Status</dt><dd id="wifiStatus">-</dd>
<dt>Network</dt><dd id="wifiSsid">-</dd>
<dt>Signal</dt><dd id="wifiRssi">-</dd>
<dt>Address</dt><dd id="wifiIp">-</dd>
<dt>Reconnect attempts</dt><dd id="wifiRetries">-</dd>
</dl>
</section>
<section class="card" aria-labelledby="timeTitle">
<h2 id="timeTitle">Time source</h2>
<dl class="kv">
<dt>Source</dt><dd id="timeSource">-</dd>
<dt>Server</dt><dd id="timeServer">-</dd>
<dt>Last sync</dt><dd id="timeLast">-</dd>
<dt>Next sync</dt><dd id="timeNext">-</dd>
</dl>
<div class="bottom"><div class="row"><button id="btnSync" type="button" class="btn">Sync time now</button></div></div>
</section>
<section class="card" aria-labelledby="devTitle">
<h2 id="devTitle">Device</h2>
<dl class="kv">
<dt>Board</dt><dd id="devBoard">-</dd>
<dt>Antenna output</dt><dd id="devPin">-</dd>
<dt>Uptime</dt><dd id="devUptime">-</dd>
<dt>Last restart</dt><dd id="devReset">-</dd>
<dt>Free memory</dt><dd id="devHeap">-</dd>
</dl>
<div class="bottom">
<div id="forgetBox" class="confirm" hidden>
<span id="forgetText"></span>
<div class="row"><button id="btnForgetNo" type="button" class="btn">Cancel</button><button id="btnForgetYes" type="button" class="btn red">Forget network</button></div>
</div>
<div id="devButtons" class="row"><button id="btnRestart" type="button" class="btn">Restart</button><button id="btnForget" type="button" class="btn danger">Forget Wi-Fi network…</button></div>
</div>
</section>
</div>

<div class="grid2 fade">
<section class="card" aria-labelledby="setTitle">
<h2 id="setTitle">Settings</h2>
<form id="setForm" novalidate>
<fieldset>
<legend>Carrier frequency</legend>
<label class="opt"><input type="radio" name="freq" value="40000"><span><b>40 kHz</b><small>Like the Fukushima transmitter (east Japan)</small></span></label>
<label class="opt"><input type="radio" name="freq" value="60000"><span><b>60 kHz</b><small>Like the Saga transmitter (west Japan)</small></span></label>
<span class="sub">In east Japan, 60 kHz lets you tell whether a clock synced to the simulator or to the real signal. The ESP32 can only approximate 60 kHz.</span>
</fieldset>
<div class="field">
<label for="resSel">Carrier fine-tuning</label>
<select id="resSel"></select>
<span id="resNote" class="sub"></span>
</div>
<div class="field">
<label for="tzSel">Time zone</label>
<select id="tzSel"></select>
<input id="tzCustom" type="text" maxlength="63" autocomplete="off" spellcheck="false" aria-label="POSIX TZ string" placeholder="POSIX TZ, e.g. JST-9" hidden>
<span class="sub">A JJY clock shows whatever local time it receives, so choose your own zone to use one outside Japan. Sent to the device as <code id="tzValue">-</code></span>
</div>
<div class="field">
<label for="ntp">NTP server</label>
<input id="ntp" type="text" maxlength="63" autocomplete="off" spellcheck="false">
</div>
<div class="row">
<button id="btnSave" type="submit" class="btn primary" disabled>Save and restart</button>
<span id="setNote" class="sub"></span>
</div>
</form>
</section>
<section class="card" aria-labelledby="tmTitle">
<div><h2 id="tmTitle">Signal timing</h2><span class="sub">How evenly the 100 ms slots that switch the carrier start, since the last restart. Updated every minute.</span></div>
<span id="tmNone" class="sub">Timing statistics appear a minute after the time code starts.</span>
<div id="tmSome" class="stack" hidden>
<div><div id="tmShare" class="big">-</div><span class="sub">of slots within ±0.05 ms of schedule</span></div>
<dl class="stats">
<div><dt>Std deviation</dt><dd id="tmSdv">-</dd></div>
<div><dt>Worst early</dt><dd id="tmMin">-</dd></div>
<div><dt>Worst late</dt><dd id="tmMax">-</dd></div>
<div><dt>Slots counted</dt><dd id="tmN">-</dd></div>
</dl>
<details>
<summary>All 9 timing buckets</summary>
<table>
<thead><tr><th scope="col">Slot started vs 100 ms</th><th scope="col">Slots</th><th scope="col">Share</th></tr></thead>
<tbody id="tmRows"></tbody>
</table>
</details>
</div>
</section>
</div>

<section class="card fade" aria-labelledby="logTitle">
<div class="head"><h2 id="logTitle">Serial log</h2><span class="sub">The same output as the USB serial monitor (115200 baud)</span></div>
<pre id="log" tabindex="0"></pre>
</section>
</main>
<output id="toast" class="toast"></output>

<script>
'use strict';
const $ = id => document.getElementById(id);
const MARK = 255;
const DAYS = ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];
const MONTHS = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
const ROWS = ['Minute', 'Hour', 'Day of year', 'Day, parity', 'Year', 'Weekday'];
const WEIGHTS = [
  ['', '40', '20', '10', '', '8', '4', '2', '1', ''],
  ['', '', '20', '10', '', '8', '4', '2', '1', ''],
  ['', '', '200', '100', '', '80', '40', '20', '10', ''],
  ['8', '4', '2', '1', '', '', 'PA1', 'PA2', '', ''],
  ['', '80', '40', '20', '10', '8', '4', '2', '1', ''],
  ['4', '2', '1', '', '', '', '', '', '', '']];
const MARKERS = {0: 'M', 9: 'P1', 19: 'P2', 29: 'P3', 39: 'P4', 49: 'P5', 59: 'P0'};
const ZONES = [
  ['JST-9', 'Japan · JST, UTC+9'],
  ['UTC0', 'UTC'],
  ['GMT0BST,M3.5.0/1,M10.5.0', 'London · GMT/BST'],
  ['CET-1CEST,M3.5.0,M10.5.0/3', 'Central Europe · CET/CEST'],
  ['EST5EDT,M3.2.0,M11.1.0', 'US Eastern · EST/EDT'],
  ['PST8PDT,M3.2.0,M11.1.0', 'US Pacific · PST/PDT'],
  ['AEST-10AEDT,M10.1.0,M4.1.0/3', 'Sydney · AEST/AEDT']];
const BUCKETS = ['< −50 ms', '−50 to −5 ms', '−5 to −0.5 ms', '−0.5 to −0.05 ms', '±0.05 ms',
  '+0.05 to +0.5 ms', '+0.5 to +5 ms', '+5 to +50 ms', '≥ +50 ms'];
const WIFI_TEXT = {WL_CONNECTED: 'Connected', WL_NO_SSID_AVAIL: 'Network not found', WL_DISCONNECTED: 'Disconnected',
  WL_CONNECTION_LOST: 'Connection lost', WL_IDLE_STATUS: 'Idle', WL_CONNECT_FAILED: 'Connect failed'};
const TZ_CHARS = /^[A-Za-z0-9<>+\-,./:]{1,63}$/;
// POSIX TZ syntax, as the device checks it (WebUiValidTimeZone()): std offset [dst [offset] [,rule,rule]]
const TZ_SYNTAX = (() => {
  const name = '(?:[A-Za-z]{3,}|<[A-Za-z0-9+\\-]{3,}>)';
  const hms = '\\d{1,3}(?::\\d{1,3}){0,2}';
  const rule = '(?:M\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}|J\\d{1,3}|\\d{1,3})(?:/' + hms + ')?';
  return new RegExp('^' + name + '[+-]?' + hms + '(?:' + name + '(?:[+-]?' + hms + ')?(?:,' + rule + ',' + rule + ')?)?$');
})();
function validTz(tz) {
  if (!TZ_CHARS.test(tz) || !TZ_SYNTAX.test(tz)) {
    return false;
  }
  // the ranges the regular expression can't check
  const inRange = (t, max) => t.split(':').every((v, i) => Number(v) <= (i ? 59 : max));
  const body = tz.replace(/<[^>]*>/g, 'X').split(',');
  const offsets = body[0].match(/[+-]?\d[\d:]*/g) || [];
  if (!offsets.every(o => inRange(o.replace(/^[+-]/, ''), 24))) {
    return false;
  }
  return body.slice(1).every(r => {
    const [date, time] = r.split('/');
    const n = date.replace(/^[MJ]/, '').split('.').map(Number);
    const dateOk = date[0] === 'M' ? n[0] >= 1 && n[0] <= 12 && n[1] >= 1 && n[1] <= 5 && n[2] <= 6
      : date[0] === 'J' ? n[0] >= 1 && n[0] <= 365 : n[0] <= 365;
    return dateOk && (time === undefined || inRange(time, 167));
  });
}
// The LEDC timer divides its 80 MHz clock by a divider with 8 fractional bits, and counts 2^bits steps per cycle.
const RESOLUTIONS = [[8, '8-bit', 'default'], [2, '2-bit', ''], [1, '1-bit', '']];
function ledcFreq(f, bits) {
  const steps = 2 ** bits;
  return 80e6 * 256 / (Math.round(80e6 * 256 / (f * steps)) * steps);
}
const NTP_CHARS = /^[A-Za-z0-9.\-]{1,63}$/;

let st = null;           // the last /api/status
let clockBase = null;    // device clock = epoch + (performance.now() - perf)
let lastOk = 0;          // Date.now() of the last good status
let lastUptime = null;
let restarting = 0;      // Date.now() when a restart was asked for
let forgotten = false;   // Wi-Fi settings erased: the device won't come back on this network
let formDirty = false;
let ledBusy = false;
let logNext = 0, logText = '';
let frameKey = '', frameBits = null;

// device clock and local time fields (the device's own time zone)
const devNow = () => clockBase ? clockBase.epoch + (performance.now() - clockBase.perf) : Date.now();
const p2 = n => String(n).padStart(2, '0');
const num = n => Number(n).toLocaleString('en-US');
const hms = z => p2(z.hour) + ':' + p2(z.min) + ':' + p2(z.sec);

function fields(ms) {
  const d = new Date(ms + (st ? st.utc_offset : 0) * 1000);
  const year = d.getUTCFullYear();
  return {
    year, mon: d.getUTCMonth(), mday: d.getUTCDate(), hour: d.getUTCHours(), min: d.getUTCMinutes(), sec: d.getUTCSeconds(),
    wday: d.getUTCDay(), yday: Math.round((Date.UTC(year, d.getUTCMonth(), d.getUTCDate()) - Date.UTC(year, 0, 1)) / 86400000),
    ms: ((ms % 1000) + 1000) % 1000
  };
}

// the time code of a minute: the same encoding as TcoValue(), MARK for markers
function frame(z) {
  const bcd = a => (a % 10) + (Math.floor(a / 10) % 10) * 16 + (Math.floor(a / 100) % 10) * 256;
  const par = a => { let p = a; for (let i = 1; i < 8; i++) p += a >> i; return p % 2; };
  const h = bcd(z.hour), m = bcd(z.min), y = bcd(z.year), d = bcd(z.yday + 1), w = z.wday;
  const b = (v, k) => (v & k) ? 1 : 0;
  const f = new Array(60).fill(0);
  [0, 9, 19, 29, 39, 49, 59].forEach(i => { f[i] = MARK; });
  [[1, 0x40], [2, 0x20], [3, 0x10], [5, 8], [6, 4], [7, 2], [8, 1]].forEach(([s, k]) => { f[s] = b(m, k); });
  [[12, 0x20], [13, 0x10], [15, 8], [16, 4], [17, 2], [18, 1]].forEach(([s, k]) => { f[s] = b(h, k); });
  [[22, 0x200], [23, 0x100], [25, 0x80], [26, 0x40], [27, 0x20], [28, 0x10], [30, 8], [31, 4], [32, 2], [33, 1]].forEach(([s, k]) => { f[s] = b(d, k); });
  f[36] = par(h);
  f[37] = par(m);
  [[41, 0x80], [42, 0x40], [43, 0x20], [44, 0x10], [45, 8], [46, 4], [47, 2], [48, 1]].forEach(([s, k]) => { f[s] = b(y, k); });
  [[50, 4], [51, 2], [52, 1]].forEach(([s, k]) => { f[s] = b(w, k); });
  return f;
}
const onSlots = v => v === MARK ? 2 : v ? 5 : 8;  // carrier on for 0.2, 0.5 or 0.8 s
const fieldOf = s => s >= 1 && s <= 8 ? 'minute' : s >= 12 && s <= 18 ? 'hour' : s >= 22 && s <= 33 ? 'day of year'
  : s === 36 ? 'hour parity' : s === 37 ? 'minute parity' : s >= 41 && s <= 48 ? 'year' : s >= 50 && s <= 52 ? 'weekday' : '';

// build the 60 cells, the 10 slots and the bucket rows once
const el = (tag, cls, text) => { const e = document.createElement(tag); if (cls) e.className = cls; if (text) e.textContent = text; return e; };
const cells = [];
ROWS.forEach((label, r) => {
  const row = el('div', 'frow'), lab = el('div', 'flabel'), grid = el('div', 'cells');
  lab.append(el('span', '', label), el('small', '', 's ' + r * 10 + '-' + (r * 10 + 9)));
  for (let i = 0; i < 10; i++) {
    const c = el('div', 'cell'), box = el('div', 'box'), sym = el('span', 'sym'), bar = el('span', 'bar'), fill = el('i');
    bar.append(fill);
    box.append(sym, bar);
    c.append(box, el('span', 'w', WEIGHTS[r][i]));
    grid.append(c);
    cells.push({c, sym, fill, s: r * 10 + i});
  }
  row.append(lab, grid);
  $('frame').append(row);
});
const slots = [];
for (let i = 0; i < 10; i++) { slots.push(el('span')); $('slots').append(slots[i]); }
const bucketCells = BUCKETS.map((label, i) => {
  const tr = el('tr', i === 4 ? 'mid' : ''), n = el('td'), share = el('td');
  tr.append(el('td', '', label), n, share);
  $('tmRows').append(tr);
  return {n, share};
});
ZONES.forEach(([tz, label]) => { const o = el('option', '', label); o.value = tz; $('tzSel').append(o); });
{ const o = el('option', '', 'Custom POSIX TZ…'); o.value = ''; $('tzSel').append(o); }
RESOLUTIONS.forEach(([bits]) => { const o = el('option', '', ''); o.value = String(bits); $('resSel').append(o); });

// --- live parts, every 100 ms of the device clock ---
function tick() {
  try { renderLive(); } catch (e) { console.error(e); }
  setTimeout(tick, 104 - (((devNow() % 100) + 100) % 100));
}

function renderLive() {
  const now = devNow();
  const z = fields(now);
  const running = !!(st && st.time_valid);
  const slot = Math.floor(z.ms / 100);

  if (running) {
    const key = z.year + '-' + z.yday + '-' + z.hour + ':' + z.min;
    if (key !== frameKey) {
      frameKey = key;
      frameBits = frame(z);
      cells.forEach(x => {
        const v = frameBits[x.s], f = fieldOf(x.s), w = WEIGHTS[Math.floor(x.s / 10)][x.s % 10];
        x.c.title = 'Second ' + x.s + ': ' + (v === MARK ? 'marker ' + MARKERS[x.s]
          : f ? f + (w && f.indexOf('parity') < 0 ? ', weight ' + w : '') + ' = ' + v : 'unused, always 0');
      });
    }
  }
  const v = running ? frameBits[z.sec] : 0;
  const on = running ? onSlots(v) : 0;
  const carrier = running && slot < on;

  $('frame').classList.toggle('idle', !running);
  cells.forEach(x => {
    const b = running ? frameBits[x.s] : 0, mk = b === MARK, full = onSlots(b), isNow = running && x.s === z.sec;
    x.c.className = 'cell' + (running ? (mk ? ' k-m' : b ? ' k-1' : ' k-0') : '') + (isNow ? ' now' : '') + (running && x.s > z.sec ? ' future' : '');
    x.sym.textContent = running ? (mk ? MARKERS[x.s] : String(b)) : '';
    x.fill.style.width = (running ? (isNow ? Math.min(slot + 1, full) : full) * 10 : 0) + '%';
  });
  slots.forEach((s, i) => {
    const lit = running && i < on, past = running && i <= slot;
    s.style.background = lit ? (past ? '#f4b13d' : '#806429') : (past ? '#3a4148' : '#232a30');
  });

  if (!st) {
    return;
  }
  $('zone').textContent = st.zone + ' · UTC' + (st.utc_offset < 0 ? '−' : '+') + Math.floor(Math.abs(st.utc_offset) / 3600)
    + (Math.abs(st.utc_offset) % 3600 ? ':' + p2(Math.floor(Math.abs(st.utc_offset) % 3600 / 60)) : '');
  $('clock').textContent = running ? hms(z) : '--:--:--';
  $('date').textContent = running ? DAYS[z.wday] + ' ' + z.mday + ' ' + MONTHS[z.mon] + ' ' + z.year : 'Clock not set yet';
  $('secNum').textContent = running ? p2(z.sec) : '--';
  $('secWhat').textContent = !running ? 'No time code' : v === MARK ? 'Marker ' + MARKERS[z.sec] : 'Bit ' + v + ' · ' + (fieldOf(z.sec) || 'unused');
  $('secCarrier').textContent = running ? 'Carrier on ' + on / 10 + ' s, then off' : 'Until the clock is set';
  $('tcCaption').textContent = running ? 'Minute ' + p2(z.hour) + ':' + p2(z.min) + ' ' + st.zone + ' · one symbol per second' : 'Not running until the clock is set';
  if (running) {
    $('decMin').textContent = p2(z.min);
    $('decHour').textContent = p2(z.hour);
    $('decDay').textContent = (z.yday + 1) + ' (' + z.mday + ' ' + MONTHS[z.mon].slice(0, 3) + ')';
    $('decYear').textContent = p2(z.year % 100) + ' (' + z.year + ')';
    $('decWday').textContent = z.wday + ' (' + DAYS[z.wday].slice(0, 3) + ')';
    $('decParity').textContent = frameBits[36] + ', ' + frameBits[37];
  }
  else {
    ['decMin', 'decHour', 'decDay', 'decYear', 'decWday', 'decParity'].forEach(id => { $(id).textContent = '-'; });
  }

  // the LED as the device shows it: blink rates from LedShow(), blue with the carrier
  const lost = isLost();
  const blink = (state, ms) => state === 3 || (state === 2 && Math.floor(ms / 200) % 2 === 1) || (state === 1 && Math.floor(ms / 1000) % 2 === 1);
  const ms = Date.now();
  const r = !lost && blink(st.led.r, ms), g = !lost && blink(st.led.g, ms), b = !lost && carrier;
  const colors = {'100': '#ff5a4f', '010': '#4fd37f', '001': '#4f9dff', '110': '#ffd04f', '101': '#c77dff', '011': '#4fe3e0', '111': '#f2f2f2'};
  const key = (r ? '1' : '0') + (g ? '1' : '0') + (b ? '1' : '0');
  const lit = st.led.enabled && colors[key];
  $('ledDot').style.background = lit || '#24292e';
  $('ledDot').style.boxShadow = lit ? '0 0 22px ' + lit + ', 0 0 6px ' + lit : '';
  $('ledR').style.background = r ? '#ff5a4f' : '';
  $('ledG').style.background = g ? '#4fd37f' : '';
  $('ledB').style.background = b ? '#4f9dff' : '';
  $('pill').classList.toggle('on', carrier && !lost);
}

// --- status, every second ---
const isLost = () => !lastOk || Date.now() - lastOk > 5000;

async function fetchTimeout(url, options, ms) {
  const ctl = new AbortController();
  const timer = setTimeout(() => ctl.abort(), ms);
  try {
    return await fetch(url, Object.assign({cache: 'no-store', signal: ctl.signal}, options || {}));
  }
  finally {
    clearTimeout(timer);
  }
}

async function pollStatus() {
  const t0 = performance.now();
  try {
    const res = await fetchTimeout('/api/status', null, 4000);
    if (!res.ok) {
      throw new Error('HTTP ' + res.status);
    }
    const s = await res.json();
    const t1 = performance.now();
    const epoch = s.epoch_ms + (t1 - t0) / 2;  // the device read its clock about halfway through the request
    if (!clockBase || Math.abs(epoch - devNow()) > 40) {
      clockBase = {epoch, perf: t1};
    }
    if (lastUptime !== null && s.uptime < lastUptime) {
      logNext = 0;  // the device restarted
      if (restarting) {
        toast('The simulator is back.');
      }
      restarting = 0;
      forgotten = false;
    }
    else if (restarting && Date.now() - restarting > 60000) {
      restarting = 0;  // it kept answering without restarting: stop saying it is restarting
    }
    lastUptime = s.uptime;
    st = s;
    lastOk = Date.now();
    renderStatus();
  }
  catch (e) {
    renderBanner();
  }
  setTimeout(pollStatus, 1000);
}

function renderBanner() {
  const lost = isLost();
  const silent = Math.round((Date.now() - lastOk) / 1000);
  let title = '', text = '', info = false;
  if (forgotten) {
    title = 'Wi-Fi settings erased';
    text = 'The simulator restarted into its setup hotspot. Connect to that network to give it Wi-Fi settings again.';
    info = true;
  }
  else if (restarting) {
    title = 'Restarting the simulator';
    text = 'This page reconnects on its own once it is back.';
    info = true;
  }
  else if (lost && lastOk) {
    title = 'No response from the simulator for ' + (silent < 60 ? silent + ' s' : Math.floor(silent / 60) + ' min');
    text = 'If its Wi-Fi dropped, it tries to reconnect every 60 seconds and restarts itself after 3 failed attempts. '
      + 'This page keeps trying and shows the last values it received.';
  }
  $('banner').hidden = !title;
  $('banner').classList.toggle('info', info);
  $('bannerTitle').textContent = title;
  $('bannerText').textContent = text;
  $('main').classList.toggle('stale', lost && !!lastOk);

  const expected = restarting || forgotten;  // a restart we asked for: not an error
  const pill = $('pill');
  pill.className = 'pill' + (expected ? '' : lost ? (lastOk ? ' bad' : '') : st && st.time_valid ? ' tx' : '');
  $('pillText').textContent = expected ? 'Restarting' : lost ? (lastOk ? 'No response' : 'Connecting')
    : st && st.time_valid ? 'Transmitting' : 'Waiting for time';
  renderControls();
}

// buttons and the form work only with a password set and the device answering
function renderControls() {
  const usable = !!st && st.auth && !isLost();
  ['btnSync', 'btnRestart', 'btnForget', 'ledSwitch'].forEach(id => { $(id).disabled = !usable; });
  renderForm();
}

function ago(seconds) {
  const s = Math.max(0, Math.round(seconds));
  return s < 60 ? s + ' s ago' : s < 3600 ? Math.floor(s / 60) + ' min ago' : Math.floor(s / 3600) + ' h ' + Math.floor(s % 3600 / 60) + ' min ago';
}

function renderStatus() {
  const s = st;
  const kHz = s.freq / 1000 + ' kHz';
  const nowSec = devNow() / 1000;
  renderBanner();
  $('host').textContent = s.host + '.local · ' + s.board;
  document.title = 'JJY Simulator · ' + s.host;

  $('sentence').textContent = !s.time_valid
    ? 'No time code yet because the clock has not been set. Waiting for the first reply from ' + s.ntp + '.'
    : 'Transmitting the JJY time code at ' + kHz + '. Last synced with ' + s.ntp + ' ' + ago(nowSec - s.last_sync) + '.';
  $('tcChip').textContent = kHz + ' · GPIO' + s.pin;

  // LED card
  const l = s.led;
  const R = [['Off', 'Wi-Fi connected'], ['Slow blink', 'Network not found'], ['Fast blink', 'Wi-Fi disconnected'], ['On', 'Wi-Fi changing']][l.r] || ['-', ''];
  const G = [['Off', 'Clock set'], ['Slow blink', 'Waiting for time'], ['Fast blink', ''], ['On', 'Setup portal']][l.g] || ['-', ''];
  const B = s.time_valid ? ['Pulsing', 'Sending time code'] : ['Off', 'No time code'];
  $('ledRState').textContent = R[0]; $('ledRText').textContent = R[1];
  $('ledGState').textContent = G[0]; $('ledGText').textContent = G[1];
  $('ledBState').textContent = B[0]; $('ledBText').textContent = B[1];
  $('ledHead').textContent = !l.enabled ? 'LED monitor is off' : l.r ? 'Red: Wi-Fi problem' : l.g ? 'Green, blinking slowly' : 'Blue, blinking with the time code';
  $('ledSub').textContent = !l.enabled ? 'The device LED stays dark; the states below still apply'
    : l.r ? 'Check the Wi-Fi access point' : l.g ? 'Waiting for the clock to be set' : 'Everything is working';
  if (!ledBusy) {
    $('ledSwitch').setAttribute('aria-checked', String(l.enabled));
  }

  // cards
  const w = s.wifi;
  $('wifiCode').textContent = w.status;
  $('wifiStatus').textContent = WIFI_TEXT[w.status] || w.status;
  $('wifiSsid').textContent = w.ssid || '-';
  $('wifiRssi').textContent = w.rssi ? '−' + Math.abs(w.rssi) + ' dBm · ' + (w.rssi >= -55 ? 'excellent' : w.rssi >= -67 ? 'good' : w.rssi >= -75 ? 'fair' : 'weak') : '-';
  $('wifiIp').textContent = w.ip;
  $('wifiRetries').textContent = w.retries + ' of ' + w.retry_max;

  $('timeSource').textContent = s.last_sync ? 'NTP' : 'None yet';
  $('timeServer').textContent = s.ntp;
  $('timeLast').textContent = s.last_sync ? hms(fields(s.last_sync * 1000)) + ' (' + ago(nowSec - s.last_sync) + ')' : 'Not yet';
  $('timeNext').textContent = s.last_sync ? 'In about ' + Math.max(1, Math.round((s.last_sync + s.sync_interval - nowSec) / 60)) + ' min' : 'Waiting for a reply';

  const up = s.uptime, days = Math.floor(up / 86400);
  $('devBoard').textContent = s.board;
  $('devPin').textContent = 'GPIO' + s.pin + ' · ' + kHz + (s.freq_actual && s.freq_actual !== s.freq ? ' (' + num(s.freq_actual) + ' Hz)' : '')
    + (s.res !== 8 ? ' · ' + s.res + '-bit' : '');
  $('devUptime').textContent = (days ? days + ' d ' : '') + p2(Math.floor(up % 86400 / 3600)) + ':' + p2(Math.floor(up % 3600 / 60)) + ':' + p2(up % 60);
  $('devReset').textContent = s.reset;
  $('devHeap').textContent = num(Math.round(s.heap / 1024)) + ' KB';
  $('forgetText').textContent = 'Forget ' + (w.ssid || 'this network') + '? The simulator restarts into its setup hotspot, '
    + 'and this page stops working until you connect it to Wi-Fi again.';

  // timing
  const t = s.stats;
  $('tmNone').hidden = t.n > 1;
  $('tmSome').hidden = !(t.n > 1);
  if (t.n > 1) {
    const total = t.b.reduce((a, c) => a + c, 0) || 1;
    $('tmShare').textContent = (t.b[4] / total * 100).toFixed(2) + '%';
    $('tmSdv').textContent = (t.sdv / 1000).toFixed(1) + ' µs';
    $('tmMin').textContent = '−' + (Math.abs(t.min) / 1e6).toFixed(2) + ' ms';
    $('tmMax').textContent = '+' + (t.max / 1e6).toFixed(2) + ' ms';
    $('tmN').textContent = num(t.n);
    bucketCells.forEach((c, i) => {
      const q = t.b[i] / total * 100;
      c.n.textContent = num(t.b[i]);
      c.share.textContent = !t.b[i] ? '0%' : q < 0.01 ? '<0.01%' : q.toFixed(2) + '%';
    });
  }

  // settings: show the device's values unless they are being edited
  if (!formDirty) {
    document.querySelectorAll('input[name=freq]').forEach(r => { r.checked = Number(r.value) === s.freq; });
    $('resSel').value = String(s.res);
    const preset = ZONES.some(z => z[0] === s.tz);
    $('tzSel').value = preset ? s.tz : '';
    $('tzCustom').hidden = preset;
    $('tzCustom').value = s.tz;
    $('ntp').value = s.ntp;
  }
  renderControls();
}

// --- settings form ---
function formValues() {
  const checked = document.querySelector('input[name=freq]:checked');
  const tz = $('tzSel').value || $('tzCustom').value.trim();
  return {freq: checked ? Number(checked.value) : 0, res: Number($('resSel').value), tz, ntp: $('ntp').value.trim()};
}

function renderForm() {
  const f = formValues();
  const tzOk = validTz(f.tz);
  const valid = (f.freq === 40000 || f.freq === 60000) && RESOLUTIONS.some(r => r[0] === f.res) && tzOk && NTP_CHARS.test(f.ntp);
  const changed = st && (f.freq !== st.freq || f.res !== st.res || f.tz !== st.tz || f.ntp !== st.ntp);
  $('tzValue').textContent = f.tz || '-';
  const freq = f.freq || 40000;
  RESOLUTIONS.forEach(([bits, label, note], i) => {
    const hz = ledcFreq(freq, bits);
    $('resSel').options[i].textContent = label + ' · ' + hz.toLocaleString('en-US', {minimumFractionDigits: 1, maximumFractionDigits: 1})
      + ' Hz' + (note ? ' (' + note + ')' : '');
  });
  $('resNote').textContent = freq === 40000
    ? '40 kHz comes out exact at every setting. This only matters at 60 kHz.'
    : 'If a clock won\'t sync at 60 kHz, try 2-bit or 1-bit: they bring the carrier within a fraction of a hertz of 60 kHz. '
      + 'The duty stays at 50%, so the signal strength is the same.';
  $('btnSave').disabled = !st || !st.auth || isLost() || !valid || !changed;
  $('setNote').textContent = !st ? '' : !st.auth ? 'Read-only: set web_password in the sketch to allow changes.'
    : !tzOk ? 'The time zone is not a POSIX TZ string the simulator can use, e.g. JST-9 or CET-1CEST,M3.5.0,M10.5.0/3.'
    : !valid ? 'Check the NTP server: letters, digits, dots and hyphens only.'
    : changed ? 'Saving asks for the device password, then the simulator restarts.' : 'No changes.';
}

function edited() {
  formDirty = true;
  renderForm();
}

$('setForm').addEventListener('input', edited);
$('tzSel').addEventListener('change', () => {
  const custom = $('tzSel').value === '';
  $('tzCustom').hidden = !custom;
  if (custom) {
    $('tzCustom').focus();
  }
  edited();
});

async function post(path, data) {
  const res = await fetchTimeout(path, {
    method: 'POST',
    headers: {'X-JJY': '1', 'Content-Type': 'application/x-www-form-urlencoded'},
    body: new URLSearchParams(data || {}).toString()
  }, 120000);  // long enough for the browser's password prompt
  let json = null;
  try { json = await res.json(); } catch (e) { json = null; }
  return {status: res.status, ok: res.ok && !!json && json.ok === true};
}

function failed(r, what) {
  toast(r.status === 401 ? 'The password is needed to ' + what + '.' : r.status === 403 ? 'Changes are not allowed.'
    : r.status === 400 ? 'The simulator did not accept those values.' : 'Could not ' + what + '. Try again.');
}

$('setForm').addEventListener('submit', async e => {
  e.preventDefault();
  if ($('btnSave').disabled) {
    return;
  }
  const f = formValues();
  $('btnSave').disabled = true;
  try {
    const r = await post('/api/settings', f);
    if (r.ok) {
      formDirty = false;
      restarting = Date.now();
      toast('Saved. The simulator restarts to use the new settings.');
    }
    else {
      failed(r, 'save');
    }
  }
  catch (err) {
    toast('Could not reach the simulator.');
  }
  renderForm();
});

async function action(path, data, what, done) {
  try {
    const r = await post(path, data);
    if (r.ok) {
      done();
    }
    else {
      failed(r, what);
    }
    return r.ok;
  }
  catch (err) {
    toast('Could not reach the simulator.');
    return false;
  }
}

$('btnSync').addEventListener('click', () => action('/api/sync', null, 'sync the time', () => toast('Asked ' + st.ntp + ' for the time.')));
$('btnRestart').addEventListener('click', () => action('/api/restart', null, 'restart', () => { restarting = Date.now(); toast('Restarting.'); }));
$('btnForget').addEventListener('click', () => { $('forgetBox').hidden = false; $('devButtons').hidden = true; $('btnForgetNo').focus(); });
$('btnForgetNo').addEventListener('click', () => { $('forgetBox').hidden = true; $('devButtons').hidden = false; $('btnForget').focus(); });
$('btnForgetYes').addEventListener('click', async () => {
  $('forgetBox').hidden = true;
  $('devButtons').hidden = false;
  await action('/api/forget-wifi', null, 'erase the Wi-Fi settings', () => { restarting = Date.now(); forgotten = true; renderBanner(); });
});
$('ledSwitch').addEventListener('click', async () => {
  const sw = $('ledSwitch');
  const on = sw.getAttribute('aria-checked') !== 'true';
  ledBusy = true;
  sw.setAttribute('aria-checked', String(on));
  const ok = await action('/api/led', {on: on ? '1' : '0'}, 'switch the LED monitor', () => { st.led.enabled = on; });
  if (!ok) {
    sw.setAttribute('aria-checked', String(!on));
  }
  ledBusy = false;
});

// --- serial log, every 2 seconds ---
async function pollLog() {
  try {
    const res = await fetchTimeout('/api/log?from=' + logNext, null, 4000);
    if (res.ok) {
      const first = Number(res.headers.get('X-Log-First'));
      const next = Number(res.headers.get('X-Log-Next'));
      const text = await res.text();
      if (first < logNext || logNext === 0) {
        logText = '';  // first read, or the device restarted
      }
      else if (first > logNext) {
        logText += '\n[...]\n';  // fell behind the kept output
      }
      logText = (logText + text).slice(-16000);
      logNext = next;
      const pre = $('log');
      const atEnd = pre.scrollTop + pre.clientHeight >= pre.scrollHeight - 24;
      pre.textContent = logText;
      if (atEnd) {
        pre.scrollTop = pre.scrollHeight;
      }
    }
  }
  catch (e) {
    // the status poll reports the lost connection
  }
  setTimeout(pollLog, 2000);
}

// --- toast ---
let toastTimer = 0;
function toast(text) {
  const t = $('toast');
  t.textContent = text;
  t.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => t.classList.remove('show'), 4000);
}

setInterval(renderBanner, 1000);
pollStatus();
pollLog();
tick();
</script>
</body>
</html>
)rawliteral";

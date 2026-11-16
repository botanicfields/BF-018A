// JJY Simulator web UI demo: a BF-018A Rev.5 device emulated in the browser
//
// index.html next to this file is the device's own page (BF-018ARev5/BF_WebUiPage.h, copied by
// tools/make_web_ui_demo.py). This script loads before the page's script and answers the page's /api/ requests
// the way BF_WebUi.cpp and BF-018ARev5.ino do, so the page runs without hardware: the time code, the serial log,
// the timing statistics, restarts and the settings all come from this emulation. Nothing is stored.
//
(() => {
'use strict';

const realFetch = window.fetch.bind(window);

// --- the emulated device (BF-018ARev5.ino defaults, ATOMS3 Lite on board package 3.x) ---
const HOST = 'jjy-sim';
const BOARD = 'ATOMS3 Lite';
const PIN = 5;                // GPIO5 on the ATOMS3 Lite
const IP = '192.168.1.42';
const SSID = 'home-network';
const SYNC_INTERVAL = 3600;   // s, the SNTP default
const RETRY_MAX = 3;
const LOG_SIZE = 4096;        // LogTee::m_size
const MARK = 255;
const DAYS = ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];
const MONTHS = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
const TZ_CHARS = /^[A-Za-z0-9<>+\-,./:]+$/;
const NTP_CHARS = /^[A-Za-z0-9.\-]+$/;

const dev = {
  settings: {freq: 40000, res: 8, tz: 'JST-9', ntp: 'pool.ntp.org'},
  saved: null,          // settings saved from the page, applied at the next boot
  auth: true,           // web_password set
  online: true,         // answering HTTP
  portal: false,        // Wi-Fi settings erased: waiting in the setup hotspot
  wifiDropUntil: 0,     // Date.now() until which Wi-Fi is down
  restarting: false,
  gen: 0,               // boot generation: timers of an earlier boot do nothing
  bootAt: 0,            // Date.now() at uptime 0
  reset: 'Power on',
  clockOffset: 0,       // device clock = Date.now() + clockOffset
  timeValid: false,
  lastSync: 0,          // s since the epoch, 0: not yet
  nextSync: 0,          // Date.now() of the next SNTP reply
  led: true,
  tz: null,             // parsed settings.tz
};

// --- POSIX TZ strings, as newlib reads them: offsets here are seconds east of UTC ---
function parseTz(s) {
  let i = 0;
  const name = () => {
    if (s[i] === '<') {
      const j = s.indexOf('>', i);
      if (j < 0) return null;
      const n = s.slice(i + 1, j);
      i = j + 1;
      return n;
    }
    const m = /^[A-Za-z]{3,}/.exec(s.slice(i));
    if (!m) return null;
    i += m[0].length;
    return m[0];
  };
  const offset = () => {
    const m = /^([+-]?)(\d{1,2})(?::(\d{1,2}))?(?::(\d{1,2}))?/.exec(s.slice(i));
    if (!m) return null;
    i += m[0].length;
    const v = Number(m[2]) * 3600 + Number(m[3] || 0) * 60 + Number(m[4] || 0);
    return m[1] === '-' ? v : -v;  // POSIX counts west of UTC as positive
  };
  const rule = () => {
    const m = /^(?:M(\d{1,2})\.(\d)\.(\d)|J(\d{1,3})|(\d{1,3}))(?:\/([+-]?)(\d{1,3})(?::(\d{1,2}))?(?::(\d{1,2}))?)?/.exec(s.slice(i));
    if (!m) return null;
    i += m[0].length;
    const time = m[7] === undefined ? 7200
      : (m[6] === '-' ? -1 : 1) * (Number(m[7]) * 3600 + Number(m[8] || 0) * 60 + Number(m[9] || 0));
    return m[1] ? {type: 'M', m: Number(m[1]), w: Number(m[2]), d: Number(m[3]), time}
      : m[4] ? {type: 'J', n: Number(m[4]), time} : {type: 'n', n: Number(m[5]), time};
  };
  const std = name();
  const stdOff = std === null ? null : offset();
  if (stdOff === null) return null;
  const tz = {std, stdOff};
  if (i >= s.length) return tz;
  const dst = name();
  if (dst === null) return tz;
  tz.dst = dst;
  tz.dstOff = stdOff + 3600;
  if (i < s.length && s[i] !== ',') {
    const o = offset();
    if (o !== null) tz.dstOff = o;
  }
  tz.start = {type: 'M', m: 3, w: 2, d: 0, time: 7200};
  tz.end = {type: 'M', m: 11, w: 1, d: 0, time: 7200};
  if (s[i] === ',') {
    i++;
    const a = rule();
    if (a && s[i] === ',') {
      i++;
      const b = rule();
      if (b) {
        tz.start = a;
        tz.end = b;
      }
    }
  }
  return tz;
}

function ruleDay(r, year) {  // local midnight of the rule's day, as if it were UTC
  if (r.type === 'M') {
    const first = new Date(Date.UTC(year, r.m - 1, 1)).getUTCDay();
    const len = new Date(Date.UTC(year, r.m, 0)).getUTCDate();
    let day = 1 + (r.d - first + 7) % 7 + (r.w - 1) * 7;
    while (day > len) day -= 7;
    return Date.UTC(year, r.m - 1, day);
  }
  const leap = (year % 4 === 0 && year % 100 !== 0) || year % 400 === 0;
  if (r.type === 'J') return Date.UTC(year, 0, 1) + (r.n - 1 + (leap && r.n >= 60 ? 1 : 0)) * 86400000;
  return Date.UTC(year, 0, 1) + r.n * 86400000;
}

function zoneAt(ms) {  // {off, name} at the device clock time ms
  const tz = dev.tz || {std: 'UTC', stdOff: 0};
  if (!tz.dst) return {off: tz.stdOff, name: tz.std};
  const year = new Date(ms + tz.stdOff * 1000).getUTCFullYear();
  const start = ruleDay(tz.start, year) + tz.start.time * 1000 - tz.stdOff * 1000;  // given in standard time
  const end = ruleDay(tz.end, year) + tz.end.time * 1000 - tz.dstOff * 1000;        // given in summer time
  const inDst = start < end ? ms >= start && ms < end : !(ms >= end && ms < start);
  return inDst ? {off: tz.dstOff, name: tz.dst} : {off: tz.stdOff, name: tz.std};
}

function local(ms) {
  const z = zoneAt(ms);
  const d = new Date(ms + z.off * 1000);
  const year = d.getUTCFullYear();
  return {
    year, mon: d.getUTCMonth(), mday: d.getUTCDate(), hour: d.getUTCHours(), min: d.getUTCMinutes(), sec: d.getUTCSeconds(),
    wday: d.getUTCDay(), yday: Math.round((Date.UTC(year, d.getUTCMonth(), d.getUTCDate()) - Date.UTC(year, 0, 1)) / 86400000),
    ms: ((ms % 1000) + 1000) % 1000, off: z.off, zone: z.name,
  };
}

const p2 = n => String(n).padStart(2, '0');
const longDate = z => DAYS[z.wday] + ' ' + MONTHS[z.mon] + ' ' + p2(z.mday) + ' ' + z.year + ' ' + p2(z.hour) + ':' + p2(z.min) + ':' + p2(z.sec);
const commaDate = z => DAYS[z.wday] + ', ' + MONTHS[z.mon] + ' ' + p2(z.mday) + ' ' + z.year + ' ' + p2(z.hour) + ':' + p2(z.min) + ':' + p2(z.sec);
const devNow = () => Date.now() + dev.clockOffset;

// --- the time code: TcoValue() ---
const bcd = a => (a % 10) + (Math.floor(a / 10) % 10) * 16 + (Math.floor(a / 100) % 10) * 256;
const parity = a => { let p = a; for (let i = 1; i < 8; i++) p += a >> i; return p % 2; };
function tcoValue(z) {
  const h = bcd(z.hour), m = bcd(z.min), y = bcd(z.year), d = bcd(z.yday + 1), w = z.wday;
  const bits = {
    1: m & 0x40, 2: m & 0x20, 3: m & 0x10, 5: m & 8, 6: m & 4, 7: m & 2, 8: m & 1,
    12: h & 0x20, 13: h & 0x10, 15: h & 8, 16: h & 4, 17: h & 2, 18: h & 1,
    22: d & 0x200, 23: d & 0x100, 25: d & 0x80, 26: d & 0x40, 27: d & 0x20, 28: d & 0x10,
    30: d & 8, 31: d & 4, 32: d & 2, 33: d & 1, 36: parity(h), 37: parity(m),
    41: y & 0x80, 42: y & 0x40, 43: y & 0x20, 44: y & 0x10, 45: y & 8, 46: y & 4, 47: y & 2, 48: y & 1,
    50: w & 4, 51: w & 2, 52: w & 1,
  };
  if ([0, 9, 19, 29, 39, 49, 59].includes(z.sec)) return MARK;
  return bits[z.sec] || 0;
}
const onSlots = v => v === MARK ? 2 : v ? 5 : 8;

// --- serial log: LogTee keeps the last 4096 bytes ---
let logKept = '', logTotal = 0;
function log(s) {
  logKept += s;
  logTotal += s.length;
  if (logKept.length > LOG_SIZE) logKept = logKept.slice(-LOG_SIZE);
}
function logRead(from) {
  const oldest = logTotal - logKept.length;
  if (!(from >= oldest && from <= logTotal)) from = oldest;
  return {first: from, text: logKept.slice(from - oldest)};
}

// --- timing statistics of the 100 ms slots, as TcoGen() keeps them ---
// Each slot's start is drawn from a spread like a busy ESP32's: almost all within ±0.05 ms, a few later or earlier,
// and now and then a 10 ms step when the 10 ms Ticker's phase crosses a slot boundary.
const BUCKET_P = [0, 0.0000025, 0.0000725, 0.001, 0.99785, 0.001, 0.0000725, 0.0000025, 0];
const BUCKET_MEAN = [0, -1e7, -2e6, -2e5, 0, 2e5, 2e6, 1e7, 0];
const BUCKET_SQ = [0, 1e14, 5.8e12, 5.8e10, 8.1e7, 5.8e10, 5.8e12, 1e14, 0];
let tk = null, tcoStats = null;
function statsReset() {
  tk = {count: 0, sum: 0, sq: 0, min: 0, max: 0, dist: new Array(9).fill(0)};
  tcoStats = {n: 0, b: new Array(9).fill(0), ave: 0, sdv: 0, min: 0, max: 0};
}
function gauss() {
  let u = 0, v = 0;
  while (u === 0) u = Math.random();
  while (v === 0) v = Math.random();
  return Math.sqrt(-2 * Math.log(u)) * Math.cos(2 * Math.PI * v);
}
function deviation() {  // ns
  let r = Math.random();
  for (let i = 0; i < 9; i++) {
    if (r < BUCKET_P[i] || i === 8) {
      const sign = i < 4 ? -1 : 1, u = Math.random();
      if (i === 4) return Math.max(-49999, Math.min(49999, Math.round(gauss() * 9000)));
      if (i === 3 || i === 5) return sign * Math.round(50000 + u * u * 449999);
      if (i === 2 || i === 6) return sign * Math.round(500000 + u * u * 4499999);
      return sign * Math.round(9900000 + u * 200000);
    }
    r -= BUCKET_P[i];
  }
  return 0;
}
function bucketOf(d) {
  return d < -50000000 ? 0 : d < -5000000 ? 1 : d < -500000 ? 2 : d < -50000 ? 3 : d < 50000 ? 4
    : d < 500000 ? 5 : d < 5000000 ? 6 : d < 50000000 ? 7 : 8;
}
function statsSlot() {
  if (tk.count++ === 0) return;  // the first slot has nothing to compare with
  const d = deviation();
  tk.max = Math.max(tk.max, d);
  tk.min = Math.min(tk.min, d);
  tk.sum += d;
  tk.sq += d * d;
  tk.dist[bucketOf(d)]++;
}
function statsSeed(slots) {  // statistics as if the device had already run this many slots
  statsReset();
  tk.count = slots;
  BUCKET_P.forEach((p, i) => {
    if (i !== 4) tk.dist[i] = Math.round(slots * p * (0.85 + 0.3 * Math.random()));
  });
  tk.dist[4] = slots - 1 - tk.dist.reduce((a, c) => a + c, 0);
  tk.dist.forEach((n, i) => {
    tk.sum += n * BUCKET_MEAN[i];
    tk.sq += n * BUCKET_SQ[i];
  });
  tk.min = tk.dist[1] ? -Math.round(9900000 + Math.random() * 200000) : tk.dist[2] ? -Math.round(500000 + Math.random() * 4e6) : -49000;
  tk.max = tk.dist[7] ? Math.round(9900000 + Math.random() * 200000) : tk.dist[6] ? Math.round(500000 + Math.random() * 4e6) : 49000;
}
function statsPublish() {
  const ave = tk.sum / tk.count;
  const sdv = Math.sqrt(Math.max(0, (tk.sq - tk.sum * tk.sum / tk.count) / tk.count));
  log(tk.dist.join(' ') + ' ');
  log('\nn= ' + tk.count + ', ave= ' + ave.toFixed(4) + '  sdv= ' + sdv.toFixed(4) + '  min= ' + tk.min + '  max= ' + tk.max + '\n');
  tcoStats = {n: tk.count, b: tk.dist.slice(), ave, sdv, min: tk.min, max: tk.max};
}

// --- TcoGen(): one call per 100 ms slot of the device clock, catching up after the tab slept ---
let lastSlot = null;
function tcoSlot(ms) {
  const z = local(ms);
  const slot = Math.floor(z.ms / 100);
  const v = tcoValue(z);
  if (slot === 0 && z.sec === 0) log('\n' + longDate(z) + '\n');
  if (slot === 2 && v === MARK) log(' ' + z.sec + ' ');
  if (slot === 5 && v !== 0 && v !== MARK) log('1');
  if (slot === 8 && v === 0) log('0');
  statsSlot();
  if (slot === 0 && z.sec === 0) statsPublish();
}
function runTco() {
  if (!dev.timeValid) {
    lastSlot = null;
    return;
  }
  const now = Math.floor(devNow() / 100);
  if (lastSlot === null || now < lastSlot || now - lastSlot > 12000) lastSlot = now - 1;
  for (let k = lastSlot + 1; k <= now; k++) tcoSlot(k * 100);
  lastSlot = now;
}

// --- NTP ---
function ntpBegin() {
  log('NtpBegin: config TZ time = ' + dev.settings.tz + '\n');
  log('NtpBegin: SNTP sync mode = 0 (0:IMMED 1:SMOOTH)\n');
  log('NtpBegin: SNTP sync interval = ' + SYNC_INTERVAL * 1000 + 'ms\n');
}
function ntpReply() {
  dev.clockOffset = 0;
  dev.timeValid = true;
  dev.lastSync = Math.floor(Date.now() / 1000);
  dev.nextSync = Date.now() + SYNC_INTERVAL * 1000;
  log('SNTP callback: status = 1 SNTP_SYNC_STATUS_COMPLETED   \n');
  log('SNTP sync: ' + commaDate(local(devNow())) + '\n');
}

// --- boot: setup() as far as Wi-Fi, then the rest once Wi-Fi is up ---
// what the LEDC timer really makes from the 80 MHz clock: a divider with 8 fractional bits, 2^res steps per cycle,
// and ledcChangeFrequency() reports it rounded down
function freqActual(f, res) {
  const steps = 2 ** res;
  return Math.floor(80e6 * 256 / (Math.round(80e6 * 256 / (f * steps)) * steps));
}
function later(gen, ms, fn) {
  setTimeout(() => { if (gen === dev.gen) fn(); }, ms);
}
function boot(reason, portal) {
  const gen = ++dev.gen;
  dev.online = false;
  dev.restarting = true;
  dev.portal = false;
  dev.wifiDropUntil = 0;
  dev.timeValid = false;
  dev.lastSync = 0;
  dev.nextSync = 0;
  dev.led = true;
  dev.reset = reason;
  dev.bootAt = Date.now();
  if (dev.saved) {
    dev.settings = dev.saved;
    dev.saved = null;
  }
  dev.tz = null;  // UTC until NtpBegin() sets the zone
  logKept = '';
  logTotal = 0;
  statsReset();
  later(gen, 3200, () => {  // after the 3 s delay in setup()
    log('\n');  // Log.println()
    log('settings: ' + dev.settings.freq + ' Hz, ' + dev.settings.res + ' bits, ' + dev.settings.tz + ', ' + dev.settings.ntp + '\n');
    log('FastLED add LEDs for ' + BOARD + '\n');
    log('set the localtime temporarily until NTP replies\n');
    dev.clockOffset = Date.UTC(2017, 0, 1) - Date.now();
    log('localtime: ' + commaDate(local(devNow())) + '\n');
    if (portal) {
      dev.portal = true;  // the setup hotspot runs until it is given Wi-Fi settings
      dev.restarting = false;
    }
    else {
      later(gen, 1800, () => wifiUp(gen));
    }
  });
}
function wifiUp(gen) {
  dev.portal = false;
  dev.restarting = false;
  dev.tz = parseTz(dev.settings.tz);
  ntpBegin();
  log('ledcAttach result= 1\n');
  log('ledcWrite result= 1\n');
  log('pin= ' + PIN + ', resolution= ' + dev.settings.res + ' bits, duty= 0, freq= ' + freqActual(dev.settings.freq, dev.settings.res) + '\n');
  log('[Web]http://' + HOST + '.local/ or http://' + IP + '/ (' + (dev.auth ? 'changes need the password' : 'read-only: no password set') + ')\n');
  dev.online = true;
  later(gen, 150, () => log('[WiFi]WL_CONNECTED\n'));
  later(gen, 700 + Math.random() * 600, ntpReply);
}
function restart(portal) {
  dev.restarting = true;
  later(dev.gen, 1000, () => boot('Software restart', portal));  // after the response reached the browser
}

// --- Wi-Fi drop, for the demo bar ---
function wifiDrop(seconds) {
  const gen = dev.gen;
  dev.wifiDropUntil = Date.now() + seconds * 1000;
  log('[WiFi]WL_DISCONNECTED\n');
  later(gen, Math.min(6000, seconds * 400), () => log('[WiFi]reconnect 1\n'));
  later(gen, seconds * 1000, () => {
    dev.wifiDropUntil = 0;
    log('[WiFi]WL_CONNECTED\n');
  });
}
const answering = () => dev.online && !dev.portal && Date.now() >= dev.wifiDropUntil;

// --- the HTTP API: BF_WebUi.cpp ---
function json(status, obj) {
  return new Response(JSON.stringify(obj), {status, headers: {'Content-Type': 'application/json', 'Cache-Control': 'no-store'}});
}

function status() {
  const ms = devNow();
  const z = local(ms);
  const carrier = dev.timeValid && Math.floor(z.ms / 100) < onSlots(tcoValue(z));
  return {
    auth: dev.auth, host: HOST, board: BOARD, uptime: Math.floor((Date.now() - dev.bootAt) / 1000), reset: dev.reset,
    heap: 231000 + Math.round(Math.random() * 3000),
    epoch_ms: Math.floor(ms), utc_offset: z.off, zone: z.zone,
    time_valid: dev.timeValid, last_sync: dev.lastSync, sync_interval: SYNC_INTERVAL,
    tz: dev.settings.tz, ntp: dev.settings.ntp, freq: dev.settings.freq,
    freq_actual: freqActual(dev.settings.freq, dev.settings.res), res: dev.settings.res, pin: PIN,
    wifi: {status: 'WL_CONNECTED', ssid: SSID, rssi: -56 - Math.round(Math.random() * 5), ip: IP, retries: 0, retry_max: RETRY_MAX},
    led: {enabled: dev.led, r: 0, g: dev.timeValid ? 0 : 1, b: carrier ? 3 : 0},
    stats: tcoStats,
  };
}

function validText(s, chars) {
  return s.length > 0 && s.length < 64 && chars.test(s);
}

function route(method, path, query, headers, body) {
  if (method === 'GET' && path === '/api/status') return json(200, status());
  if (method === 'GET' && path === '/api/log') {
    const r = logRead(Number(query.get('from')) || 0);
    return new Response(r.text, {status: 200, headers: {
      'Content-Type': 'text/plain; charset=utf-8', 'Cache-Control': 'no-store',
      'X-Log-First': String(r.first), 'X-Log-Next': String(r.first + r.text.length),
    }});
  }
  if (method !== 'POST' || !['/api/settings', '/api/led', '/api/sync', '/api/restart', '/api/forget-wifi'].includes(path)) {
    return new Response('Not found\n', {status: 404, headers: {'Content-Type': 'text/plain'}});
  }
  // Allowed(): a password is set and the page sent its header. The demo asks no password.
  if (!dev.auth) return json(403, {ok: false, error: 'read-only'});
  if (!headers.has('X-JJY')) return json(403, {ok: false, error: 'forbidden'});
  const args = new URLSearchParams(body || '');
  if (path === '/api/settings') {
    const freq = parseInt(args.get('freq'), 10);
    const res = parseInt(args.get('res'), 10);
    const tz = (args.get('tz') || '').trim();
    const ntp = (args.get('ntp') || '').trim();
    // validTz() is the page's own copy of the device's POSIX TZ check (WebUiValidTimeZone())
    if ((freq !== 40000 && freq !== 60000) || ![8, 2, 1].includes(res) || !validText(tz, TZ_CHARS) || !validTz(tz)
        || !validText(ntp, NTP_CHARS)) {
      return json(400, {ok: false, error: 'invalid'});
    }
    dev.saved = {freq, res, tz, ntp};
    log('[Web]settings saved: ' + freq + ' Hz, ' + res + ' bits, ' + tz + ', ' + ntp + ': restarting..\n');
    restart(false);
    return json(200, {ok: true, restart: true});
  }
  if (path === '/api/led') {
    dev.led = args.get('on') === '1';
    log('[Web]LED monitor ' + (dev.led ? 'on' : 'off') + '\n');
    return json(200, {ok: true});
  }
  if (path === '/api/sync') {
    log('[Web]time sync requested\n');
    ntpBegin();
    later(dev.gen, 600 + Math.random() * 500, ntpReply);
    return json(200, {ok: true});
  }
  if (path === '/api/restart') {
    log('[Web]restart requested: restarting..\n');
    restart(false);
    return json(200, {ok: true, restart: true});
  }
  log('[Web]erase SSID/Key requested: restarting..\n');
  restart(true);
  return json(200, {ok: true, restart: true});
}

const abortError = () => new DOMException('The operation was aborted.', 'AbortError');

window.fetch = function (input, init) {
  const url = new URL(input instanceof Request ? input.url : String(input), location.href);
  if (!url.pathname.startsWith('/api/')) return realFetch(input, init);
  init = init || {};
  const method = (init.method || 'GET').toUpperCase();
  const headers = new Headers(init.headers || {});
  const signal = init.signal;
  return new Promise((resolve, reject) => {
    if (signal) {
      if (signal.aborted) return reject(abortError());
      signal.addEventListener('abort', () => reject(abortError()));
    }
    // a device that is not answering leaves the request hanging until the page gives up
    const hang = () => { if (!signal) setTimeout(() => reject(new TypeError('Failed to fetch')), 4000); };
    if (!answering()) return hang();
    setTimeout(() => {
      if (signal && signal.aborted) return;
      if (!answering()) return hang();
      runTco();
      resolve(route(method, url.pathname, url.searchParams, headers, init.body));
    }, 25 + Math.random() * 45);
  });
};

// --- start: a device that has been up for a couple of days, synced 21 minutes ago ---
(function start() {
  const now = Date.now();
  const uptime = 2 * 86400 + 5 * 3600 + 13 * 60 + 27;
  const replay = 16 * 60;  // seconds of time code replayed into the log
  dev.bootAt = now - uptime * 1000;
  dev.tz = parseTz(dev.settings.tz);
  dev.timeValid = true;
  dev.lastSync = Math.floor(now / 1000) - 1260;
  dev.nextSync = (dev.lastSync + SYNC_INTERVAL) * 1000;
  statsSeed((uptime - replay) * 10);
  logTotal = 1162400;
  lastSlot = Math.floor((now - replay * 1000) / 100);
  runTco();
})();

setInterval(() => {
  runTco();
  if (dev.timeValid && dev.lastSync && Date.now() >= dev.nextSync) ntpReply();  // the hourly SNTP sync
}, 50);

// --- the demo bar ---
const style = document.createElement('style');
style.textContent = `
.demo-bar{background:#1f1a10;border-bottom:1px solid #4a3a1c;color:var(--t2);font-size:13.5px;line-height:1.45}
.demo-in{display:flex;flex-wrap:wrap;align-items:center;gap:8px 16px;padding-top:10px;padding-bottom:10px}
.demo-tag{flex:none;font:600 11px/1 var(--mono);letter-spacing:.08em;text-transform:uppercase;color:var(--ink);background:var(--amber);border-radius:4px;padding:4px 6px}
.demo-text{flex:1 1 380px;min-width:0}
.demo-text a{color:var(--amber)}
.demo-actions{display:flex;flex-wrap:wrap;align-items:center;gap:4px 12px}
.demo-actions .btn{min-height:36px;padding:0 12px;font-size:13px}
.demo-actions .switch{min-height:36px}`;
document.head.append(style);

const bar = document.createElement('div');
bar.className = 'demo-bar';
bar.setAttribute('role', 'note');
bar.setAttribute('aria-label', 'About this demo');
bar.innerHTML = `<div class="wrap demo-in">
<span class="demo-tag">Demo</span>
<span class="demo-text">The Rev.5 web UI running in your browser with a simulated device: nothing here reaches real hardware,
nothing is saved, and no password is asked. <a href="../">Back to the article</a> · <a href="https://github.com/tunlezah/JJYSim">JJYSim on GitHub</a></span>
<span class="demo-actions">
<button id="demoWifi" type="button" class="btn">Simulate a Wi-Fi drop</button>
<button id="demoAuth" type="button" role="switch" aria-checked="true" class="switch"><span>Password set</span><span class="track" aria-hidden="true"><span class="knob"></span></span></button>
</span></div>`;
document.body.prepend(bar);

const wifiBtn = document.getElementById('demoWifi');
const authBtn = document.getElementById('demoAuth');
wifiBtn.addEventListener('click', () => {
  if (dev.portal) {
    wifiUp(dev.gen);  // given Wi-Fi settings in the setup hotspot: setup() carries on
  }
  else if (answering()) {
    wifiDrop(15);
  }
  renderBar();
});
authBtn.addEventListener('click', () => {
  dev.auth = !dev.auth;
  renderBar();
});
function renderBar() {
  const dropLeft = Math.ceil((dev.wifiDropUntil - Date.now()) / 1000);
  wifiBtn.disabled = !dev.portal && !answering();
  wifiBtn.classList.toggle('primary', dev.portal);
  wifiBtn.textContent = dev.portal ? 'Give it Wi-Fi settings again' : dropLeft > 0 ? 'Wi-Fi back in ' + dropLeft + ' s'
    : dev.restarting || !dev.online ? 'Restarting…' : 'Simulate a Wi-Fi drop';
  authBtn.setAttribute('aria-checked', String(dev.auth));
  authBtn.title = dev.auth ? 'Changes allowed, as with web_password set' : 'Read-only, as with web_password empty';
}
setInterval(renderBar, 250);
renderBar();
})();

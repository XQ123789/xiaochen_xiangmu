'use strict';
/* =====================================================================
 * GPS 实时定位监控台（网页端）
 * 数据源：1) 演示模拟器  2) OneNET MQTT(WebSocket)  3) 自定义 MQTT
 * 物模型格式：{"id":"123","params":{"temp":{"value":25},...}}
 *           兼容普通 JSON：{temp,humi,lat,lon,alt,speed,dir,ts}
 * 坐标系：GCJ-02（与固件 wgs84→gcj02 转换后的上报一致），配高德瓦片无偏移
 * ===================================================================== */

/* ---------- 全局状态 ---------- */
const S = {
  mode: 'idle',              // idle | demo | mqtt
  client: null,              // mqtt 客户端
  demoTimer: null,           // 演示定时器
  records: [],               // 全部数据记录（上限 5000）
  trail: [],                 // 轨迹点 [lat,lng]
  marker: null,
  startMarker: null,
  trailLine: null,
  chart: null,
  msgCount: 0,
  nav: null,                 // 导航目标 {lat, lng}
  navMarker: null,
  navLine: null,
  pickNav: false,            // 地图设目标模式
  cfg: { mode: 'onenet', url: 'wss://mqtts.heclouds.com:443/mqtt',
         pid: '', dev: '', key: '', token: '', days: 365,
         sub: 'gps/+/data', pub: 'gps/web/data', remember: false, lock: '' },
};
const MAX_RECORDS = 5000, MAX_CHART_POINTS = 600, MAX_TABLE_ROWS = 100, MAX_LOG_LINES = 300;

/* ---------- 工具 ---------- */
const $ = id => document.getElementById(id);
const fmt = (v, d = 1) => (v == null || isNaN(v)) ? '--' : Number(v).toFixed(d);
function nowStr() {
  const d = new Date();
  const p = n => String(n).padStart(2, '0');
  return `${p(d.getHours())}:${p(d.getMinutes())}:${p(d.getSeconds())}`;
}

/* =====================================================================
 * OneNET 安全 token（version 2018-10-31, md5）
 * 与项目 onenet_token.py 输出完全一致（已对照验证）
 * ===================================================================== */
function md5Bytes(bytes) {
  const s = [7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
             5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
             4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
             6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21];
  const K = new Int32Array(64);
  for (let i = 0; i < 64; i++) K[i] = Math.floor(Math.abs(Math.sin(i + 1)) * 4294967296);
  const rotl = (x, c) => (x << c) | (x >>> (32 - c));
  const len = bytes.length;
  const buf = new Uint8Array((((len + 8) >> 6) + 1) * 64);
  buf.set(bytes);
  buf[len] = 0x80;
  const dv = new DataView(buf.buffer);
  const bitLen = len * 8;
  dv.setUint32(buf.length - 8, bitLen >>> 0, true);
  dv.setUint32(buf.length - 4, Math.floor(bitLen / 4294967296), true);
  let a0 = 0x67452301, b0 = 0xefcdab89, c0 = 0x98badcfe, d0 = 0x10325476;
  const M = new Int32Array(16);
  for (let off = 0; off < buf.length; off += 64) {
    for (let i = 0; i < 16; i++) M[i] = dv.getInt32(off + i * 4, true);
    let A = a0, B = b0, C = c0, D = d0;
    for (let i = 0; i < 64; i++) {
      let F, g;
      if (i < 16)      { F = (B & C) | (~B & D); g = i; }
      else if (i < 32) { F = (D & B) | (~D & C); g = (5 * i + 1) % 16; }
      else if (i < 48) { F = B ^ C ^ D;          g = (3 * i + 5) % 16; }
      else             { F = C ^ (B | ~D);       g = (7 * i) % 16; }
      F = (F + A + K[i] + M[g]) | 0;
      A = D; D = C; C = B;
      B = (B + rotl(F, s[i])) | 0;
    }
    a0 = (a0 + A) | 0; b0 = (b0 + B) | 0; c0 = (c0 + C) | 0; d0 = (d0 + D) | 0;
  }
  const out = new Uint8Array(16), odv = new DataView(out.buffer);
  odv.setInt32(0, a0, true); odv.setInt32(4, b0, true);
  odv.setInt32(8, c0, true); odv.setInt32(12, d0, true);
  return out;
}
function hmacMd5(keyBytes, msgBytes) {
  const B = 64;
  let k = keyBytes.length > B ? md5Bytes(keyBytes) : keyBytes;
  const inner = new Uint8Array(B + msgBytes.length);
  const outer = new Uint8Array(B + 16);
  for (let i = 0; i < B; i++) { inner[i] = (k[i] || 0) ^ 0x36; outer[i] = (k[i] || 0) ^ 0x5c; }
  inner.set(msgBytes, B);
  outer.set(md5Bytes(inner), B);
  return md5Bytes(outer);
}
function b64FromBytes(b) { let s = ''; for (const x of b) s += String.fromCharCode(x); return btoa(s); }
function bytesFromB64(b64) { const s = atob(b64.trim()); return Uint8Array.from(s, c => c.charCodeAt(0)); }
/* 与 python quote(x, safe="") 一致：仅字母数字 - . _ ~ 不转义，hex 大写 */
function pyQuote(str) {
  let out = '';
  for (const b of new TextEncoder().encode(str)) {
    const ok = (b >= 0x41 && b <= 0x5a) || (b >= 0x61 && b <= 0x7a) ||
               (b >= 0x30 && b <= 0x39) || b === 0x2d || b === 0x2e || b === 0x5f || b === 0x7e;
    out += ok ? String.fromCharCode(b) : '%' + b.toString(16).toUpperCase().padStart(2, '0');
  }
  return out;
}
function genOnenetToken(pid, dev, accessKeyB64, days) {
  const version = '2018-10-31', method = 'md5';
  const res = `products/${pid}/devices/${dev}`;
  const et = String(Math.floor(Date.now() / 1000) + days * 86400);
  const msg = `${et}\n${method}\n${res}\n${version}`;
  const sign = pyQuote(b64FromBytes(hmacMd5(bytesFromB64(accessKeyB64), new TextEncoder().encode(msg))));
  return `version=${version}&res=${pyQuote(res)}&et=${et}&method=${method}&sign=${sign}`;
}

/* =====================================================================
 * 数据解析：OneNET 物模型 / 普通 JSON
 * ===================================================================== */
const FIELD_ALIAS = {
  temp: ['temp', 'temperature'], humi: ['humi', 'humidity'],
  lat: ['lat', 'latitude'], lon: ['lon', 'lng', 'longitude'],
  alt: ['alt', 'altitude'], speed: ['speed'],
  dir: ['dir', 'direction', 'heading', 'course'],
};
function num(v) { const n = parseFloat(v); return isNaN(n) ? null : n; }
function parseTs(v) {
  if (v == null) return Date.now();
  if (typeof v === 'number') return v < 1e12 ? v * 1000 : v;   // 秒/毫秒时间戳自适应
  const n = Number(v);
  if (!isNaN(n)) return n < 1e12 ? n * 1000 : n;
  const t = Date.parse(v);
  return isNaN(t) ? Date.now() : t;
}
/* 返回记录对象或 null（无法识别） */
function parsePayload(str) {
  let obj;
  try { obj = JSON.parse(str); } catch { return null; }
  if (obj == null || typeof obj !== 'object') return null;

  const rec = { ts: parseTs(obj.ts ?? obj.time ?? obj.timestamp), raw: str };
  let hit = false;

  if (obj.params && typeof obj.params === 'object') {
    // OneNET 物模型：{"id":"123","params":{"temp":{"value":25},...}}
    for (const [key, aliases] of Object.entries(FIELD_ALIAS)) {
      for (const a of aliases) {
        const p = obj.params[a];
        if (p != null && typeof p === 'object' && p.value !== undefined) { rec[key] = num(p.value); hit = true; break; }
        if (p != null && typeof p !== 'object') { rec[key] = num(p); hit = true; break; }
      }
    }
  } else {
    for (const [key, aliases] of Object.entries(FIELD_ALIAS)) {
      for (const a of aliases) {
        if (obj[a] !== undefined) { rec[key] = num(obj[a]); hit = true; break; }
      }
    }
  }
  return hit ? rec : null;
}

/* =====================================================================
 * 串口帧解析（树莓派 ↔ STM32 链路，可经自定义 MQTT 主题转发上来）
 *   GPS,纬度,经度*校验和   —— receive.py 接收的定位帧（纬度在前）
 *   NAV,经度,纬度          —— send.py 下发的导航目标（经度在前）
 * ===================================================================== */
function parseSerialFrame(str) {
  const s = String(str).trim();
  let m = /^GPS\s*,\s*(-?\d+(?:\.\d+)?)\s*,\s*(-?\d+(?:\.\d+)?)\s*(?:\*[^*\s]*)?$/i.exec(s);
  if (m) return { kind: 'record', rec: { ts: Date.now(), lat: +m[1], lon: +m[2], raw: s } };
  m = /^NAV\s*,\s*(-?\d+(?:\.\d+)?)\s*,\s*(-?\d+(?:\.\d+)?)\s*$/i.exec(s);
  if (m) return { kind: 'nav', lng: +m[1], lat: +m[2], raw: s };
  return null;
}

/* 消息统一分类：JSON 数据 / JSON 应答 / 串口帧 / 无法识别 */
function classifyMessage(str) {
  const rec = parsePayload(str);
  if (rec) return { kind: 'record', rec };
  try {
    const o = JSON.parse(str);
    if (o && typeof o === 'object' && o.id !== undefined && o.code !== undefined) {
      return { kind: 'reply', reply: o };
    }
  } catch { /* 非 JSON，继续尝试串口帧 */ }
  const frame = parseSerialFrame(str);
  if (frame) return frame;
  return { kind: 'unknown' };
}

/* =====================================================================
 * 界面渲染
 * ===================================================================== */
const DIR_NAMES = ['北','东北偏北','东北','东北偏东','东','东南偏东','东南','东南偏南','南','西南偏南','西南','西南偏西','西','西北偏西','西北','西北偏北'];
function dirName(d) { return d == null ? '--' : DIR_NAMES[Math.round(((d % 360) + 360) % 360 / 22.5) % 16]; }

function renderRecord(r) {
  $('vTemp').textContent = fmt(r.temp, 1);
  $('vHumi').textContent = fmt(r.humi, 0);
  $('vSpeed').textContent = fmt(r.speed, 2);
  $('vAlt').textContent = fmt(r.alt, 1);
  $('vDir').textContent = r.dir == null ? '--' : `${fmt(r.dir, 0)}° ${dirName(r.dir)}`;
  if (r.dir != null) $('compass').style.transform = `rotate(${r.dir}deg)`;
  if (r.lat != null && r.lon != null) {
    $('vCoord').textContent = `${r.lat.toFixed(6)}, ${r.lon.toFixed(6)}`;
  }
  $('vCount').textContent = S.msgCount;
  $('statPill').textContent = `消息 ${S.msgCount} · 更新 ${nowStr()}`;
}

function addTableRow(r) {
  const tb = $('recBody');
  const empty = $('recEmpty');
  if (empty) empty.remove();
  const tr = document.createElement('tr');
  const cells = [
    new Date(r.ts).toLocaleTimeString('zh-CN', { hour12: false }),
    fmt(r.lat, 6), fmt(r.lon, 6), fmt(r.temp, 1), fmt(r.humi, 0),
    fmt(r.speed, 2), fmt(r.alt, 1), r.dir == null ? '--' : fmt(r.dir, 0) + '°',
  ];
  for (const c of cells) {
    const td = document.createElement('td');
    td.textContent = c;
    tr.appendChild(td);
  }
  tb.insertBefore(tr, tb.firstChild);
  while (tb.rows.length > MAX_TABLE_ROWS) tb.deleteRow(-1);
}

function pushChart(r) {
  if (!S.chart) return;
  const opt = S.chart.getOption();
  if (opt.title && opt.title.length && opt.title[0].text === '暂无数据') {
    S.chart.setOption({ title: { show: false } });
  }
  const t = new Date(r.ts);
  const push = (arr, v) => { if (v != null) { arr.push([t, v]); if (arr.length > MAX_CHART_POINTS) arr.shift(); } };
  push(opt.series[0].data, r.temp);
  push(opt.series[1].data, r.humi);
  push(opt.series[2].data, r.speed);
  S.chart.setOption({ series: opt.series });
}

/* ---------- 日志 ---------- */
function log(tag, msg) {
  const box = $('log');
  const line = document.createElement('div');
  line.className = 'log-line';
  const t = document.createElement('span'); t.className = 'log-time'; t.textContent = nowStr();
  const g = document.createElement('span'); g.className = 'log-tag ' + tag; g.textContent = tag.toUpperCase();
  const m = document.createElement('span'); m.className = 'log-msg'; m.textContent = msg;
  line.append(t, g, m);
  box.insertBefore(line, box.firstChild);
  while (box.children.length > MAX_LOG_LINES) box.removeChild(box.lastChild);
}

/* =====================================================================
 * 地图（Leaflet + 高德 GCJ-02 瓦片，无需 key）
 * ===================================================================== */
let map = null;
function initMap() {
  map = L.map('map', { center: [35.0, 108.0], zoom: 5, zoomControl: true, attributionControl: false });
  L.tileLayer('https://weprd{s}.is.autonavi.com/appmaptile?lang=zh_cn&size=1&style=7&x={x}&y={y}&z={z}', {
    subdomains: '1234', maxZoom: 18,
  }).addTo(map);
  L.control.attribution({ prefix: false }).addAttribution('© 高德地图').addTo(map);
  map.on('mousemove', e => { $('mapCoord').textContent = `${e.latlng.lat.toFixed(5)}, ${e.latlng.lng.toFixed(5)}`; });
}
/* 弹窗内容全部用 DOM + textContent 构建（Leaflet 按内联 HTML 渲染弹窗，禁止拼字符串） */
function buildPopup(r) {
  const div = document.createElement('div');
  const line = (label, val) => {
    const p = document.createElement('div');
    const b = document.createElement('b'); b.textContent = label;
    p.append(b, document.createTextNode(val));
    div.appendChild(p);
  };
  line('位置 ', `${r.lat.toFixed(5)}, ${r.lon.toFixed(5)}`);
  line('速度 ', `${fmt(r.speed, 2)} m/s`);
  line('时间 ', nowStr());
  return div;
}

function updateMap(r) {
  if (!map || r.lat == null || r.lon == null) return;
  const ll = [r.lat, r.lon];
  if (!S.marker) {
    const icon = L.divIcon({
      className: '',
      html: '<div class="marker-arrow"><svg viewBox="0 0 34 34"><circle cx="17" cy="17" r="16" fill="rgba(53,194,255,.18)" stroke="#35c2ff" stroke-width="1.5"/><g id="arrowG"><path d="M17 5 L22 26 L17 22 L12 26 Z" fill="#35c2ff"/></g></svg></div>',
      iconSize: [34, 34], iconAnchor: [17, 17],
    });
    S.marker = L.marker(ll, { icon }).addTo(map);
    S.marker.bindPopup(buildPopup(r));
    S.startMarker = L.circleMarker(ll, { radius: 6, color: '#3ddc97', fillColor: '#3ddc97', fillOpacity: .9 }).addTo(map)
      .bindPopup('轨迹起点');
    S.trailLine = L.polyline([ll], { color: '#35c2ff', weight: 3, opacity: .85 }).addTo(map);
    map.setView(ll, 15);
  }
  S.marker.setLatLng(ll);
  const g = S.marker.getElement() && S.marker.getElement().querySelector('#arrowG');
  if (g && r.dir != null) g.setAttribute('transform', `rotate(${r.dir} 17 17)`);
  S.marker.setPopupContent(buildPopup(r));
  S.trail.push(ll);
  if (S.trail.length > MAX_RECORDS) S.trail.shift();
  S.trailLine.setLatLngs(S.trail);
  if ($('chkFollow').checked) map.setView(ll, map.getZoom(), { animate: true });
  updateNavLine();
}

/* =====================================================================
 * 导航目标（树莓派 send.py 的 "NAV,经度,纬度" 可视化）
 * ===================================================================== */
function haversineM(lat1, lon1, lat2, lon2) {
  const R = 6371000, rad = d => d * Math.PI / 180;
  const dLat = rad(lat2 - lat1), dLon = rad(lon2 - lon1);
  const a = Math.sin(dLat / 2) ** 2 + Math.cos(rad(lat1)) * Math.cos(rad(lat2)) * Math.sin(dLon / 2) ** 2;
  return 2 * R * Math.asin(Math.sqrt(a));
}
function bearingDeg(lat1, lon1, lat2, lon2) {
  const rad = d => d * Math.PI / 180, deg = r => r * 180 / Math.PI;
  const y = Math.sin(rad(lon2 - lon1)) * Math.cos(rad(lat2));
  const x = Math.cos(rad(lat1)) * Math.sin(rad(lat2)) -
            Math.sin(rad(lat1)) * Math.cos(rad(lat2)) * Math.cos(rad(lon2 - lon1));
  return (deg(Math.atan2(y, x)) + 360) % 360;
}
function updateNavLine() {
  if (!map) return;
  const cur = S.marker && S.marker.getLatLng();
  if (S.nav && cur) {
    const pts = [[cur.lat, cur.lng], [S.nav.lat, S.nav.lng]];
    if (!S.navLine) S.navLine = L.polyline(pts, { color: '#ffb454', weight: 2, dashArray: '6 8' }).addTo(map);
    else S.navLine.setLatLngs(pts);
  }
}
function setNavTarget(lat, lng, sourceLabel) {
  S.nav = { lat, lng };
  if (!map) return;
  const ll = [lat, lng];
  if (!S.navMarker) {
    const icon = L.divIcon({ className: '', html: '<div class="target-icon">🎯</div>', iconSize: [22, 22], iconAnchor: [11, 11] });
    S.navMarker = L.marker(ll, { icon }).addTo(map);
  } else S.navMarker.setLatLng(ll);
  $('navCard').style.display = '';
  $('navCmd').textContent = `NAV,${lng.toFixed(6)},${lat.toFixed(6)}`;
  updateNavLine();
  const cur = S.marker && S.marker.getLatLng();
  if (cur) {
    const dist = haversineM(cur.lat, cur.lng, lat, lng);
    const brg = bearingDeg(cur.lat, cur.lng, lat, lng);
    const distStr = dist >= 1000 ? (dist / 1000).toFixed(2) + ' km' : dist.toFixed(0) + ' m';
    $('vNavDist').textContent = `距离 ${distStr} · 方位 ${brg.toFixed(0)}° ${dirName(brg)}`;
  } else {
    $('vNavDist').textContent = '等待定位数据后计算距离与方位';
  }
  log('info', `[${sourceLabel}] 导航目标已设置：NAV,${lng.toFixed(6)},${lat.toFixed(6)}`);
}
function clearNavTarget() {
  S.nav = null;
  if (S.navMarker) { map.removeLayer(S.navMarker); S.navMarker = null; }
  if (S.navLine) { map.removeLayer(S.navLine); S.navLine = null; }
  $('navCard').style.display = 'none';
  log('info', '导航目标已清除');
}

/* =====================================================================
 * 图表（ECharts）
 * ===================================================================== */
function initChart() {
  S.chart = echarts.init($('chart'), 'dark');
  S.chart.setOption({
    backgroundColor: 'transparent',
    title: { text: '暂无数据', left: 'center', top: 'middle', textStyle: { color: '#8b9bb4', fontSize: 13, fontWeight: 'normal' } },
    grid: { left: 42, right: 42, top: 34, bottom: 24 },
    tooltip: { trigger: 'axis' },
    legend: { data: ['温度','湿度','速度'], textStyle: { color: '#8b9bb4', fontSize: 11 }, top: 2 },
    xAxis: { type: 'time', axisLabel: { formatter: '{HH}:{mm}:{ss}' } },
    yAxis: [
      { type: 'value', name: '°C / %', scale: true },
      { type: 'value', name: 'm/s', scale: true, splitLine: { show: false } },
    ],
    series: [
      { name: '温度', type: 'line', showSymbol: false, smooth: true, data: [], lineStyle: { width: 2 }, color: '#ffb454' },
      { name: '湿度', type: 'line', showSymbol: false, smooth: true, data: [], lineStyle: { width: 2 }, color: '#35c2ff' },
      { name: '速度', type: 'line', showSymbol: false, smooth: true, yAxisIndex: 1, data: [], lineStyle: { width: 2 }, color: '#3ddc97' },
    ],
  });
}

/* =====================================================================
 * 数据入口
 * ===================================================================== */
function ingest(r, sourceLabel) {
  S.msgCount++;
  S.records.push(r);
  if (S.records.length > MAX_RECORDS) S.records.shift();
  renderRecord(r);
  addTableRow(r);
  pushChart(r);
  updateMap(r);
  log('info', `[${sourceLabel}] ${r.raw.length > 120 ? r.raw.slice(0, 120) + '…' : r.raw}`);
}

/* =====================================================================
 * 演示模式（模拟车载运动 + DHT11）
 * ===================================================================== */
const demo = { lat: 39.90923, lon: 116.397428, dir: 45, speed: 8, temp: 24, humi: 55, alt: 46 };
function demoTick() {
  demo.dir = (demo.dir + (Math.random() - 0.5) * 40 + 360) % 360;
  demo.speed = Math.min(16, Math.max(0.5, demo.speed + (Math.random() - 0.5) * 3));
  const d = 3; // 秒，与固件 update_interval 一致
  const dy = demo.speed * d * Math.cos(demo.dir * Math.PI / 180);
  const dx = demo.speed * d * Math.sin(demo.dir * Math.PI / 180);
  demo.lat += dy / 111320;
  demo.lon += dx / (111320 * Math.cos(demo.lat * Math.PI / 180));
  demo.temp = Math.min(32, Math.max(16, demo.temp + (Math.random() - 0.5) * 0.4));
  demo.humi = Math.min(90, Math.max(30, demo.humi + (Math.random() - 0.5) * 1.5));
  demo.alt = Math.min(80, Math.max(20, demo.alt + (Math.random() - 0.5) * 0.8));
  const payload = {
    id: String(Date.now()),
    params: {
      temp: { value: Math.round(demo.temp) }, humi: { value: Math.round(demo.humi) },
      latitude: { value: +demo.lat.toFixed(6) }, longitude: { value: +demo.lon.toFixed(6) },
      altitude: { value: +demo.alt.toFixed(2) }, speed: { value: +demo.speed.toFixed(2) },
      direction: { value: +demo.dir.toFixed(1) },
    },
  };
  ingest(parsePayload(JSON.stringify(payload)), '演示');
}
function startDemo() {
  stopAll('演示模式');
  S.mode = 'demo';
  S.demoTimer = setInterval(demoTick, 3000);
  demoTick();
  setConn('demo', `演示模式运行中（每 3s 模拟一帧）`);
  log('ok', '演示模式已启动：模拟北京城区车载运动 + DHT11 数据，每 3 秒一帧');
}
function stopAll(nextLabel) {
  if (S.demoTimer) { clearInterval(S.demoTimer); S.demoTimer = null; }
  if (S.client) { try { S.client.end(true); } catch {} S.client = null; }
  S.mode = 'idle';
  setConn('off', nextLabel || '未连接');
}
function setConn(cls, text) {
  $('connDot').className = 'conn-dot' + (cls === 'off' ? '' : ' ' + cls);
  $('connText').textContent = text;
}

/* =====================================================================
 * MQTT 连接（OneNET / 自定义）
 * ===================================================================== */
function loadCfg() {
  try {
    const saved = JSON.parse(localStorage.getItem('gpsdash.cfg.v1') || '{}');
    Object.assign(S.cfg, saved);
    if (!S.cfg.remember) { S.cfg.key = ''; S.cfg.token = ''; }
  } catch {}
  $('fMode').value = S.cfg.mode; $('fUrl').value = S.cfg.url;
  $('fPid').value = S.cfg.pid; $('fDev').value = S.cfg.dev;
  $('fKey').value = S.cfg.key; $('fToken').value = S.cfg.token;
  $('fDays').value = S.cfg.days; $('fSub').value = S.cfg.sub; $('fPub').value = S.cfg.pub;
  $('fRemember').checked = !!S.cfg.remember;
  $('fLockAddr').value = S.cfg.lock || '';
  onModeChange();
}
function saveCfg() {
  S.cfg.mode = $('fMode').value; S.cfg.url = $('fUrl').value.trim();
  S.cfg.pid = $('fPid').value.trim(); S.cfg.dev = $('fDev').value.trim();
  S.cfg.key = $('fKey').value.trim(); S.cfg.token = $('fToken').value.trim();
  S.cfg.days = parseInt($('fDays').value, 10) || 365;
  S.cfg.sub = $('fSub').value.trim(); S.cfg.pub = $('fPub').value.trim();
  S.cfg.remember = $('fRemember').checked;
  S.cfg.lock = $('fLockAddr').value.trim();
  const toSave = { ...S.cfg };
  if (!S.cfg.remember) { toSave.key = ''; toSave.token = ''; }
  localStorage.setItem('gpsdash.cfg.v1', JSON.stringify(toSave));
}
function onModeChange() {
  const onenet = $('fMode').value === 'onenet';
  $('onenetFields').style.display = onenet ? '' : 'none';
  $('onenetKeyRow').style.display = onenet ? '' : 'none';
  $('daysRow').style.display = onenet ? '' : 'none';
  $('customFields').style.display = onenet ? 'none' : '';
}

const MAX_RECONNECT = 6;
function connect() {
  if (typeof mqtt === 'undefined') { alert('MQTT 库未加载，请检查网络'); return false; }
  saveCfg();
  const c = S.cfg;
  const onenet = c.mode === 'onenet';
  let url = c.url, clientId, username, password, subs = [];

  if (onenet) {
    if (!c.pid || !c.dev) { alert('请填写产品 ID 与设备名称'); return false; }
    if (c.token) password = c.token;
    else if (c.key) password = genOnenetToken(c.pid, c.dev, c.key, c.days);
    else { alert('请填写 AccessKey 或直接粘贴 token'); return false; }
    clientId = c.dev; username = c.pid;
    subs = [
      `$sys/${c.pid}/${c.dev}/thing/property/post/reply`,
      `$sys/${c.pid}/${c.dev}/thing/property/set`,
    ];
  } else {
    clientId = 'gpsdash-' + Math.random().toString(16).slice(2, 10);
    username = ''; password = '';
    subs = c.sub ? [c.sub] : [];
  }
  if (!/^wss?:\/\//.test(url)) { alert('WebSocket 地址须以 ws:// 或 wss:// 开头'); return false; }
  if (url.startsWith('ws://')) {
    const go = confirm('当前地址为明文 ws://，token 与定位数据将不加密传输，确定继续吗？');
    if (!go) { log('warn', '已取消明文 ws:// 连接'); return false; }
    log('warn', '正在使用明文 ws:// 连接，token 与数据不做加密');
  }

  stopAll('连接中…');
  S.mode = 'mqtt';
  log('info', `正在连接 ${url}（clientId: ${clientId}）`);

  const client = mqtt.connect(url, {
    clientId, username, password,
    keepalive: 60, clean: true, reconnectPeriod: 5000, connectTimeout: 10000,
  });
  S.client = client;

  let lastErr = '', reconTries = 0, connectedAt = 0, rapidKicks = 0;

  client.on('connect', () => {
    const justNow = connectedAt && Date.now() - connectedAt < 8000;
    if (justNow) rapidKicks++; else rapidKicks = 0;
    connectedAt = Date.now();
    reconTries = 0;
    setConn('on', `已连接 · ${onenet ? c.dev + '@OneNET' : url}`);
    log('ok', 'MQTT 已连接');
    if (rapidKicks >= 2 && onenet) {
      log('err', '连接反复被断开——疑似与真实设备使用了同一设备名（clientId 相同会互踢），请在 OneNET 单独建一个设备给网页用');
      setConn('err', '疑似设备名冲突互踢');
    }
    for (const t of subs) {
      client.subscribe(t, { qos: 1 }, err => log(err ? 'err' : 'ok', err ? `订阅失败 ${t}: ${err.message}` : `已订阅 ${t}`));
    }
  });
  client.on('message', (topic, payload) => {
    const str = payload.toString('utf8');
    const cls = classifyMessage(str);
    if (cls.kind === 'record') {
      log('info', `收到消息 ${topic}`);
      ingest(cls.rec, 'MQTT');
    } else if (cls.kind === 'nav') {
      log('info', `收到导航目标帧 ${topic}`);
      setNavTarget(cls.lat, cls.lng, 'MQTT');
    } else if (cls.kind === 'reply') {
      log(cls.reply.code === 200 ? 'ok' : 'warn',
        `上报应答 id=${cls.reply.id} code=${cls.reply.code}${cls.reply.msg != null ? ' msg=' + cls.reply.msg : ''}`);
    } else {
      log('warn', `收到无法识别的消息 ${topic}: ${str.slice(0, 80)}`);
    }
  });
  client.on('error', err => {
    lastErr = err.message;
    log('err', `MQTT 错误: ${err.message}`);
    setConn('err', `连接错误: ${err.message}`);
  });
  client.on('reconnect', () => {
    reconTries++;
    if (reconTries >= MAX_RECONNECT) {
      log('err', `连续 ${reconTries} 次重连失败，已停止自动重试（原因: ${lastErr || '未取得错误信息'}）。请检查地址/凭据后重新连接`);
      setConn('err', `连接失败: ${lastErr || '已停止重试'}`);
      try { client.end(true); } catch {}
    } else {
      setConn('demo', `重连中 ${reconTries}/${MAX_RECONNECT}…`);
      log('warn', `正在重连（第 ${reconTries} 次，原因: ${lastErr || '连接断开'}）`);
    }
  });
  client.on('close', () => {
    if (S.client !== client || S.mode !== 'mqtt') return;   // 仅当前生效的客户端才更新状态
    setConn('off', '连接已断开');
  });
  return true;
}

function sendTest() {
  if (S.mode !== 'mqtt' || !S.client || !S.client.connected) { alert('请先建立 MQTT 连接'); return; }
  const c = S.cfg, onenet = c.mode === 'onenet';
  const body = onenet ? {
    id: String(Date.now()),
    params: {
      temp: { value: Math.round(20 + Math.random() * 8) }, humi: { value: Math.round(40 + Math.random() * 30) },
      latitude: { value: +(39.9 + Math.random() * 0.02).toFixed(6) },
      longitude: { value: +(116.39 + Math.random() * 0.02).toFixed(6) },
      altitude: { value: 45.0 }, speed: { value: +(Math.random() * 5).toFixed(2) },
      direction: { value: +(Math.random() * 360).toFixed(1) },
    },
  } : { temp: 25, humi: 60, lat: 39.909, lon: 116.397, alt: 45, speed: 3.2, dir: 90, ts: Math.floor(Date.now() / 1000) };
  const topic = onenet ? `$sys/${c.pid}/${c.dev}/thing/property/post` : (c.pub || 'gps/web/data');
  S.client.publish(topic, JSON.stringify(body), { qos: 0 }, err => {
    if (err) log('err', `发布失败: ${err.message}`);
    else { log('ok', `测试数据已发布 → ${topic}`); ingest(parsePayload(JSON.stringify(body)), '测试'); }
  });
}

/* =====================================================================
 * CSV 导出
 * ===================================================================== */
function exportCsv() {
  if (!S.records.length) { alert('暂无数据可导出'); return; }
  const head = '时间,纬度,经度,温度(°C),湿度(%RH),速度(m/s),海拔(m),方向(°)\n';
  const rows = S.records.map(r => [
    new Date(r.ts).toLocaleString('zh-CN', { hour12: false }),
    fmt(r.lat, 6), fmt(r.lon, 6), fmt(r.temp, 1), fmt(r.humi, 0),
    fmt(r.speed, 2), fmt(r.alt, 1), fmt(r.dir, 1),
  ].join(',')).join('\n');
  const blob = new Blob(['\uFEFF' + head + rows], { type: 'text/csv;charset=utf-8' });
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = `gps_records_${new Date().toISOString().slice(0, 19).replace(/[:T]/g, '-')}.csv`;
  a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 5000);
  log('ok', `已导出 ${S.records.length} 条记录`);
}

/* =====================================================================
 * 设置弹窗（<dialog> 降级：不支持时用固定定位 + 遮罩）
 * ===================================================================== */
function openDlg() {
  const d = $('dlgSettings');
  if (typeof d.showModal === 'function') d.showModal();
  else { d.setAttribute('open', ''); document.body.classList.add('dlg-open'); }
}
function closeDlg() {
  const d = $('dlgSettings');
  if (typeof d.close === 'function') d.close();
  else { d.removeAttribute('open'); document.body.classList.remove('dlg-open'); }
}

/* =====================================================================
 * 智能锁箱（ESP8266 Web 接口 /on /off /status，支持 natapp 隧道）
 * ===================================================================== */
function lockSay(cls, msg) { const el = $('lockState'); el.textContent = msg; el.className = cls || ''; }
function saveLockAddr() {
  S.cfg.lock = $('fLockAddr').value.trim();
  const saved = { ...S.cfg };
  if (!S.cfg.remember) { saved.key = ''; saved.token = ''; }
  localStorage.setItem('gpsdash.cfg.v1', JSON.stringify(saved));
}
function lockBuildUrl(path) {
  let base = $('fLockAddr').value.trim().replace(/\/+$/, '');
  if (!base) { lockSay('err', '请先填写 ESP8266 的 IP 或 natapp 隧道地址'); return null; }
  if (!/^https?:\/\//.test(base)) base = 'http://' + base;
  if (location.protocol === 'https:' && base.startsWith('http:')) {
    lockSay('err', '本页以 https 打开，浏览器禁止访问 http 设备（混合内容限制）。请改用 http:// 或 file:// 打开本页');
    log('err', '智能锁：https 页面无法访问 http 设备（混合内容限制）');
    return null;
  }
  return base + path;
}
async function lockCmd(path, label) {
  const url = lockBuildUrl(path);
  if (!url) return;
  saveLockAddr();
  log('info', `智能锁：${label} → ${url}`);
  lockSay('', `${label}：请求中…`);
  const ctl = new AbortController();
  const timer = setTimeout(() => ctl.abort(), 5000);
  try {
    const res = await fetch(url, { signal: ctl.signal });
    const text = (await res.text()).trim();
    lockSay(res.ok ? 'ok' : 'err', `${label}：${text || 'HTTP ' + res.status}`);
    log(res.ok ? 'ok' : 'err', `智能锁 ${label} 回复：${text || 'HTTP ' + res.status}`);
  } catch (err) {
    if (path !== '/status') {
      /* ESP8266WebServer 默认不带 CORS 头：读取被拦，但指令可以 no-cors 盲发 */
      try {
        await fetch(url, { mode: 'no-cors', signal: ctl.signal });
        lockSay('', `${label}：指令已发出（跨域限制，无法读取回复）`);
        log('warn', `智能锁 ${label}：no-cors 盲发完成，结果无法确认`);
      } catch (err2) {
        lockSay('err', `${label}：发送失败（${err2.message || err.message || '网络错误'}）`);
        log('err', `智能锁 ${label} 发送失败：${err2.message || err.message}`);
      }
    } else {
      lockSay('err', `读取状态失败（${err.message}）。ESP8266 未配置 CORS，跨域访问读不到回复，可看固件串口日志确认`);
      log('err', `智能锁状态读取失败：${err.message}`);
    }
  } finally {
    clearTimeout(timer);
  }
}

/* NAV 指令复制（供树莓派 send.py 使用） */
async function copyNavCmd() {
  const cmd = $('navCmd').textContent;
  try {
    await navigator.clipboard.writeText(cmd);
    log('ok', 'NAV 指令已复制：' + cmd);
  } catch {
    const ta = document.createElement('textarea');
    ta.value = cmd;
    document.body.appendChild(ta);
    ta.select();
    try { document.execCommand('copy'); log('ok', 'NAV 指令已复制'); }
    catch { log('err', '复制失败，请手动选择复制：' + cmd); }
    ta.remove();
  }
}

/* =====================================================================
 * 事件绑定 & 初始化
 * ===================================================================== */
function bind() {
  $('btnDemo').onclick = () => S.mode === 'demo' ? stopAll('未连接') : startDemo();
  $('btnSettings').onclick = openDlg;
  $('btnCloseDlg').onclick = closeDlg;
  $('btnConnect').onclick = () => { if (connect()) closeDlg(); };
  $('btnDisconnect').onclick = () => { stopAll('未连接'); log('info', '已手动断开'); };
  $('btnSendTest').onclick = sendTest;
  $('btnExport').onclick = exportCsv;
  $('fMode').onchange = onModeChange;
  $('fLockAddr').onchange = saveLockAddr;

  /* 导航目标 */
  $('btnPickNav').onclick = () => {
    S.pickNav = !S.pickNav;
    $('btnPickNav').textContent = S.pickNav ? '🎯 点击地图设目标' : '🎯 设目标模式';
    $('map').classList.toggle('lockpick-mode', S.pickNav);
    if (S.pickNav) log('info', '设目标模式已开启：点击地图任意位置设为导航目标（再次点击按钮退出）');
  };
  $('btnCopyNav').onclick = copyNavCmd;
  $('btnClearNav').onclick = clearNavTarget;

  /* 智能锁 */
  $('btnLockOpen').onclick = () => lockCmd('/on', '开锁');
  $('btnLockClose').onclick = () => lockCmd('/off', '关锁');
  $('btnLockStatus').onclick = () => lockCmd('/status', '状态');

  $('tabRec').onclick = () => switchTab(true);
  $('tabLog').onclick = () => switchTab(false);
  $('btnClearTab').onclick = () => {
    if ($('bodyRec').classList.contains('active')) {
      const n = S.records.length;
      if (n === 0) { log('info', '数据记录已为空'); return; }
      if (!confirm(`将清空 ${n} 条数据记录（CSV 导出数据源，不可恢复），确定吗？`)) return;
      $('recBody').innerHTML = '';
      S.records = [];
      log('info', '已清空数据记录');
    } else {
      $('log').innerHTML = '';
    }
  };
  $('btnFit').onclick = () => {
    if (S.trail.length > 1 && map) {
      const bounds = S.nav ? S.trail.concat([[S.nav.lat, S.nav.lng]]) : S.trail;
      map.fitBounds(L.latLngBounds(bounds), { padding: [30, 30] });
    }
  };
  $('btnClearTrail').onclick = () => {
    S.trail = [];
    if (S.trailLine) { map.removeLayer(S.trailLine); S.trailLine = null; }
    if (S.marker) { map.removeLayer(S.marker); S.marker = null; }
    if (S.startMarker) { map.removeLayer(S.startMarker); S.startMarker = null; }
    log('info', '已清空地图轨迹');
  };
  if (map) {
    map.on('click', e => {
      if (S.pickNav) setNavTarget(e.latlng.lat, e.latlng.lng, '地图点选');
    });
    map.on('contextmenu', e => setNavTarget(e.latlng.lat, e.latlng.lng, '右键/长按'));
  }
  window.addEventListener('resize', () => { if (S.chart) S.chart.resize(); if (map) map.invalidateSize(); });
}
function switchTab(rec) {
  $('tabRec').classList.toggle('active', rec);
  $('tabLog').classList.toggle('active', !rec);
  $('bodyRec').classList.toggle('active', rec);
  $('bodyLog').classList.toggle('active', !rec);
}

function init() {
  const missing = [];
  if (typeof L === 'undefined') missing.push('地图');
  if (typeof echarts === 'undefined') missing.push('图表');
  if (typeof mqtt === 'undefined') missing.push('MQTT');
  if (missing.length) { $('cdnWarn').style.display = 'block'; log('err', '组件库加载失败: ' + missing.join(' / ')); }

  /* <dialog> 不支持时启用降级样式（iOS ≤ 15.3 等） */
  const probe = document.createElement('dialog');
  if (typeof probe.showModal !== 'function') document.documentElement.classList.add('no-dialog');

  if (typeof L !== 'undefined') initMap();
  if (typeof echarts !== 'undefined') initChart();
  bind();
  loadCfg();
  log('info', '监控台就绪：右上角「▶ 演示模式」直接体验；「⚙ 连接设置」接入 OneNET / 自定义 MQTT；地图上右键（或长按）可设导航目标；下方「🔒 智能锁箱」可控制 ESP8266。');
  log('info', '提示：固件上报坐标系为 GCJ-02（注意：审查发现原固件 wgs84→gcj02 变换非标准，可能存在数十米级系统偏差，详见 README「已知问题」）。');
}

module.exports = { genOnenetToken, parsePayload, parseSerialFrame, classifyMessage, pyQuote, md5Bytes, hmacMd5, b64FromBytes, bytesFromB64, parseTs, haversineM, bearingDeg };

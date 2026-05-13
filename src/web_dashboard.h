#ifndef WEB_DASHBOARD_H
#define WEB_DASHBOARD_H

// =====================================================================
//  Embedded mobile-first dashboard.
//
//  Single-page app served at GET /. Designed to be readable on a phone
//  in portrait and to assemble a WiGLE CSV entirely client-side from
//  navigator.geolocation + the scan events streamed over WebSocket.
//
//  Kept here as a raw C string literal so the build is self-contained
//  (no asset bundling step). Total uncompressed size budget: ~16 KB.
// =====================================================================

static const char HUGINN_DASHBOARD_HTML[] PROGMEM = R"HUGINNHTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<title>HuginnESP</title>
<style>
:root{--bg:#0b0d12;--fg:#e6e8ee;--mut:#7a8092;--accent:#5cc8ff;--warn:#ffb454;--bad:#ff5c7c;--ok:#7ee787;--card:#151823;--bdr:#222633}
*{box-sizing:border-box}
html,body{margin:0;padding:0;background:var(--bg);color:var(--fg);font:14px -apple-system,BlinkMacSystemFont,"SF Pro Text","Segoe UI",Roboto,sans-serif;-webkit-font-smoothing:antialiased}
header{position:sticky;top:0;background:rgba(11,13,18,.92);backdrop-filter:saturate(1.4) blur(10px);border-bottom:1px solid var(--bdr);padding:env(safe-area-inset-top) 12px 8px;z-index:5}
.title{display:flex;align-items:center;gap:8px;padding-top:6px}
.title h1{margin:0;font-size:17px;font-weight:600;letter-spacing:.2px}
.dot{width:8px;height:8px;border-radius:50%;background:var(--mut)}
.dot.live{background:var(--ok);box-shadow:0 0 8px var(--ok)}
.dot.warn{background:var(--warn)}
.tabs{display:flex;gap:4px;margin-top:8px}
.tab{flex:1;text-align:center;padding:8px;border-radius:8px;background:var(--card);color:var(--mut);font-weight:500;font-size:13px}
.tab.active{background:var(--accent);color:#06121a}
.bar{display:flex;gap:6px;align-items:center;padding:8px 12px;border-bottom:1px solid var(--bdr);font-size:12px;color:var(--mut)}
.bar input{flex:1;min-width:0;padding:7px 10px;background:var(--card);color:var(--fg);border:1px solid var(--bdr);border-radius:8px;font-size:13px}
.bar button{padding:7px 10px;background:var(--card);color:var(--fg);border:1px solid var(--bdr);border-radius:8px;font-size:13px;white-space:nowrap}
.bar button.primary{background:var(--accent);color:#06121a;border-color:var(--accent);font-weight:600}
section{padding:8px 12px 80px}
.kpis{display:grid;grid-template-columns:repeat(4,1fr);gap:6px;margin-bottom:10px}
.kpi{background:var(--card);border:1px solid var(--bdr);border-radius:10px;padding:8px;text-align:center}
.kpi .v{font-size:18px;font-weight:600}
.kpi .l{font-size:10px;color:var(--mut);text-transform:uppercase;letter-spacing:.4px}
.list{display:flex;flex-direction:column;gap:6px}
.row{background:var(--card);border:1px solid var(--bdr);border-radius:10px;padding:9px 10px;display:flex;flex-direction:column;gap:3px}
.row.flag-flipper{border-left:3px solid var(--warn)}
.row.flag-airtag{border-left:3px solid var(--accent)}
.row.flag-skimmer{border-left:3px solid var(--bad)}
.r1{display:flex;justify-content:space-between;align-items:baseline;gap:6px}
.r1 .name{font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;flex:1}
.r1 .rssi{font-variant-numeric:tabular-nums;color:var(--mut);font-size:12px}
.r2{display:flex;justify-content:space-between;color:var(--mut);font-size:11px;font-variant-numeric:tabular-nums}
.spark{height:12px;width:60px;display:inline-block;vertical-align:middle}
.muted{color:var(--mut)}
.hide{display:none}
#alerts .row{border-left:3px solid var(--bad)}
.toast{position:fixed;left:50%;transform:translateX(-50%);bottom:calc(20px + env(safe-area-inset-bottom));background:var(--card);border:1px solid var(--bdr);padding:10px 14px;border-radius:10px;box-shadow:0 8px 24px #0008;z-index:10;font-size:13px}
.toast.err{border-color:var(--bad);color:var(--bad)}
.toast.ok{border-color:var(--ok);color:var(--ok)}
footer{position:fixed;bottom:0;left:0;right:0;background:rgba(11,13,18,.94);border-top:1px solid var(--bdr);padding:8px 12px calc(8px + env(safe-area-inset-bottom));font-size:11px;color:var(--mut);display:flex;justify-content:space-between;gap:8px}
</style>
</head>
<body>
<header>
  <div class="title"><span id="dot" class="dot"></span><h1>HuginnESP</h1><span id="mode" class="muted" style="font-size:12px;margin-left:auto"></span></div>
  <div class="tabs"><div class="tab active" data-tab="wifi">Wi-Fi</div><div class="tab" data-tab="ble">BLE</div><div class="tab" data-tab="alerts">Alerts</div></div>
</header>

<div class="bar">
  <input id="filter" placeholder="Filter SSID / MAC / name">
  <button id="cmd-wardrive">Wardrive</button>
  <button id="cmd-stop">Stop</button>
  <button id="btn-csv" class="primary">WiGLE CSV</button>
</div>

<section id="view-wifi">
  <div class="kpis">
    <div class="kpi"><div class="v" id="k-wifi">0</div><div class="l">Wi-Fi now</div></div>
    <div class="kpi"><div class="v" id="k-wifi-uniq">0</div><div class="l">Unique BSSIDs</div></div>
    <div class="kpi"><div class="v" id="k-ble-uniq">0</div><div class="l">Unique BLE</div></div>
    <div class="kpi"><div class="v" id="k-alerts">0</div><div class="l">Alerts</div></div>
  </div>
  <div id="wifi" class="list"></div>
</section>
<section id="view-ble" class="hide">
  <div id="ble" class="list"></div>
</section>
<section id="view-alerts" class="hide">
  <div id="alerts" class="list"></div>
</section>

<footer>
  <span id="conn">connecting…</span>
  <span id="gps">GPS: off</span>
  <span id="seq">seq 0</span>
</footer>

<script>
(() => {
const $ = s => document.querySelector(s);
const wifiEl = $('#wifi'), bleEl = $('#ble'), alertsEl = $('#alerts');
const dot = $('#dot'), conn = $('#conn'), gpsEl = $('#gps'), seqEl = $('#seq');
const filterEl = $('#filter');

// Tabs
document.querySelectorAll('.tab').forEach(t => t.addEventListener('click', () => {
  document.querySelectorAll('.tab').forEach(x => x.classList.remove('active'));
  t.classList.add('active');
  const which = t.dataset.tab;
  $('#view-wifi').classList.toggle('hide', which !== 'wifi');
  $('#view-ble').classList.toggle('hide', which !== 'ble');
  $('#view-alerts').classList.toggle('hide', which !== 'alerts');
}));

// Tracked aggregates (most-recent per MAC). Capped to keep DOM small.
const wifi = new Map(), ble = new Map(), alerts = [];
const rssiHist = new Map();   // mac -> [rssi,...]
function flagsToClass(f){ return (f&1)?'flag-flipper':(f&2)?'flag-airtag':(f&4)?'flag-skimmer':''; }
function flagsToText(f){
  const t=[]; if(f&1)t.push('FLIPPER'); if(f&2)t.push('AIRTAG'); if(f&4)t.push('SKIMMER');
  return t.join(' ');
}

function sparkSvg(history){
  if(!history||history.length<2) return '';
  const w=60,h=12,n=history.length,mn=-100,mx=-30;
  const pts=history.map((v,i)=>{
    const x=(i/(n-1))*w;
    const y=h-((Math.max(mn,Math.min(mx,v))-mn)/(mx-mn))*h;
    return `${x.toFixed(1)},${y.toFixed(1)}`;
  }).join(' ');
  return `<svg class="spark" viewBox="0 0 ${w} ${h}" preserveAspectRatio="none"><polyline points="${pts}" fill="none" stroke="#5cc8ff" stroke-width="1.2"/></svg>`;
}

function render(){
  const f = filterEl.value.trim().toLowerCase();
  const pass = e => !f || (e.mac||'').toLowerCase().includes(f) || (e.name||'').toLowerCase().includes(f);

  const wifiArr = [...wifi.values()].filter(pass).sort((a,b)=>b.ts-a.ts).slice(0,200);
  const bleArr  = [...ble.values()].filter(pass).sort((a,b)=>b.ts-a.ts).slice(0,200);

  $('#k-wifi').textContent      = wifiArr.length;
  $('#k-wifi-uniq').textContent = wifi.size;
  $('#k-ble-uniq').textContent  = ble.size;
  $('#k-alerts').textContent    = alerts.length;

  wifiEl.innerHTML = wifiArr.map(e => `
    <div class="row">
      <div class="r1"><div class="name">${escapeHtml(e.name || '<hidden>')}</div><div class="rssi">${e.rssi} dBm ${sparkSvg(rssiHist.get(e.mac))}</div></div>
      <div class="r2"><span>${e.mac} · ch ${e.channel} · ${authStr(e.auth)}</span><span>${age(e.ts)}</span></div>
    </div>`).join('');

  bleEl.innerHTML = bleArr.map(e => `
    <div class="row ${flagsToClass(e.flags)}">
      <div class="r1"><div class="name">${escapeHtml(e.name || '<unnamed>')}</div><div class="rssi">${e.rssi} dBm ${sparkSvg(rssiHist.get(e.mac))}</div></div>
      <div class="r2"><span>${e.mac}${e.flags?' · '+flagsToText(e.flags):''}</span><span>${age(e.ts)}</span></div>
    </div>`).join('');

  alertsEl.innerHTML = alerts.slice(-100).reverse().map(a => `
    <div class="row"><div class="r1"><div class="name">${escapeHtml(a.kind)}</div><div class="rssi">${a.rssi||''}</div></div><div class="r2"><span>${a.mac}</span><span>${age(a.ts)}</span></div></div>`).join('');
}

function escapeHtml(s){ return (s||'').replace(/[&<>"]/g,c=>({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;"}[c])); }
function authStr(a){ return ["Open","WEP","WPA","WPA2","WPA/WPA2","WPA2-Ent","WPA3"][a]||"?"; }
function age(ts){ const s=Math.max(0,Math.round((Date.now()-ts)/1000)); return s<60?`${s}s`:`${Math.round(s/60)}m`; }

function ingest(evt){
  evt.ts = evt.ts || Date.now();
  const hist = rssiHist.get(evt.mac) || [];
  hist.push(evt.rssi); if(hist.length>20) hist.shift();
  rssiHist.set(evt.mac, hist);

  if(evt.type === 1){
    wifi.set(evt.mac, evt);
  } else if(evt.type === 2){
    ble.set(evt.mac, evt);
    if(evt.flags) alerts.push({kind: flagsToText(evt.flags), mac: evt.mac, rssi: evt.rssi, ts: evt.ts});
  }
}

setInterval(render, 1000);

// ---- WebSocket live stream ----
let ws, wsRetry = 1000, lastSeq = 0;
function connect(){
  const url = (location.protocol==='https:'?'wss://':'ws://') + location.host + '/ws';
  ws = new WebSocket(url);
  ws.onopen = () => { dot.classList.add('live'); conn.textContent = 'live'; wsRetry = 1000; };
  ws.onclose = () => { dot.classList.remove('live'); conn.textContent = 'offline'; setTimeout(connect, wsRetry); wsRetry = Math.min(wsRetry*1.6, 15000); };
  ws.onerror = () => ws.close();
  ws.onmessage = ev => {
    try {
      const m = JSON.parse(ev.data);
      if(m.events) m.events.forEach(ingest);
      if(m.seq != null){ lastSeq = m.seq; seqEl.textContent = 'seq '+m.seq; }
      if(m.mode) $('#mode').textContent = m.mode;
    } catch(e){}
  };
}
connect();

// ---- Bootstrap from REST so the page isn't empty before first WS frame ----
fetch('/api/scans?limit=300').then(r=>r.json()).then(j => {
  (j.events||[]).forEach(ingest);
  render();
}).catch(()=>{});

// ---- Filter ----
filterEl.addEventListener('input', render);

// ---- Command buttons (re-uses serial command vocabulary) ----
function cmd(c){
  fetch('/api/cmd', {method:'POST', headers:{'content-type':'text/plain'}, body:c})
    .then(r=>r.text()).then(t=>toast(t||'ok','ok')).catch(e=>toast('failed: '+e,'err'));
}
$('#cmd-wardrive').addEventListener('click',()=>cmd('wardrive'));
$('#cmd-stop').addEventListener('click',()=>cmd('stop'));

// ---- WiGLE CSV export ----
// Phone supplies GPS via navigator.geolocation. Each scan event is stamped
// with the freshest fix (or the firmware-side GPS fix if it ever arrives
// in the event). The CSV is assembled client-side so there's no need for
// a firmware-side fileesystem.
let lastFix = null, gpsWatchId = null;
function ensureGps(){
  return new Promise((resolve, reject) => {
    if(!('geolocation' in navigator)) return reject(new Error('Geolocation not supported'));
    if(gpsWatchId != null && lastFix) return resolve();
    navigator.geolocation.getCurrentPosition(p => {
      lastFix = p; gpsEl.textContent = 'GPS: ' + p.coords.latitude.toFixed(4) + ',' + p.coords.longitude.toFixed(4);
      gpsWatchId = navigator.geolocation.watchPosition(pp => {
        lastFix = pp; gpsEl.textContent = 'GPS: ' + pp.coords.latitude.toFixed(4) + ',' + pp.coords.longitude.toFixed(4);
      }, () => {}, {enableHighAccuracy:true, maximumAge:5000});
      resolve();
    }, err => reject(err), {enableHighAccuracy:true, timeout:15000});
  });
}

function wigleCsv(){
  // WiGLE pre-import CSV header (Wireshark/Kismet compatible subset).
  const hdr1 = 'WigleWifi-1.4,appRelease=HuginnESP,model=ESP32,release=1.0,device=HuginnESP,display=USB,board=ESP32,brand=Espressif,star=Sol,body=3,subBody=0';
  const hdr2 = 'MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type';
  const rows = [hdr1, hdr2];
  const isoNow = () => new Date().toISOString().replace('T',' ').replace(/\..*/,'');
  const lat = lastFix ? lastFix.coords.latitude : 0;
  const lon = lastFix ? lastFix.coords.longitude : 0;
  const alt = lastFix ? (lastFix.coords.altitude || 0) : 0;
  const acc = lastFix ? (lastFix.coords.accuracy || 0) : 0;
  const auth = a => ({0:'[ESS]',1:'[WEP][ESS]',2:'[WPA-PSK-CCMP][ESS]',3:'[WPA2-PSK-CCMP][ESS]',4:'[WPA-PSK-CCMP][WPA2-PSK-CCMP][ESS]',5:'[WPA2-EAP-CCMP][ESS]',6:'[WPA3-PSK-CCMP][ESS]'}[a]||'[ESS]');
  for(const e of wifi.values()){
    rows.push([e.mac, csvField(e.name||''), auth(e.auth), isoNow(), e.channel, e.rssi,
               (e.lat||lat).toFixed(7), (e.lon||lon).toFixed(7), (e.alt||alt).toFixed(1),
               acc.toFixed(1), 'WIFI'].join(','));
  }
  for(const e of ble.values()){
    rows.push([e.mac, csvField(e.name||''), '', isoNow(), 0, e.rssi,
               (e.lat||lat).toFixed(7), (e.lon||lon).toFixed(7), (e.alt||alt).toFixed(1),
               acc.toFixed(1), 'BLE'].join(','));
  }
  return rows.join('\n');
}
function csvField(s){ s = String(s); return /[,"\n]/.test(s) ? '"'+s.replace(/"/g,'""')+'"' : s; }

$('#btn-csv').addEventListener('click', async () => {
  try {
    await ensureGps();
    const blob = new Blob([wigleCsv()], {type:'text/csv'});
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    const stamp = new Date().toISOString().replace(/[:.]/g,'-').slice(0,19);
    a.download = `huginn-${stamp}.csv`;
    document.body.appendChild(a); a.click(); a.remove();
    toast(`Exported ${wifi.size + ble.size} entries`,'ok');
  } catch(e){ toast('GPS needed: ' + e.message, 'err'); }
});

function toast(msg, kind){
  const el = document.createElement('div');
  el.className = 'toast ' + (kind||'');
  el.textContent = msg;
  document.body.appendChild(el);
  setTimeout(()=>el.remove(), 2400);
}
})();
</script>
</body>
</html>
)HUGINNHTML";

#endif // WEB_DASHBOARD_H

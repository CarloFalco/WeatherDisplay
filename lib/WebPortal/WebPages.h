/**
 * @file WebPages.h
 * @brief Pagina HTML della Web UI (embedded in flash).
 */
#pragma once

#include <pgmspace.h>

static const char kIndexHtml[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Gateway Meteo</title>
<style>
:root{--bg:#f4f4f0;--card:#fff;--ink:#222;--mut:#777;--acc:#2a6f4e;--bad:#b33}
*{box-sizing:border-box}
body{font-family:system-ui,sans-serif;margin:0;background:var(--bg);color:var(--ink)}
header{background:var(--ink);color:#fff;padding:14px 20px;display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap}
header h1{font-size:18px;margin:0}
header small{opacity:.7}
nav{display:flex;gap:8px;padding:12px 20px}
nav button{padding:8px 18px;border:1px solid #ccc;background:var(--card);border-radius:8px;cursor:pointer;font-size:14px}
nav button.on{background:var(--acc);color:#fff;border-color:var(--acc)}
main{padding:0 20px 40px;max-width:980px}
.card{background:var(--card);border:1px solid #ddd;border-radius:10px;padding:16px;margin-bottom:14px}
.card h2{margin:0 0 10px;font-size:15px;text-transform:uppercase;letter-spacing:.05em;color:var(--mut)}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:10px}
.kv{display:flex;justify-content:space-between;border-bottom:1px dashed #eee;padding:4px 0;font-size:14px}
.kv b{font-weight:600}
.ok{color:var(--acc)}.bad{color:var(--bad)}
table{width:100%;border-collapse:collapse;font-size:14px}
th,td{text-align:left;padding:6px 8px;border-bottom:1px solid #eee}
th{color:var(--mut);font-weight:600}
label{display:block;font-size:13px;color:var(--mut);margin:10px 0 3px}
input,select{width:100%;padding:8px;border:1px solid #ccc;border-radius:6px;font-size:14px}
input[type=checkbox]{width:auto}
.btn{padding:9px 20px;border:0;border-radius:8px;background:var(--acc);color:#fff;font-size:14px;cursor:pointer;margin:6px 6px 0 0}
.btn.sec{background:#666}
.btn.warn{background:var(--bad)}
#msg{position:fixed;top:10px;right:10px;background:var(--ink);color:#fff;padding:10px 16px;border-radius:8px;display:none}
fieldset{border:1px solid #ddd;border-radius:8px;margin-bottom:12px}
legend{font-size:13px;font-weight:600;padding:0 6px}
.fldgrid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:0 14px}
</style>
</head>
<body>
<header><h1>Gateway Meteo LoRa</h1><small id="fwv"></small></header>
<nav>
<button id="tabS" class="on" onclick="show('S')">Stato</button>
<button id="tabC" onclick="show('C')">Configurazione</button>
</nav>
<main>
<div id="pgS">
 <div class="card"><h2>Gateway</h2><div class="grid" id="gwinfo"></div></div>
 <div class="card"><h2>Stazioni collegate</h2><table id="nodes"><thead><tr>
  <th>Nome</th><th>FW</th><th>Temp</th><th>Umid</th><th>Batteria</th><th>RSSI</th><th>Ultimo agg.</th><th>Stato</th><th></th>
 </tr></thead><tbody></tbody></table></div>
 <div class="card"><h2>OTA nodi</h2><div class="grid" id="otainfo"></div>
  <button class="btn" onclick="api('/api/ota/check','POST')">Controlla aggiornamenti</button></div>
 <div class="card"><h2>Manutenzione</h2>
  <button class="btn warn" onclick="if(confirm('Riavviare il gateway?'))api('/api/reboot','POST')">Riavvia</button>
  <a class="btn sec" style="text-decoration:none" href="/api/config/export" download="config.json">Esporta config</a>
  <label style="display:inline">Importa config <input type="file" id="impf" style="width:auto" onchange="importCfg(this)"></label>
  <form method="POST" action="/update" enctype="multipart/form-data" style="margin-top:10px">
   <label>Firmware gateway (.bin)</label>
   <input type="file" name="fw" style="width:auto"> <button class="btn sec">Aggiorna firmware</button>
  </form>
 </div>
</div>
<div id="pgC" style="display:none">
 <div class="card"><h2>Configurazione</h2><form id="cfgform"></form>
  <button class="btn" onclick="saveCfg()">Salva e riavvia</button></div>
</div>
</main>
<div id="msg"></div>
<script>
const LBL={ssid:'SSID',password:'Password',host:'Host',port:'Porta',username:'Utente',
base_topic:'Topic base',frequency:'Frequenza (Hz)',bandwidth:'Banda (Hz)',
spreading_factor:'Spreading factor',coding_rate:'Coding rate',tx_power:'Potenza TX (dBm)',
sync_word:'Sync word',sck:'Pin SCK',miso:'Pin MISO',mosi:'Pin MOSI',cs:'Pin CS',
rst:'Pin RST',dio0:'Pin DIO0',dio1:'Pin DIO1',name:'Nome gateway',timezone:'Fuso orario',
enabled:'Abilitato',city:'Città',country:'Paese (ISO)',api_key:'API key',
update_interval:'Intervallo agg. (s)',repo:'Repository GitHub',asset_name:'Nome asset',
check_interval:'Intervallo controllo (s)',rotation_interval:'Rotazione stazioni (s)',
full_refresh_every:'Refresh completo ogni',node_timeout:'Timeout nodo (s)',
level:'Livello log',mqtt:'Log via MQTT',file:'Log su file'};
const SEC={wifi:'Wi-Fi',mqtt:'Broker MQTT',lora:'Radio LoRa',pins:'Pin LoRa',
gateway:'Gateway',weather_api:'OpenWeatherMap',node_ota:'OTA nodi',display:'Display',log:'Logging'};
function show(t){for(const p of['S','C']){document.getElementById('pg'+p).style.display=p===t?'':'none';
document.getElementById('tab'+p).className=p===t?'on':''}}
function msg(t,bad){const m=document.getElementById('msg');m.textContent=t;
m.style.background=bad?'#b33':'#2a6f4e';m.style.display='block';setTimeout(()=>m.style.display='none',4000)}
async function api(u,m,b){try{const r=await fetch(u,{method:m||'GET',body:b});
if(!r.ok)throw new Error(r.status);msg('OK');return r}catch(e){msg('Errore: '+e.message,1)}}
function kv(k,v,cls){return `<div class="kv"><span>${k}</span><b class="${cls||''}">${v}</b></div>`}
function onoff(b){return b?'<span class="ok">connesso</span>':'<span class="bad">disconnesso</span>'}
async function refresh(){let r;try{r=await fetch('/api/status')}catch(e){return}
const s=await r.json();
document.getElementById('fwv').textContent='fw '+s.fw+' · '+s.name;
document.getElementById('gwinfo').innerHTML=
 kv('Wi-Fi',s.wifi.connected?s.wifi.ssid+' ('+s.wifi.rssi+' dBm)':'disconnesso',s.wifi.connected?'ok':'bad')+
 kv('IP',s.wifi.ip)+kv('MQTT',onoff(s.mqtt.connected))+
 kv('LoRa',s.lora.ready?'attiva · '+s.lora.packets+' pacchetti':'non disponibile',s.lora.ready?'ok':'bad')+
 kv('Meteo esterno',s.weather.valid?s.weather.temp+'°C · '+s.weather.desc:'n/d')+
 kv('Uptime',s.uptime)+kv('Heap libero',Math.round(s.free_heap/1024)+' kB')+kv('Ora',s.time);
const tb=document.querySelector('#nodes tbody');tb.innerHTML='';
for(const n of s.nodes){tb.innerHTML+=`<tr><td>${n.id}</td><td>${n.fw||'-'}</td>
<td>${n.t==null?'-':n.t+'°C'}</td><td>${n.rh==null?'-':n.rh+'%'}</td>
<td>${n.battery_pct==null?'-':n.battery_pct+'% ('+n.vbat+'V)'}</td>
<td>${n.rssi} dBm</td><td>${n.last_seen}</td>
<td class="${n.offline?'bad':'ok'}">${n.offline?'offline':'online'}</td>
<td><button class="btn sec" style="padding:4px 10px" onclick="otaNode('${n.id}')">Aggiorna</button></td></tr>`}
if(!s.nodes.length)tb.innerHTML='<tr><td colspan="9">Nessuna stazione ricevuta finora</td></tr>';
document.getElementById('otainfo').innerHTML=
 kv('Stato',s.ota.state)+kv('Ultima release',s.ota.latest_version||'-')+
 (s.ota.node?kv('Nodo in aggiornamento',s.ota.node+' · '+s.ota.progress+'%'):'')}
function otaNode(id){if(confirm('Aggiornare il nodo '+id+'?'))api('/api/ota/confirm','POST',id)}
function fld(path,key,val){const id='f_'+path.replace(/\./g,'_');
if(typeof val==='boolean')return `<label>${LBL[key]||key}
<input type="checkbox" id="${id}" data-path="${path}" data-t="b" ${val?'checked':''}></label>`;
const t=typeof val==='number'?'number':(key.includes('password')||key==='api_key'?'password':'text');
const ph=t==='password'?' placeholder="(invariato)"':'';
return `<label>${LBL[key]||key}
<input type="${t}" id="${id}" data-path="${path}" data-t="${typeof val==='number'?'n':'s'}" value="${t==='password'?'':val}"${ph}></label>`}
function renderForm(obj,prefix){let h='';
for(const[k,v]of Object.entries(obj)){const p=prefix?prefix+'.'+k:k;
if(v!==null&&typeof v==='object'){h+=`<fieldset><legend>${SEC[k]||k}</legend><div class="fldgrid">${renderForm(v,p)}</div></fieldset>`}
else h+=fld(p,k,v)}
return h}
async function loadCfg(){const r=await fetch('/api/config');const c=await r.json();
document.getElementById('cfgform').innerHTML=renderForm(c,'')}
async function saveCfg(){const out={};
for(const el of document.querySelectorAll('#cfgform [data-path]')){
const path=el.dataset.path.split('.');let o=out;
for(let i=0;i<path.length-1;i++){o[path[i]]=o[path[i]]||{};o=o[path[i]]}
const k=path[path.length-1];
o[k]=el.dataset.t==='b'?el.checked:(el.dataset.t==='n'?Number(el.value):el.value)}
const r=await api('/api/config','POST',JSON.stringify(out));
if(r){msg('Salvato, riavvio...');setTimeout(()=>location.reload(),8000)}}
function importCfg(inp){const f=inp.files[0];if(!f)return;const rd=new FileReader();
rd.onload=async()=>{const r=await api('/api/config/import','POST',rd.result);
if(r){msg('Importata, riavvio...');setTimeout(()=>location.reload(),8000)}};rd.readAsText(f)}
loadCfg();refresh();setInterval(refresh,5000);
</script>
</body>
</html>)HTML";

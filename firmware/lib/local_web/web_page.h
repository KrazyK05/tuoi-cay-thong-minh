// Trang web nội bộ chạy ngay trên ESP32 (http://<IP> hoặc http://iotfarm.local).
// Dùng được trong mạng LAN kể cả khi mất Internet / server cloud ngủ.
#pragma once
#include <pgmspace.h>

const char LOCAL_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="vi"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>IoTFarm - Nội bộ</title>
<style>
:root{--bg:#f4f7f5;--c:#fff;--t:#16241c;--m:#5b6b62;--b:#d9e3dd;--p:#1f7a4d;--w:#1d6fb8;--s:#eef3f0}
@media(prefers-color-scheme:dark){:root{--bg:#0f1512;--c:#161e1a;--t:#e4ece7;--m:#97a69d;--b:#2a3630;--p:#3fb57a;--w:#5aa8ec;--s:#1c2621}}
*{box-sizing:border-box}body{margin:0;font:15px/1.5 system-ui,sans-serif;background:var(--bg);color:var(--t)}
main{max-width:520px;margin:0 auto;padding:16px}h1{font-size:1.3rem;margin:0}
.card{background:var(--c);border:1px solid var(--b);border-radius:12px;padding:16px;margin-top:14px}
.g{display:grid;grid-template-columns:1fr 1fr;gap:10px}.k{color:var(--m);font-size:.8rem}.v{font-size:1.6rem;font-weight:700}
.seg{display:flex;background:var(--s);padding:3px;border-radius:10px}.seg button{flex:1;border:0;padding:8px;border-radius:8px;background:none;font:inherit;font-weight:600;color:var(--m)}
.seg button.on{background:var(--c);color:var(--t)}
.pump{width:100%;margin-top:12px;padding:16px;border-radius:12px;border:2px solid var(--b);background:var(--s);color:var(--t);font:inherit;font-weight:700;font-size:1.05rem}
.pump.run{background:var(--w);border-color:var(--w);color:#fff}
label{font-size:.85rem;font-weight:600}input{width:100%;padding:8px;border:1px solid var(--b);border-radius:8px;font:inherit;background:var(--c);color:var(--t)}
.save{margin-top:12px;width:100%;padding:10px;border:0;border-radius:10px;background:var(--p);color:#fff;font:inherit;font-weight:600}
.m{color:var(--m);font-size:.85rem}.dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:6px}
</style></head><body><main>
<h1>Hệ thống giám sát cây trồng</h1><div class="m" id="st">Đang tải…</div>
<div class="card g">
 <div><div class="k">Nhiệt độ</div><div class="v" id="t">--</div></div>
 <div><div class="k">Độ ẩm không khí</div><div class="v" id="h">--</div></div>
 <div><div class="k">Độ ẩm đất</div><div class="v" id="s">--</div><div class="m" id="raw"></div></div>
 <div><div class="k">Máy bơm</div><div class="v" id="p">--</div></div>
</div>
<div class="card">
 <div class="seg"><button id="ma" onclick="ctl({mode:'auto'})">Tự động</button><button id="mm" onclick="ctl({mode:'manual'})">Thủ công</button></div>
 <button class="pump" id="pb" onclick="pumpClick()"></button>
</div>
<form class="card" onsubmit="save(event)">
 <div class="g">
  <div><label>Bật bơm khi đất &lt; (%)</label><input id="lo" type="number" min="0" max="99"></div>
  <div><label>Tắt bơm khi đất ≥ (%)</label><input id="hi" type="number" min="1" max="100"></div>
  <div><label>Bơm tối đa (giây)</label><input id="mx" type="number" min="5" max="3600"></div>
  <div><label>Nghỉ giữa 2 lần (giây)</label><input id="cd" type="number" min="0" max="86400"></div>
 </div>
 <button class="save">Lưu cài đặt</button>
</form>
<div class="card"><div class="k">Đồng hồ DS1302</div>
 <div style="display:flex;gap:10px;align-items:center;justify-content:space-between;flex-wrap:wrap">
  <b id="clk">--</b>
  <button class="save" style="width:auto;margin:0;padding:8px 14px" onclick="setClock()">Đặt giờ theo máy này</button>
 </div>
</div>
<p class="m">Trang này chạy trực tiếp trên ESP32. Mọi thay đổi ở đây sẽ tự đồng bộ lên cloud khi có mạng.</p>
</main><script>
let S={},dirty=false;const $=i=>document.getElementById(i);
document.querySelectorAll('input').forEach(i=>i.oninput=()=>dirty=true);
function f(v,d){return v==null?'--':Number(v).toFixed(d)}
async function load(){try{S=await(await fetch('/api/status')).json();
$('t').textContent=f(S.t,1)+'°C';$('h').textContent=f(S.h,0)+'%';$('s').textContent=S.soil_ok?S.soil+'%':'Lỗi';$('raw').textContent='ADC thô: '+S.soil_raw;
$('p').textContent=S.pump?'Đang bơm':'Tắt';$('p').style.color=S.pump?'var(--w)':'var(--m)';
$('ma').className=S.mode=='auto'?'on':'';$('mm').className=S.mode=='manual'?'on':'';
$('pb').className='pump'+(S.pump?' run':'');
$('pb').textContent=S.mode=='auto'?'Đang tự động — bấm để chuyển Thủ công':(S.manual_pump?'TẮT BƠM':'BẬT BƠM');
$('st').innerHTML='<span class="dot" style="background:'+(S.cloud?'#1f7a4d':'#b3261e')+'"></span>'+(S.cloud?'Đã kết nối cloud':'Chưa kết nối cloud')+' · IP '+S.ip+' · WiFi '+S.rssi+' dBm';
$('clk').textContent=(S.time||'Chua co gio')+' · '+S.time_status;$('clk').style.color=S.time_status=='RTC'?'':'#b3261e';
if(!dirty){$('lo').value=S.soil_low;$('hi').value=S.soil_high;$('mx').value=S.max_pump_sec;$('cd').value=S.cooldown_sec}
}catch(e){$('st').textContent='Mất kết nối với ESP32'}}
async function post(u,b){const r=await fetch(u,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b)});
if(!r.ok)alert((await r.json()).error||'Lỗi');load()}
function ctl(b){post('/api/control',b)}
function setClock(){if(confirm('Ghi giờ của thiết bị này ('+new Date().toLocaleString('vi-VN')+') vào DS1302?'))post('/api/time',{epoch:Math.floor(Date.now()/1000)})}
function save(e){e.preventDefault();dirty=false;post('/api/settings',{soil_low:+$('lo').value,soil_high:+$('hi').value,max_pump_sec:+$('mx').value,cooldown_sec:+$('cd').value})}
function pumpClick(){S.mode=='auto'?ctl({mode:'manual'}):ctl({pump:!S.manual_pump})}
load();setInterval(load,2000);
</script></body></html>)HTML";

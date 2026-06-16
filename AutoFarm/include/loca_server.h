#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <functional>
#include "config.h"

// Fills a JsonDocument with the current sensor/relay snapshot.
using SnapshotFn = std::function<void(JsonDocument&)>;
// Sets a relay by id; returns false if the id is unknown.
using RelayCmdFn = std::function<bool(const char* id, bool state)>;

// Dashboard page — generic: renders whatever /data returns, so new
// sensors/relays appear automatically with no markup changes.
static const char DASH_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>AutoFarm</title>
<style>
*{box-sizing:border-box}
body{font-family:sans-serif;max-width:520px;margin:0 auto;padding:20px;background:#1a1a2e;color:#eee}
h2{text-align:center}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin:16px 0}
.card{background:#16213e;border:1px solid #2a3a5e;border-radius:10px;padding:16px;text-align:center}
.card .label{font-size:13px;color:#9aa;text-transform:uppercase;letter-spacing:.5px}
.card .val{font-size:30px;font-weight:bold;margin:6px 0}
.card .unit{font-size:14px;color:#9aa}
.card.off{opacity:.4}
.dot{display:inline-block;width:8px;height:8px;border-radius:50%;background:#2ecc71;margin-left:6px}
.dot.bad{background:#e74c3c}
.relays{display:flex;gap:10px;flex-wrap:wrap;justify-content:center}
.relay{flex:1;min-width:120px;padding:14px;border-radius:8px;border:none;font-size:15px;font-weight:bold;cursor:pointer;color:#fff}
.relay.on{background:#2ecc71}
.relay.offstate{background:#444}
.meta{text-align:center;color:#778;font-size:12px;margin-top:16px}
</style></head><body>
<h2>&#127793; AutoFarm</h2>
<div id="sensors" class="grid"></div>
<div id="relays" class="relays"></div>
<div class="meta" id="meta">Loading...</div>
<script>
const LABELS={air_temp:'Temp',humidity:'Humidity',water_level:'Water',ph:'pH'};
async function refresh(){
  try{
    let d=await (await fetch('/data')).json();
    let s=document.getElementById('sensors');s.innerHTML='';
    for(const [k,v] of Object.entries(d.sensors||{})){
      let c=document.createElement('div');
      c.className='card'+(v.online?'':' off');
      c.innerHTML='<div class="label">'+(LABELS[k]||k)+
        '<span class="dot'+(v.online?'':' bad')+'"></span></div>'+
        '<div class="val">'+Number(v.value).toFixed(1)+'</div>'+
        '<div class="unit">'+(v.unit||'')+'</div>';
      s.appendChild(c);
    }
    let r=document.getElementById('relays');r.innerHTML='';
    for(const [k,st] of Object.entries(d.relays||{})){
      let on=st==='on';
      let b=document.createElement('button');
      b.className='relay '+(on?'on':'offstate');
      b.textContent=k.toUpperCase()+': '+(on?'ON':'OFF');
      b.onclick=()=>toggle(k,!on);
      r.appendChild(b);
    }
    document.getElementById('meta').textContent=
      (d.grow?('Grow: '+d.grow+'  \u2022  '):'')+'Uptime: '+fmt(d.uptime||0);
  }catch(e){document.getElementById('meta').textContent='Disconnected \u2014 retrying...';}
}
async function toggle(id,state){
  await fetch('/relay?id='+encodeURIComponent(id)+'&state='+(state?1:0));
  refresh();
}
function fmt(s){let h=s/3600|0,m=(s%3600)/60|0;return h+'h '+m+'m';}
refresh();setInterval(refresh,2500);
</script></body></html>
)HTML";

class LocalServer {
public:
    LocalServer() : _web(WEB_PORT) {}

    void begin(SnapshotFn snapshot, RelayCmdFn relayCmd) {
        _snapshot = snapshot;
        _relayCmd = relayCmd;

        _web.on("/",      [this]() { _web.send_P(200, "text/html", DASH_HTML); });
        _web.on("/data",  [this]() { handleData(); });
        _web.on("/relay", [this]() { handleRelay(); });
        _web.onNotFound([this]() { _web.send(404, "text/plain", "Not found"); });
        _web.begin();

        Serial.printf("[Local] Dashboard at http://%s/\n",
            WiFi.localIP().toString().c_str());
    }

    void loop() { _web.handleClient(); }

private:
    WebServer  _web;
    SnapshotFn _snapshot;
    RelayCmdFn _relayCmd;

    void handleData() {
        JsonDocument doc;
        if (_snapshot) _snapshot(doc);
        String out;
        serializeJson(doc, out);
        _web.send(200, "application/json", out);
    }

    void handleRelay() {
        if (!_relayCmd) { _web.send(503, "text/plain", "No relay handler"); return; }
        String id = _web.arg("id");
        String st = _web.arg("state");
        if (id.isEmpty() || st.isEmpty()) {
            _web.send(400, "text/plain", "id & state required");
            return;
        }
        bool on = (st == "1" || st == "on");
        bool ok = _relayCmd(id.c_str(), on);
        _web.send(ok ? 200 : 404, "text/plain", ok ? "ok" : "unknown relay");
    }
};

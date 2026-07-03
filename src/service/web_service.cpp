#include "web_service.h"
#include "operation_mode_service.h"
#include "../config/cfg_display.h"

// ── Original interactive HTML (PROGMEM) ────────────────────────
static const char INDEX_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>Clawd Mochi</title>
<style>
*{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent}
body{background:#1c1c20;font-family:'Courier New',monospace;color:#e8e4dc;
  display:flex;flex-direction:column;align-items:center;
  padding:20px 14px 52px;gap:14px;min-height:100vh}
.hdr{text-align:center;padding:2px 0 4px}
.mascot{font-size:15px;color:#c96a3e;line-height:1.3;font-weight:bold;
  font-family:'Courier New',monospace;display:block;letter-spacing:1px}
.sitename{font-size:10px;color:#5a5048;margin-top:8px;letter-spacing:3px}
.sec{width:100%;max-width:390px;font-size:10px;color:#8a8278;
  letter-spacing:2px;font-weight:bold;padding:0 2px}
.busy{width:100%;max-width:390px;height:2px;background:#2e2a28;
  border-radius:1px;overflow:hidden;opacity:0;transition:opacity .2s}
.busy.show{opacity:1}
.busy-i{height:100%;width:30%;background:#c96a3e;border-radius:1px;
  animation:sl 1s linear infinite}
@keyframes sl{0%{margin-left:-30%}100%{margin-left:100%}}
.ctrl{display:flex;gap:8px;width:100%;max-width:390px}
.cbtn{flex:1;background:#252428;border:1.5px solid #38343a;border-radius:10px;
  color:#b8b4ac;font-family:'Courier New',monospace;font-size:11px;font-weight:bold;
  padding:12px 4px;cursor:pointer;text-align:center;transition:all .12s}
.cbtn:active:not(:disabled){transform:scale(.94)}
.cbtn:disabled{opacity:.3;cursor:default}
.cbtn.on{border-color:#c96a3e;color:#c96a3e;background:#201408}
.cbtn.dim{border-color:#2e2a28;color:#4a4540}
.vgrid{display:grid;grid-template-columns:1fr 1fr;gap:8px;width:100%;max-width:390px}
.vbtn{background:#252428;border:1.5px solid #38343a;border-radius:12px;
  color:#d8d4cc;font-family:'Courier New',monospace;
  padding:14px 6px 10px;cursor:pointer;text-align:center;
  transition:all .12s;user-select:none}
.vbtn:active:not(:disabled){transform:scale(.94)}
.vbtn:disabled{opacity:.3;cursor:default}
.vbtn .ic{font-size:20px;display:block;margin-bottom:4px;line-height:1;color:#c96a3e}
.vbtn .nm{font-size:12px;font-weight:bold;color:#e8e4dc}
.vbtn .ht{font-size:9px;color:#8a8278;margin-top:3px}
.vbtn.active{border-color:#c96a3e;background:#201408}
.vbtn[data-v="2"].active{border-color:#4a8acd;background:#0c1628}
.vbtn[data-v="3"].active{border-color:#38343a;background:#201c18}
.speed-row{width:100%;max-width:390px;display:flex;align-items:center;gap:10px}
.sl{font-size:10px;color:#6a6058;white-space:nowrap;min-width:36px}
input[type=range]{flex:1;accent-color:#c96a3e;cursor:pointer;height:20px}
.sv{font-size:11px;color:#c96a3e;min-width:44px;text-align:right;font-weight:bold}
.twrap{width:100%;max-width:390px;display:none;flex-direction:column;gap:8px}
.twrap.open{display:flex}
.thdr{display:flex;justify-content:space-between;align-items:center}
.tttl{font-size:11px;color:#28b878;letter-spacing:1px;font-weight:bold}
.tx{background:#0c1e12;border:2px solid #1a4828;border-radius:9px;
  color:#28b878;font-family:'Courier New',monospace;font-size:13px;
  font-weight:bold;padding:10px 18px;cursor:pointer}
.tx:active{background:#081410}
.trow{display:flex;gap:6px}
.tin{flex:1;background:#0c1018;border:1.5px solid #1a2820;border-radius:9px;
  color:#40d880;font-family:'Courier New',monospace;font-size:15px;
  padding:11px;outline:none}
.tin::placeholder{color:#2a3828}
.tgo{background:#1a9060;border:none;border-radius:9px;color:#fff;
  font-family:'Courier New',monospace;font-size:22px;font-weight:bold;
  padding:11px 16px;cursor:pointer;min-width:52px}
.tgo:active{background:#0f6040}
.cwrap{width:100%;max-width:390px;background:#222028;border:1.5px solid #38343a;
  border-radius:12px;padding:12px;flex-direction:column;gap:10px;display:none}
.cwrap.open{display:flex}
.crow{display:flex;gap:8px}
.ci{display:flex;flex-direction:column;align-items:center;gap:4px;flex:1}
.cl{font-size:10px;color:#7a7068;letter-spacing:1px;font-weight:bold}
.cs{width:100%;height:38px;border-radius:7px;border:1.5px solid #38343a;cursor:pointer;padding:0}
.dacts{display:flex;gap:7px}
.db{flex:1;background:#1c1820;border:1.5px solid #38343a;border-radius:9px;
  color:#c0bab8;font-family:'Courier New',monospace;font-size:11px;
  font-weight:bold;padding:11px 4px;cursor:pointer;transition:all .12s}
.db:active{transform:scale(.95);background:#281838}
.db.hi{border-color:#c96a3e;color:#c96a3e}
canvas{width:100%;border-radius:8px;border:1.5px solid #38343a;
  touch-action:none;cursor:crosshair;display:block}
.toast{position:fixed;bottom:18px;left:50%;transform:translateX(-50%);
  background:#252428;border:1.5px solid #38343a;border-radius:9px;
  font-size:12px;color:#d8d4cc;padding:7px 16px;opacity:0;
  transition:opacity .18s;pointer-events:none;white-space:nowrap;z-index:99}
.toast.show{opacity:1}
</style>
</head>
<body>
<div class="hdr">
  <span class="mascot">&#x2590;&#x259B;&#x2588;&#x2588;&#x2588;&#x259C;&#x258C;<br>&#x259C;&#x2588;&#x2588;&#x2588;&#x2588;&#x2588;&#x259B;<br>&#x2598;&#x2598;&nbsp;&#x259D;&#x259D;</span>
  <div class="sitename">CLAWD &middot; MOCHI &middot; CONTROLLER</div>
</div>
<div class="busy" id="busy"><div class="busy-i"></div></div>
<div class="sec">// controls</div>
<div class="ctrl">
  <button class="cbtn on" id="blBtn" onclick="toggleBL()">&#9728; display on</button>
</div>
<div class="sec">// wifi setup</div>
<div id="wwrap" style="width:100%;max-width:390px;display:flex;flex-direction:column;gap:10px">
  <div id="wstatus" style="font-size:11px;color:#8a8278;text-align:center;padding:4px">checking...</div>
  <div id="wlist" style="display:none;flex-direction:column;gap:6px"></div>
  <div id="wform" style="display:none;flex-direction:column;gap:8px">
    <input class="tin" id="wssid" type="text" placeholder="SSID"
           autocomplete="off" autocorrect="off" autocapitalize="off" spellcheck="false"
           style="background:#0c1018;border:1.5px solid #1a2820;color:#c8c4bc">
    <input class="tin" id="wpass" type="password" placeholder="password"
           style="background:#0c1018;border:1.5px solid #1a2820;color:#c8c4bc">
    <button class="tgo" onclick="connectWifi()" style="font-size:14px;padding:11px">&#128246; connect</button>
  </div>
  <button class="cbtn" onclick="loadWifiList()" id="wscanBtn">&#128269; scan networks</button>
</div>
<div class="sec">// serial mode</div>
<div class="ctrl">
  <button class="cbtn" onclick="useSerialMode()">&#9881; use serial mode</button>
</div>
<div class="sec">// views</div>
<div class="vgrid">
  <button class="vbtn active" data-v="0" onclick="setView(0)">
    <span class="ic">&#9632; &#9632;</span>
    <span class="nm">Normal eyes</span>
    <span class="ht">wiggle + blink</span>
  </button>
  <button class="vbtn" data-v="1" onclick="setView(1)">
    <span class="ic">&gt; &lt;</span>
    <span class="nm">Squish eyes</span>
    <span class="ht">open / close</span>
  </button>
  <button class="vbtn" data-v="2" onclick="setView(2)">
    <span class="ic">{ }</span>
    <span class="nm">Claude Code</span>
    <span class="ht">opens terminal</span>
  </button>
  <button class="vbtn" data-v="3" onclick="toggleCanvas()">
    <span class="ic">&#11035;</span>
    <span class="nm">Canvas</span>
    <span class="ht">draw on display</span>
  </button>
</div>
<div class="sec">// speed</div>
<div class="speed-row">
  <span class="sl">slow</span>
  <input type="range" id="spd" min="1" max="3" value="1" step="1" oninput="setSpeed(this.value)">
  <span class="sv" id="spdV">slow</span>
</div>
<div class="ctrl">
  <div class="ci" style="flex:1;display:flex;flex-direction:column;gap:4px;align-items:stretch">
    <span class="cl" style="font-size:10px;color:#8a8278;letter-spacing:1px;font-weight:bold;text-align:center">BACKGROUND</span>
    <input type="color" class="cs" id="bgCol" value="#aa4818" oninput="onBgChange(this.value)">
  </div>
  <div class="ci" style="flex:1;display:flex;flex-direction:column;gap:4px;align-items:stretch">
    <span class="cl" style="font-size:10px;color:#8a8278;letter-spacing:1px;font-weight:bold;text-align:center">PEN COLOR</span>
    <input type="color" class="cs" id="penCol" value="#000000">
  </div>
</div>
<div class="sec">// terminal</div>
<div class="twrap" id="twrap">
  <div class="thdr">
    <span class="tttl">&#9658; clawd:~$</span>
    <button class="tx" onclick="closeTerm()">&#x2715; exit terminal</button>
  </div>
  <div class="trow">
    <input class="tin" id="tin" type="text" placeholder="type here..."
           autocomplete="off" autocorrect="off" autocapitalize="off" spellcheck="false">
    <button class="tgo" onclick="termEnter()">&#8629;</button>
  </div>
</div>
<div class="cwrap" id="cwrap">
  <div class="dacts">
    <button class="db hi" onclick="clearAll()">&#11035; clear</button>
    <button class="db" style="border-color:#28b878;color:#28b878" onclick="toggleCanvas()">&#10003; done</button>
  </div>
  <canvas id="cvs" width="240" height="240"></canvas>
</div>
<div class="toast" id="toast"></div>
<script>
let activeView=0,termOpen=false,canvasOpen=false,blOn=true,isBusy=false,drawing=false;
let lastX=0,lastY=0,tt;
const spdLabels=['','slow','normal','fast'];
function toast(msg,ok=true){const el=document.getElementById('toast');el.textContent=msg;el.style.borderColor=ok?'#28b878':'#c96a3e';el.classList.add('show');clearTimeout(tt);tt=setTimeout(()=>el.classList.remove('show'),1300);}
function setBusy(b){isBusy=b;document.getElementById('busy').classList.toggle('show',b);const locked=b||termOpen;document.querySelectorAll('.vbtn').forEach(el=>{el.disabled=canvasOpen?parseInt(el.dataset.v)!==3:locked;});document.querySelectorAll('.cbtn').forEach(el=>{if(el.id!=='blBtn')el.disabled=locked;});}
async function req(path){try{const r=await fetch(path);return r.ok;}catch(e){toast('no connection',false);return false;}}
async function waitNotBusy(){for(let i=0;i<100;i++){try{const r=await fetch('/state');const j=await r.json();if(!j.busy)return;}catch(e){}await new Promise(r=>setTimeout(r,150));}}
async function onBgChange(hex){if(canvasOpen){await req('/draw/clear?bg='+encodeURIComponent(hex));}else{await req('/redraw?bg='+encodeURIComponent(hex));}redrawCanvas(hex);}
async function setSpeed(v){document.getElementById('spdV').textContent=spdLabels[v];await req('/speed?v='+v);}
async function setView(v){if(isBusy||termOpen||canvasOpen)return;if(v===3){toggleCanvas();return;}const keys=['w','s','d'];if(!await req('/cmd?k='+keys[v]))return;activeView=v;document.querySelectorAll('.vbtn').forEach(b=>b.classList.toggle('active',parseInt(b.dataset.v)===v));if(v===2){termOpen=true;document.getElementById('twrap').classList.add('open');setBusy(false);setBusy(false);document.querySelectorAll('.vbtn,.lbtn').forEach(b=>b.disabled=true);document.getElementById('tin').focus();toast('terminal open');return;}setBusy(true);await waitNotBusy();setBusy(false);}
async function toggleBL(){blOn=!blOn;await req('/backlight?on='+(blOn?1:0));const b=document.getElementById('blBtn');b.textContent=blOn?'☀ display on':'○ display off';b.classList.toggle('on',blOn);b.classList.toggle('dim',!blOn);}
async function useSerialMode(){if(!confirm('Switch to serial mode? WiFi provisioning will be skipped and Claude status synced via USB.'))return;await req('/serial_mode');toast('serial mode active');}
async function loadWifiList(){const btn=document.getElementById('wscanBtn');btn.disabled=true;btn.textContent='scanning...';document.getElementById('wstatus').textContent='scanning...';document.getElementById('wlist').style.display='none';document.getElementById('wform').style.display='none';try{const r=await fetch('/wifi/scan');const nets=await r.json();const list=document.getElementById('wlist');list.innerHTML='';if(nets.length===0){document.getElementById('wstatus').textContent='no networks found';}else{nets.sort((a,b)=>b.rssi-a.rssi);nets.forEach(n=>{const btn=document.createElement('button');btn.className='cbtn';btn.style.textAlign='left';btn.style.padding='10px 12px';const sig=n.rssi>-50?'&#128267;':(n.rssi>-70?'&#128266;':'&#128268;');btn.innerHTML='<span style="color:#c96a3e">'+sig+'</span> '+n.ssid+(n.encrypted?' <span style="color:#5a5048">&#128274;</span>':'')+' <span style="color:#5a5048;font-size:9px">'+n.rssi+'dBm</span>';btn.onclick=()=>{document.getElementById('wssid').value=n.ssid;document.getElementById('wpass').focus();};list.appendChild(btn);});document.getElementById('wstatus').textContent='select network or type SSID:';document.getElementById('wlist').style.display='flex';document.getElementById('wform').style.display='flex';}}catch(e){document.getElementById('wstatus').textContent='scan failed';}btn.disabled=false;btn.textContent='🔍 scan networks';}
async function connectWifi(){const ssid=document.getElementById('wssid').value.trim();const pass=document.getElementById('wpass').value;if(!ssid){toast('enter SSID',false);return;}const fd=new FormData();fd.append('ssid',ssid);fd.append('password',pass);document.getElementById('wstatus').textContent='connecting...';try{await fetch('/wifi/connect',{method:'POST',body:fd});}catch(e){}let ok=false;for(let i=0;i<30;i++){await new Promise(r=>setTimeout(r,1000));try{const r=await fetch('/wifi/status');const j=await r.json();if(j.connected){ok=true;break;}document.getElementById('wstatus').textContent='connecting... ('+(i+1)+'s)';}catch(e){}}if(ok){document.getElementById('wstatus').innerHTML='<span style="color:#28b878">✓ connected: '+ssid+'</span>';document.getElementById('wlist').style.display='none';document.getElementById('wform').style.display='none';toast('wifi connected');}else{document.getElementById('wstatus').innerHTML='<span style="color:#c96a3e">connection failed, retry</span>';toast('wifi failed',false);}}
async function pollWifiStatus(){try{const r=await fetch('/wifi/status');const j=await r.json();const el=document.getElementById('wstatus');if(j.connected){el.innerHTML='<span style="color:#28b878">✓ connected: '+j.ssid+' ('+j.ip+')</span>';document.getElementById('wlist').style.display='none';document.getElementById('wform').style.display='none';document.getElementById('wscanBtn').style.display='none';}else{el.textContent='not connected — scan to setup';}}catch(e){document.getElementById('wstatus').textContent='status unavailable';}}
async function toggleCanvas(){canvasOpen=!canvasOpen;document.getElementById('cwrap').classList.toggle('open',canvasOpen);document.querySelectorAll('.vbtn').forEach(btn=>btn.classList.toggle('active',canvasOpen&&parseInt(btn.dataset.v)===3));await req('/canvas?on='+(canvasOpen?1:0));if(canvasOpen){const bg=document.getElementById('bgCol').value;redrawCanvas(bg);await req('/draw/clear?bg='+encodeURIComponent(bg));document.querySelectorAll('.vbtn,.lbtn').forEach(b=>b.disabled=true);toast('canvas active');}else{setBusy(false);toast('canvas off');}}
const tin=document.getElementById('tin');let lastVal='';
tin.addEventListener('input',async()=>{const cur=tin.value,prev=lastVal;if(cur.length>prev.length){await req('/char?c='+encodeURIComponent(cur[cur.length-1]));}else if(cur.length<prev.length){await req('/char?c=%08');}lastVal=cur;});
async function termEnter(){await req('/char?c=%0A');tin.value='';lastVal='';tin.focus();}
tin.addEventListener('keydown',e=>{if(e.key==='Enter'){e.preventDefault();termEnter();}});
async function closeTerm(){await req('/cmd?k=q');termOpen=false;document.getElementById('twrap').classList.remove('open');setBusy(false);toast('terminal closed');}
const cvs=document.getElementById('cvs');const ctx=cvs.getContext('2d');let strokePts=[];   
function getPos(e){const r=cvs.getBoundingClientRect();const sx=cvs.width/r.width,sy=cvs.height/r.height;const s=e.touches?e.touches[0]:e;return{x:(s.clientX-r.left)*sx,y:(s.clientY-r.top)*sy};}
function redrawCanvas(hex){ctx.fillStyle=hex;ctx.fillRect(0,0,cvs.width,cvs.height);}
function startDraw(e){e.preventDefault();drawing=true;strokePts=[];const p=getPos(e);lastX=p.x;lastY=p.y;strokePts.push({x:Math.round(p.x),y:Math.round(p.y)});ctx.beginPath();ctx.arc(p.x,p.y,2,0,Math.PI*2);ctx.fillStyle=document.getElementById('penCol').value;ctx.fill();}
function moveDraw(e){if(!drawing)return;e.preventDefault();const p=getPos(e);ctx.beginPath();ctx.moveTo(lastX,lastY);ctx.lineTo(p.x,p.y);ctx.strokeStyle=document.getElementById('penCol').value;ctx.lineWidth=4;ctx.lineCap='round';ctx.stroke();strokePts.push({x:Math.round(p.x),y:Math.round(p.y)});lastX=p.x;lastY=p.y;}
async function endDraw(e){if(!drawing)return;drawing=false;if(!canvasOpen||strokePts.length<1)return;const pen=document.getElementById('penCol').value.replace('#','');const pts=strokePts.map(p=>p.x+','+p.y).join(';');await req('/draw/stroke?pen='+pen+'&pts='+encodeURIComponent(pts));strokePts=[];}
cvs.addEventListener('mousedown',startDraw);cvs.addEventListener('mousemove',moveDraw);cvs.addEventListener('mouseup',endDraw);cvs.addEventListener('mouseleave',endDraw);
cvs.addEventListener('touchstart',startDraw,{passive:false});cvs.addEventListener('touchmove',moveDraw,{passive:false});cvs.addEventListener('touchend',endDraw);
async function clearAll(){const bg=document.getElementById('bgCol').value;redrawCanvas(bg);await req('/draw/clear?bg='+encodeURIComponent(bg));toast('cleared');}
(async()=>{try{const r=await fetch('/state');const j=await r.json();const spd=j.speed||1;document.getElementById('spd').value=spd;document.getElementById('spdV').textContent=spdLabels[spd];if(j.bl===false){blOn=false;const b=document.getElementById('blBtn');b.textContent='○ display off';b.classList.remove('on');b.classList.add('dim');}}catch(e){}document.getElementById('bgCol').value='#aa4818';redrawCanvas('#aa4818');pollWifiStatus();})();
</script>
</body>
</html>
)rawhtml";

// ── Constructor ────────────────────────────────────────────────
WebService::WebService(ClaudeCodeService* ccService, WifiConfigService* wifiService,
                       TimeService* timeService, DisplayService* displayService)
    : _server(CFG_WIFI_WEB_PORT)
    , _ccService(ccService), _wifiService(wifiService)
    , _timeService(timeService), _displayService(displayService)
{
}

void WebService::init() {
    // SERIAL 模式无 WiFi,不启动 Web 服务器
    auto* opMode = OperationModeService::current();
    if (opMode && opMode->isSerial()) {
        LOG_INFO("Web", "串口模式:跳过 Web 服务器");
        return;
    }
    setupRoutes();
    _server.begin();
    LOG_INFO("Web", "HTTP 服务器启动 端口: %d", CFG_WIFI_WEB_PORT);
}

void WebService::update() {
    auto* opMode = OperationModeService::current();
    if (opMode && opMode->isSerial()) return;
    _server.handleClient();
}

// ── Routes ─────────────────────────────────────────────────────
void WebService::setupRoutes() {
    // Original interactive routes
    _server.on("/",            HTTP_GET, [this]() { handleRoot(); });
    _server.on("/cmd",         HTTP_GET, [this]() { handleCmd(); });
    _server.on("/char",        HTTP_GET, [this]() { handleChar(); });
    _server.on("/speed",       HTTP_GET, [this]() { handleSpeed(); });
    _server.on("/redraw",      HTTP_GET, [this]() { handleRedraw(); });
    _server.on("/canvas",      HTTP_GET, [this]() { handleCanvas(); });
    _server.on("/draw/clear",  HTTP_GET, [this]() { handleDrawClear(); });
    _server.on("/draw/stroke", HTTP_GET, [this]() { handleDrawStroke(); });
    _server.on("/backlight",   HTTP_GET, [this]() { handleBacklight(); });
    _server.on("/state",       HTTP_GET, [this]() { handleState(); });
    _server.on("/serial_mode", HTTP_GET, [this]() { handleSerialMode(); });

    // Existing routes
    _server.on("/wifi_setup", [this]() { handleWifiSetup(); });
    _server.on("/wifi_setup.html", [this]() { handleWifiSetup(); });
    _server.on("/logs", [this]() { handleLogs(); });
    _server.on("/logs.html", [this]() { handleLogs(); });
    _server.on("/cc/status", [this]() { handleCCStatus(); });
    _server.on("/cc/test", [this]() { handleCCTest(); });
    _server.on("/wifi/scan", [this]() { _wifiService->handleScanRequest(_server); });
    _server.on("/wifi/connect", HTTP_POST, [this]() { _wifiService->handleConnectRequest(_server); });
    _server.on("/wifi/status", [this]() { _wifiService->handleStatusRequest(_server); });
    _server.on("/logs/api", [this]() { handleLogsApi(); });
    _server.on("/logs/clear", HTTP_POST, [this]() { handleLogsClear(); });
    _server.on("/logs/status", [this]() { handleLogsStatus(); });
    _server.on("/time", [this]() { handleTime(); });

    // Static files from LittleFS
    _server.serveStatic("/style.css", LittleFS, "/style.css");
    _server.serveStatic("/app.js", LittleFS, "/app.js");
    _server.serveStatic("/claude_code.js", LittleFS, "/claude_code.js");
    _server.serveStatic("/wifi.js", LittleFS, "/wifi.js");

    _server.onNotFound([this]() {
        String path = _server.uri();
        if (LittleFS.exists(path)) {
            handleFile(path.c_str(), getContentType(path).c_str());
        } else {
            _server.send(404, "text/plain", "Not Found");
        }
    });
}

// ── Original interactive handlers ──────────────────────────────
void WebService::handleRoot() {
    _server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    _server.sendHeader("Pragma", "no-cache");
    _server.send_P(200, "text/html", INDEX_HTML);
}

void WebService::handleCmd() {
    if (!_server.hasArg("k") || _server.arg("k").isEmpty()) {
        _server.send(400, "application/json", "{\"e\":1}"); return;
    }
    const char c = _server.arg("k")[0];

    if (_displayService->isTermMode()) {
        if (c == 'q') {
            _displayService->exitTerminal();
        }
        _server.send(200, "application/json", "{\"ok\":1}");
        return;
    }

    _server.send(200, "application/json", "{\"ok\":1}");
    switch (c) {
        case 'w': _displayService->setInteractiveView(VIEW_EYES_NORMAL); break;
        case 's': _displayService->setInteractiveView(VIEW_EYES_SQUISH); break;
        case 'd': _displayService->setInteractiveView(VIEW_CODE); break;
        case 'a': _displayService->animLogoReveal(); break;
    }
}

void WebService::handleChar() {
    if (!_displayService->isTermMode()) {
        _server.send(200, "application/json", "{\"ok\":1}"); return;
    }
    const String val = _server.arg("c");
    if (val.length() > 0) _displayService->termAddChar(val[0]);
    _server.send(200, "application/json", "{\"ok\":1}");
}

void WebService::handleSpeed() {
    if (_server.hasArg("v")) {
        _displayService->setAnimSpeed(constrain(_server.arg("v").toInt(), 1, 3));
    }
    _server.send(200, "application/json", "{\"ok\":1}");
}

void WebService::handleRedraw() {
    if (_server.hasArg("bg")) {
        _displayService->setAnimBgColor(_displayService->hexToRgb565(_server.arg("bg")));
    }
    _displayService->redrawCurrentView();
    _server.send(200, "application/json", "{\"ok\":1}");
}

void WebService::handleCanvas() {
    const bool on = _server.hasArg("on") && _server.arg("on") == "1";
    if (on) {
        _displayService->enterInteractive();
        _displayService->setInteractiveView(VIEW_DRAW);
    }
    _server.send(200, "application/json", "{\"ok\":1}");
}

void WebService::handleDrawClear() {
    const String bg = _server.hasArg("bg") ? _server.arg("bg") : "#aa4818";
    _displayService->drawClear(_displayService->hexToRgb565(bg));
    _server.send(200, "application/json", "{\"ok\":1}");
}

void WebService::handleDrawStroke() {
    if (!_server.hasArg("pts") || !_server.hasArg("pen")) {
        _server.send(200, "application/json", "{\"ok\":1}"); return;
    }
    const uint16_t color = _displayService->hexToRgb565(_server.arg("pen"));
    _displayService->drawStroke(color, _server.arg("pts"));
    _server.send(200, "application/json", "{\"ok\":1}");
}

void WebService::handleBacklight() {
    if (_server.hasArg("on")) {
        digitalWrite(CFG_DISPLAY_PIN_BL, _server.arg("on") == "1" ? HIGH : LOW);
        _server.send(200, "application/json", "{\"ok\":true}");
    } else {
        _server.send(400, "application/json", "{\"error\":\"missing on parameter\"}");
    }
}

void WebService::handleState() {
    String j = "{\"view\":"; j += _displayService->getInteractiveView();
    j += ",\"busy\":";   j += _displayService->isBusy()       ? "true" : "false";
    j += ",\"term\":";   j += _displayService->isTermMode()   ? "true" : "false";
    j += ",\"bl\":";     j += digitalRead(CFG_DISPLAY_PIN_BL) ? "true" : "false";
    j += ",\"speed\":";  j += _displayService->getAnimSpeed();
    j += ",\"serial\":"; j += _wifiService->isSerialMode() ? "true" : "false";
    j += "}";
    _server.send(200, "application/json", j);
}

void WebService::handleSerialMode() {
    _wifiService->skipProvisioning();
    _server.send(200, "application/json", "{\"ok\":true,\"mode\":\"serial\"}");
}

// ── Existing handlers ──────────────────────────────────────────
void WebService::handleWifiSetup() { handleFile("/wifi_setup.html", "text/html"); }
void WebService::handleLogs() { handleFile("/logs.html", "text/html"); }

void WebService::handleFile(const char* path, const char* contentType) {
    File file = LittleFS.open(path, "r");
    if (!file) { _server.send(404, "text/plain", "File not found"); return; }
    _server.streamFile(file, contentType);
    file.close();
}

void WebService::handleCCStatus() { _server.send(200, "application/json", _ccService->getStatusJson()); }
void WebService::handleCCTest() { _server.send(200, "application/json", "{\"status\":\"ok\",\"device\":\"ClawdMochi\"}"); }

void WebService::handleTime() {
    String json = "{\"time\":\"" + _timeService->getDateTime() + "\",\"synced\":" + String(_timeService->isSynced() ? "true" : "false") + "}";
    _server.send(200, "application/json", json);
}

void WebService::handleLogsApi() {
    size_t maxLines = _server.hasArg("lines") ? _server.arg("lines").toInt() : 100;
    _server.send(200, "text/plain", Logger::getInstance().getLogs(maxLines));
}

void WebService::handleLogsClear() {
    Logger::getInstance().clearLogs();
    _server.send(200, "application/json", "{\"ok\":true}");
}

void WebService::handleLogsStatus() {
    _server.send(200, "application/json", "{\"size\":" + String(Logger::getInstance().getLogSize()) + "}");
}

String WebService::getContentType(const String& path) {
    if (path.endsWith(".html")) return "text/html";
    if (path.endsWith(".css")) return "text/css";
    if (path.endsWith(".js")) return "application/javascript";
    if (path.endsWith(".json")) return "application/json";
    if (path.endsWith(".png")) return "image/png";
    if (path.endsWith(".ico")) return "image/x-icon";
    return "text/plain";
}

String WebService::rgb565ToHex(uint16_t c) {
    uint8_t r = ((c >> 11) & 0x1F) << 3;
    uint8_t g = ((c >> 5)  & 0x3F) << 2;
    uint8_t b = (c & 0x1F) << 3;
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
    return String(buf);
}

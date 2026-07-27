#include "web_service.h"
#include "operation_mode_service.h"
#include "../config/cfg_display.h"
#include <ArduinoJson.h>

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
.bright{width:100%;max-width:390px;background:#101014;border:1.5px solid #38343a;
  border-radius:14px;padding:12px;display:flex;align-items:center;gap:10px}
.bicon{font-size:18px;color:#c96a3e;line-height:1;width:20px;text-align:center}
.bright input[type=range]{height:30px;accent-color:#f0a060}
.bval{font-size:12px;color:#c96a3e;font-weight:bold;min-width:40px;text-align:right}
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
.vbtn[data-v="6"].active,.vbtn[data-v="7"].active{border-color:#f0a060;background:#22140a}
  .vbtn[data-v="8"].active,.vbtn[data-v="9"].active,.vbtn[data-v="10"].active{border-color:#f0a060;background:#22140a}
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
.pwrap{width:100%;max-width:390px;background:#201c18;border:1.5px solid #3a3028;
  border-radius:12px;padding:12px;display:none;flex-direction:column;gap:10px}
.pwrap.open{display:flex}
.prow{display:flex;gap:8px}
.pstat{display:flex;justify-content:space-between;align-items:center;
  background:#141210;border:1px solid #332a23;border-radius:8px;padding:10px 12px;
  color:#d8d0c6;font-size:12px;font-weight:bold}
.pstat strong{color:#c96a3e;font-size:18px}
.pnum{width:100%;background:#101014;border:1.5px solid #38343a;border-radius:8px;
  color:#e8e4dc;font-family:'Courier New',monospace;font-size:15px;font-weight:bold;
  padding:10px;text-align:center}
.pbtn{flex:1;background:#1c1820;border:1.5px solid #4a3a2c;border-radius:9px;
  color:#e8e0d6;font-family:'Courier New',monospace;font-size:11px;font-weight:bold;
  padding:11px 4px;cursor:pointer}
.pbtn.hi{background:#c96a3e;border-color:#f0a060;color:#140c08}
.mwrap{width:100%;max-width:390px;background:#151418;border:1.5px solid #3a3028;
  border-radius:14px;padding:14px;display:none;flex-direction:column;gap:12px}
.mwrap.open{display:flex}
.mttl{font-size:19px;color:#f0ece4;font-weight:bold;letter-spacing:1px;
  border-bottom:1px solid #c96a3e;padding-bottom:10px}
.mpreview{width:240px;height:240px;max-width:100%;aspect-ratio:1;margin:0 auto;
  background:#fb6b10;color:#fff;border:2px solid #080604;display:flex;
  flex-direction:column;overflow:hidden;font-weight:900;text-shadow:.6px 0 currentColor}
.mhead{height:33px;flex:none;border-bottom:2px solid #fff0d8;padding:7px 8px 0;
  display:flex;justify-content:space-between;font-size:17px;letter-spacing:1px}
.mtime{font-size:9px;padding-top:4px;letter-spacing:0}
.mrows{display:flex;flex:1;min-height:0;flex-direction:column}
.mrow{flex:1;min-height:0;display:grid;grid-template-columns:54px 1fr 58px;
  align-items:center;padding:0 7px;border-top:1px solid #c74318}
.mrow:first-child{border-top:0}
.msym,.mprice,.mchg{font-size:16px}.mchg{text-align:right}
.mauto{display:flex;justify-content:space-between;align-items:center;font-size:11px;
  color:#6f675f;letter-spacing:1px;padding:2px 1px}
.mauto strong{color:#63c56a;font-weight:bold}
.mlabel{font-size:10px;color:#8a8278;letter-spacing:1.5px;font-weight:bold}
.mlabelrow{display:flex;align-items:center;justify-content:space-between}
.msel{display:flex;flex-direction:column;gap:0;border:1px solid #4b372c;
  border-radius:9px;overflow:hidden}
.mselrow,.mresult{display:flex;align-items:center;gap:8px;background:#222126;
  padding:10px}
.mselrow{border-bottom:1px solid #4b372c;transition:transform .08s,opacity .08s}
.mselrow:last-child{border-bottom:0}.mselrow.dragging{opacity:.75;background:#2d201a;z-index:3}
.mdrag{width:28px;flex:none;background:transparent;border:0;color:#d8d3cb;
  font:700 25px/1 'Courier New',monospace;cursor:grab;touch-action:none;padding:2px}
.mdrag:active{cursor:grabbing}
.mselname,.mresname{min-width:0;flex:1;display:flex;align-items:center;gap:12px}
.mselname strong,.mresname strong{font-size:14px;color:#df6734}
.mselname span,.mresname small{font-size:11px;color:#f0ece4;
  overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.mmini{background:#17161a;border:1px solid #494249;border-radius:6px;color:#bdb6ae;
  font:700 18px 'Courier New',monospace;width:38px;height:38px;cursor:pointer}
.mmini.remove{color:#e96d38;border-color:#9b4b2a}
.msearch{width:100%;background:#0e0e11;border:1.5px solid #494249;border-radius:9px;
  color:#eee8df;font:700 13px 'Courier New',monospace;padding:11px;outline:none}
.msearch:focus{border-color:#c96a3e}
.mresults{display:flex;flex-direction:column;gap:6px;max-height:230px;overflow:auto}
.mresult{border:1px solid #39353b;border-radius:9px}
.madd{background:#c96a3e;border:0;border-radius:7px;color:#140b06;
  font:700 10px 'Courier New',monospace;padding:8px 10px;cursor:pointer}
  .madd:disabled{opacity:.35}
  .minfo{border:1px solid #6d3a25;border-radius:8px;color:#bbb4ac;font-size:10px;
  line-height:1.45;padding:10px 12px}
  .idle-open{width:100%;max-width:390px;display:flex;align-items:center;justify-content:space-between}
  .rwrap{width:100%;max-width:390px;background:#151418;border:1.5px solid #3a3028;
  border-radius:14px;padding:14px;display:none;flex-direction:column;gap:11px}
  .rwrap.open{display:flex}
  .rhead{display:flex;align-items:center;justify-content:space-between;gap:10px}
  .rttl{font-size:14px;color:#f0ece4;font-weight:bold;letter-spacing:1px}
  .rtoggle{flex:none;min-width:118px;padding:10px 8px}
  .rrow{display:flex;align-items:center;gap:10px}
  .rrow .mlabel{min-width:58px}
  .rselect{flex:1;background:#0e0e11;border:1.5px solid #494249;border-radius:8px;
  color:#eee8df;font:700 12px 'Courier New',monospace;padding:10px;outline:none}
  .rselect:disabled,.rrange:disabled{opacity:.35}
  .rrange{flex:1;accent-color:#c96a3e;cursor:pointer}
  .rtime{font-size:12px;color:#df6734;font-weight:bold;min-width:34px;text-align:right}
  .rorder{display:flex;flex-direction:column;border:1px solid #4b372c;border-radius:9px;overflow:hidden}
  .ritem{display:flex;align-items:center;gap:9px;background:#222126;border-bottom:1px solid #4b372c;padding:8px 10px;
  transition:transform .08s,opacity .08s}.ritem:last-child{border-bottom:0}.ritem.dragging{opacity:.75;background:#2d201a;z-index:3}
  .rindex{font-size:10px;color:#746b63;min-width:16px}.rname{flex:1;color:#f0ece4;font-size:12px;font-weight:bold}
  .rdrag:disabled{opacity:.25;cursor:default}
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
  <button class="cbtn on" id="ccStatusBtn" onclick="toggleClaudeStatus()">&#9670; claude status on</button>
</div>
<div class="bright">
  <span class="bicon">&#9728;</span>
  <input type="range" id="bright" min="0" max="100" value="100" step="1" oninput="setBrightness(this.value)">
  <span class="bval" id="brightV">100%</span>
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
  <button class="vbtn" data-v="6" onclick="setView(6)">
    <span class="ic">14:28</span>
    <span class="nm">Clock</span>
    <span class="ht">orange time</span>
  </button>
  <button class="vbtn" data-v="7" onclick="setView(7)">
    <span class="ic">25</span>
    <span class="nm">Pomodoro</span>
    <span class="ht">focus timer</span>
  </button>
  <button class="vbtn" data-v="8" onclick="setView(8)">
    <span class="ic">31&#176; / CLOUD</span>
    <span class="nm">Weather</span>
    <span class="ht">automatic IP location</span>
  </button>
  <button class="vbtn" data-v="9" onclick="setView(9)">
    <span class="ic">$ BTC</span>
    <span class="nm">Crypto</span>
    <span class="ht">1-5 live assets</span>
  </button>
  <button class="vbtn" data-v="10" onclick="setView(10)">
    <span class="ic">SH  SZ</span>
    <span class="nm">Market</span>
    <span class="ht">China stocks & indices</span>
  </button>
</div>
<div class="sec">// idle display</div>
<button class="cbtn idle-open" id="carouselPanelBtn" onclick="toggleCarouselPanel()"><span>↻ idle display settings</span><span id="carouselPanelState">open</span></button>
<div class="rwrap" id="carouselWrap">
  <div class="rhead"><span class="rttl">INFO CAROUSEL</span><button class="cbtn rtoggle" id="carouselToggle" onclick="toggleCarousel()">○ carousel off</button></div>
  <div class="rrow"><span class="mlabel">FIXED PAGE</span><select class="rselect" id="carouselFixed" onchange="setCarouselFixed(this.value)"><option value="8">Weather</option><option value="9">Crypto</option><option value="10">Market</option></select></div>
  <div class="rrow"><span class="mlabel">INTERVAL</span><input class="rrange" id="carouselSpeed" type="range" min="5" max="60" step="1" oninput="setCarouselSpeed(this.value)"><span class="rtime" id="carouselSpeedV">12s</span></div>
  <div class="mlabel">ROTATION ORDER</div>
  <div class="rorder" id="carouselOrder"></div>
  <div class="minfo" id="carouselHint">When Claude Code becomes active, the carousel pauses and resumes from the same page afterwards.</div>
</div>
<div class="pwrap" id="pwrap">
  <div class="pstat"><span id="pPhase">FOCUS</span><strong id="pTime">25:00</strong></div>
  <div class="prow">
    <button class="pbtn hi" onclick="startTimer('focus')">start focus</button>
    <button class="pbtn" onclick="startTimer('break')">start break</button>
  </div>
  <div class="prow">
    <button class="pbtn" onclick="pauseTimer()">pause / resume</button>
    <button class="pbtn" onclick="resetTimer()">reset</button>
  </div>
  <div class="prow">
    <div class="ci" style="align-items:stretch">
      <span class="cl" style="text-align:center">FOCUS MIN</span>
      <input class="pnum" id="focusMin" type="number" min="1" max="180" value="25" onchange="configTimer()">
    </div>
    <div class="ci" style="align-items:stretch">
      <span class="cl" style="text-align:center">BREAK MIN</span>
      <input class="pnum" id="breakMin" type="number" min="1" max="60" value="5" onchange="configTimer()">
    </div>
  </div>
</div>
<div class="mwrap" id="mwrap">
  <div class="mttl">// crypto display</div>
  <div class="mlabel">LIVE PREVIEW (240x240)</div>
  <div class="mpreview">
    <div class="mhead"><span>CRYPTO</span><span class="mtime" id="mTime">UPDATED --:--</span></div>
    <div class="mrows" id="mPreview"></div>
  </div>
  <div class="mlabel">DISPLAYED <span id="mCount">0 / 5</span></div>
  <div class="msel" id="mSelected"></div>
  <div class="mlabel">SEARCH COINS OR SYMBOLS</div>
  <input class="msearch" id="mSearch" type="search" placeholder="BTC, Ethereum, Solana..."
         autocomplete="off" autocorrect="off" autocapitalize="off" spellcheck="false">
  <div class="mlabelrow"><span class="mlabel">SEARCH RESULTS</span><span class="mlabel" id="mResultCount"></span></div>
  <div class="mresults" id="mResults"></div>
  <div class="minfo">Remove an item above to enable Add.<br>Max 5 assets shown on device.</div>
  <div class="mauto"><strong id="mAuto">● SAVED TO DEVICE</strong><span>AUTO-SAVED</span></div>
</div>
<div class="mwrap" id="swrap">
  <div class="mttl">// market display</div>
  <div class="mlabel">LIVE PREVIEW (240x240)</div>
  <div class="mpreview">
    <div class="mhead"><span>MARKET</span><span class="mtime" id="sTime">UPDATED --:--</span></div>
    <div class="mrows" id="sPreview"></div>
  </div>
  <div class="mlabel">DISPLAYED <span id="sCount">0 / 5</span></div>
  <div class="msel" id="sSelected"></div>
  <div class="mlabel">SEARCH STOCK CODE OR NAME</div>
  <input class="msearch" id="sSearch" type="search" placeholder="600519, 贵州茅台..."
         autocomplete="off" autocorrect="off" autocapitalize="off" spellcheck="false">
  <div class="mlabelrow"><span class="mlabel">SEARCH RESULTS</span><span class="mlabel" id="sResultCount"></span></div>
  <div class="mresults" id="sResults"></div>
  <div class="minfo">The Market title stays fixed.<br>Replace the default indices with up to 5 A-share stocks.</div>
  <div class="mauto"><strong id="sAuto">● SAVED TO DEVICE</strong><span>AUTO-SAVED</span></div>
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
let activeView=0,termOpen=false,canvasOpen=false,blOn=true,claudeStatusOn=true,isBusy=false,drawing=false;
let lastX=0,lastY=0,tt;
let marketSelected=[],marketDirectory=[],marketLoaded=false,marketSaving=false,marketSaveQueued=false,marketUpdatedAt=null,marketDrag=null;
let stockSelected=[],stockSaving=false,stockSaveQueued=false,stockUpdatedAt=null,stockDrag=null,stockSearchTimer=null,stockSearchSeq=0;
let carouselConfig={enabled:false,speed:12,order:[8,9,10],fixed:8},carouselSpeedTimer=null,carouselPanelOpen=false,carouselDrag=null;
const spdLabels=['','slow','normal','fast'];
function toast(msg,ok=true){const el=document.getElementById('toast');el.textContent=msg;el.style.borderColor=ok?'#28b878':'#c96a3e';el.classList.add('show');clearTimeout(tt);tt=setTimeout(()=>el.classList.remove('show'),1300);}
function setBusy(b){isBusy=b;document.getElementById('busy').classList.toggle('show',b);const locked=b||termOpen;document.querySelectorAll('.vbtn').forEach(el=>{el.disabled=canvasOpen?parseInt(el.dataset.v)!==3:locked;});document.querySelectorAll('.cbtn').forEach(el=>{if(el.id!=='blBtn'&&el.id!=='ccStatusBtn')el.disabled=locked;});}
async function req(path){try{const r=await fetch(path);return r.ok;}catch(e){toast('no connection',false);return false;}}
async function waitNotBusy(){for(let i=0;i<100;i++){try{const r=await fetch('/state');const j=await r.json();if(!j.busy)return;}catch(e){}await new Promise(r=>setTimeout(r,150));}}
async function onBgChange(hex){if(canvasOpen){await req('/draw/clear?bg='+encodeURIComponent(hex));await req('/prefs?bg='+encodeURIComponent(hex));}else{await req('/redraw?bg='+encodeURIComponent(hex));}redrawCanvas(hex);}
async function setSpeed(v){document.getElementById('spdV').textContent=spdLabels[v];await req('/speed?v='+v);}
async function setView(v){if(isBusy||termOpen||canvasOpen)return;if(v===3){toggleCanvas();return;}const keys={0:'w',1:'s',2:'d',6:'c',7:'p',8:'e',9:'m',10:'k'};if(!await req('/cmd?k='+keys[v]))return;activeView=v;document.querySelectorAll('.vbtn').forEach(b=>b.classList.toggle('active',parseInt(b.dataset.v)===v));document.getElementById('pwrap').classList.toggle('open',v===7);document.getElementById('mwrap').classList.toggle('open',v===9);document.getElementById('swrap').classList.toggle('open',v===10);if(v===9){await loadMarketConfig();loadMarketDirectory();toast('crypto open');return;}if(v===10){await loadStockConfig();toast('market open');return;}if(v===2){termOpen=true;document.getElementById('twrap').classList.add('open');setBusy(false);setBusy(false);document.querySelectorAll('.vbtn,.lbtn').forEach(b=>b.disabled=true);document.getElementById('tin').focus();toast('terminal open');return;}if(v===6||v===7||v===8){toast(v===6?'clock open':(v===7?'pomodoro open':'locating weather'));return;}setBusy(true);await waitNotBusy();setBusy(false);}
function updateBlButton(){const b=document.getElementById('blBtn');b.textContent=blOn?'☀ display on':'○ display off';b.classList.toggle('on',blOn);b.classList.toggle('dim',!blOn);}
function updateClaudeStatusButton(){const b=document.getElementById('ccStatusBtn');b.textContent=claudeStatusOn?'◆ claude status on':'◇ claude status off';b.classList.toggle('on',claudeStatusOn);b.classList.toggle('dim',!claudeStatusOn);}
function carouselName(v){return({8:'Weather',9:'Crypto',10:'Market'})[v]||'Weather';}
function applyCarouselPrefs(p){
  carouselConfig.enabled=p.carousel===true;carouselConfig.speed=Math.max(5,Math.min(60,Number(p.carouselSpeed)||12));
  carouselConfig.order=Array.isArray(p.carouselOrder)&&p.carouselOrder.length===3?p.carouselOrder.map(Number):[8,9,10];
  carouselConfig.fixed=[8,9,10].includes(Number(p.carouselFixed))?Number(p.carouselFixed):8;renderCarousel();
}
function renderCarousel(){
  const enabled=carouselConfig.enabled,toggle=document.getElementById('carouselToggle'),fixed=document.getElementById('carouselFixed'),speed=document.getElementById('carouselSpeed');
  toggle.textContent=enabled?'● carousel on':'○ carousel off';toggle.classList.toggle('on',enabled);toggle.classList.toggle('dim',!enabled);
  fixed.value=String(carouselConfig.fixed);fixed.disabled=enabled;speed.value=String(carouselConfig.speed);speed.disabled=!enabled;document.getElementById('carouselSpeedV').textContent=carouselConfig.speed+'s';
  document.getElementById('carouselHint').textContent=enabled?'Claude Code pauses the carousel, then it resumes from the interrupted page.':'Select the single info page shown while the carousel is off.';
  const out=document.getElementById('carouselOrder');out.innerHTML='';carouselConfig.order.forEach((view,i)=>{
    const row=document.createElement('div');row.className='ritem';
    const index=document.createElement('span');index.className='rindex';index.textContent=String(i+1).padStart(2,'0');
    const name=document.createElement('span');name.className='rname';name.textContent=carouselName(view);
    const handle=document.createElement('button');handle.className='mdrag rdrag';handle.type='button';handle.textContent='⠿';handle.disabled=!enabled;handle.setAttribute('aria-label','Drag '+carouselName(view));
    handle.addEventListener('pointerdown',e=>startCarouselDrag(e,i,row));
    row.append(index,name,handle);out.appendChild(row);
  });
}
function toggleCarouselPanel(){carouselPanelOpen=!carouselPanelOpen;const panel=document.getElementById('carouselWrap'),state=document.getElementById('carouselPanelState');panel.classList.toggle('open',carouselPanelOpen);state.textContent=carouselPanelOpen?'close':'open';}
async function saveCarousel(){
  const q=new URLSearchParams({carousel:carouselConfig.enabled?'1':'0',carouselSpeed:String(carouselConfig.speed),carouselFixed:String(carouselConfig.fixed),carouselOrder:carouselConfig.order.join(',')});
  try{const r=await fetch('/prefs?'+q.toString(),{cache:'no-store'});if(!r.ok)throw new Error();const p=await r.json();if(typeof p.carousel==='boolean')applyCarouselPrefs(p);toast('idle display saved');}
  catch(e){toast('idle display save failed',false);}
}
function toggleCarousel(){carouselConfig.enabled=!carouselConfig.enabled;renderCarousel();saveCarousel();}
function setCarouselFixed(value){carouselConfig.fixed=Number(value);saveCarousel();}
function setCarouselSpeed(value){carouselConfig.speed=Number(value);document.getElementById('carouselSpeedV').textContent=carouselConfig.speed+'s';clearTimeout(carouselSpeedTimer);carouselSpeedTimer=setTimeout(saveCarousel,260);}
function startCarouselDrag(e,index,row){
  if(e.button!==undefined&&e.button!==0||!carouselConfig.enabled)return;e.preventDefault();
  const rows=[...document.querySelectorAll('#carouselOrder .ritem')],box=document.getElementById('carouselOrder').getBoundingClientRect();
  carouselDrag={index,target:index,row,startY:e.clientY,box,rowH:box.height/rows.length};row.classList.add('dragging');e.currentTarget.setPointerCapture(e.pointerId);
  document.addEventListener('pointermove',moveCarouselDrag,{passive:false});document.addEventListener('pointerup',endCarouselDrag,{once:true});
}
function moveCarouselDrag(e){if(!carouselDrag)return;e.preventDefault();const delta=e.clientY-carouselDrag.startY;carouselDrag.row.style.transform='translateY('+delta+'px)';carouselDrag.target=Math.max(0,Math.min(carouselConfig.order.length-1,Math.floor((e.clientY-carouselDrag.box.top)/carouselDrag.rowH)));}
function endCarouselDrag(){document.removeEventListener('pointermove',moveCarouselDrag);if(!carouselDrag)return;const from=carouselDrag.index,to=carouselDrag.target;carouselDrag.row.classList.remove('dragging');carouselDrag.row.style.transform='';carouselDrag=null;if(from===to)return;const view=carouselConfig.order.splice(from,1)[0];carouselConfig.order.splice(to,0,view);renderCarousel();saveCarousel();}
async function toggleBL(){blOn=!blOn;const v=blOn?100:0;document.getElementById('bright').value=v;document.getElementById('brightV').textContent=v+'%';await req('/backlight?on='+(blOn?1:0));updateBlButton();}
async function toggleClaudeStatus(){const next=!claudeStatusOn;if(!await req('/prefs?claudeStatus='+(next?'1':'0')))return;claudeStatusOn=next;updateClaudeStatusButton();toast(claudeStatusOn?'Claude status on':'Claude status off');}
async function setBrightness(v){v=parseInt(v||0);document.getElementById('brightV').textContent=v+'%';blOn=v>0;updateBlButton();await req('/brightness?v='+v);}
async function loadPrefs(){try{const r=await fetch('/prefs');const p=await r.json();document.getElementById('bgCol').value=p.bg||'#aa4818';document.getElementById('spd').value=p.speed||1;document.getElementById('spdV').textContent=spdLabels[p.speed||1];claudeStatusOn=p.claudeStatus!==false;updateClaudeStatusButton();applyCarouselPrefs(p);redrawCanvas(p.bg||'#aa4818');}catch(e){renderCarousel();}}
function fmtSec(s){s=Math.max(0,parseInt(s||0));return String(Math.floor(s/60)).padStart(2,'0')+':'+String(s%60).padStart(2,'0');}
async function pollTimer(){try{const r=await fetch('/timer/status');const j=await r.json();document.getElementById('pPhase').textContent=(j.phase==='break'?'BREAK':'FOCUS')+(j.paused?' / PAUSED':'');document.getElementById('pTime').textContent=fmtSec(j.remaining);document.getElementById('focusMin').value=j.focus;document.getElementById('breakMin').value=j.break;}catch(e){}}
async function startTimer(phase){await fetch('/timer/start?phase='+phase);document.getElementById('pwrap').classList.add('open');await pollTimer();toast(phase==='break'?'break started':'focus started');}
async function pauseTimer(){await fetch('/timer/pause');await pollTimer();toast('timer toggled');}
async function resetTimer(){await fetch('/timer/reset');await pollTimer();toast('timer reset');}
async function configTimer(){const f=document.getElementById('focusMin').value||25,b=document.getElementById('breakMin').value||5;await fetch('/timer/config?focus='+encodeURIComponent(f)+'&break='+encodeURIComponent(b));await pollTimer();}
function marketPrice(v){if(v==null)return'--';if(v>=1000000)return'$'+(v/1000000).toFixed(2)+'M';if(v>=100000)return'$'+Math.round(v/1000)+'K';if(v>=10000)return'$'+(v/1000).toFixed(1)+'K';if(v>=1000)return'$'+Math.round(v);if(v>=100)return'$'+v.toFixed(1);if(v>=1)return'$'+v.toFixed(2);if(v>=.01)return'$'+v.toFixed(4);return'$'+v.toFixed(6);}
function marketChange(v){return v==null?'--':(v>=0?'+':'')+Number(v).toFixed(1)+'%';}
function marketTime(){if(!marketUpdatedAt)return'UPDATED --:--';return'UPDATED '+marketUpdatedAt.toLocaleTimeString([],{hour:'2-digit',minute:'2-digit',hour12:false});}
function renderMarket(){
  document.getElementById('mCount').textContent=marketSelected.length+' / 5';
  document.getElementById('mTime').textContent=marketTime();
  const preview=document.getElementById('mPreview');preview.innerHTML='';
  const selected=document.getElementById('mSelected');selected.innerHTML='';
  marketSelected.forEach((a,i)=>{
    const row=document.createElement('div');row.className='mrow';
    ['msym','mprice','mchg'].forEach((cls,n)=>{const s=document.createElement('span');s.className=cls;s.textContent=n===0?a.symbol:(n===1?marketPrice(a.price):marketChange(a.change));row.appendChild(s);});
    preview.appendChild(row);
    const item=document.createElement('div');item.className='mselrow';item.dataset.index=i;
    const handle=document.createElement('button');handle.className='mdrag';handle.type='button';handle.textContent='⠿';handle.setAttribute('aria-label','Drag '+a.symbol);
    handle.addEventListener('pointerdown',e=>startMarketDrag(e,i,item));item.appendChild(handle);
    const name=document.createElement('div');name.className='mselname';
    const strong=document.createElement('strong');strong.textContent=a.symbol;
    const label=document.createElement('span');label.textContent=a.name+(a.gold?' (troy oz)':'');
    name.append(strong,label);item.appendChild(name);
    const remove=document.createElement('button');remove.className='mmini remove';remove.textContent='×';remove.setAttribute('aria-label','Remove '+a.symbol);remove.onclick=()=>removeMarket(i);item.appendChild(remove);
    selected.appendChild(item);
  });
  renderMarketResults();
}
async function loadMarketConfig(){
  try{const r=await fetch('/crypto/config',{cache:'no-store'});const j=await r.json();marketSelected=Array.isArray(j.assets)?j.assets:[];marketUpdatedAt=typeof j.updatedAgeSec==='number'?new Date(Date.now()-j.updatedAgeSec*1000):null;renderMarket();}
  catch(e){toast('market config unavailable',false);}
}
async function loadMarketDirectory(){
  if(marketLoaded)return;
  const out=document.getElementById('mResults');out.textContent='loading public asset directory...';
  try{
    const r=await fetch('https://api.coinlore.net/api/assets/');
    const j=await r.json();const list=Array.isArray(j)?j:(j.data||j.assets||[]);
    marketDirectory=list.map(a=>({id:String(a.id),symbol:String(a.symbol||'').toUpperCase(),name:String(a.name||a.nameid||''),rank:Number(a.rank)||999999,gold:false}));
    marketLoaded=true;renderMarketResults();
  }catch(e){out.textContent='asset search needs internet access';}
}
function renderMarketResults(){
  const out=document.getElementById('mResults');if(!marketLoaded){return;}
  const count=document.getElementById('mResultCount');const q=document.getElementById('mSearch').value.trim().toLowerCase();out.innerHTML='';
  if(!q){count.textContent='';out.textContent='type a symbol or asset name';return;}
  const terms=q.split(/[\s,;|/]+/).filter(Boolean),compact=q.replace(/[^a-z0-9]/g,'');
  const selectedIds=new Set(marketSelected.map(a=>(a.gold?'gold:':'coin:')+a.id));
  function matchScore(a){
    const symbol=a.symbol.toLowerCase(),name=a.name.toLowerCase();
    let best=999;
    terms.forEach(term=>{
      if(symbol===term)best=Math.min(best,0);
      else if(symbol.startsWith(term))best=Math.min(best,10);
      else if(name.split(/[\s-]+/).some(word=>word.startsWith(term)))best=Math.min(best,20);
      else if(name.startsWith(term))best=Math.min(best,25);
      else if(symbol.includes(term))best=Math.min(best,30);
      else if(name.includes(term))best=Math.min(best,40);
    });
    if(symbol.length>=2&&compact.includes(symbol))best=Math.min(best,50);
    return best;
  }
  const matches=marketDirectory.map(a=>({a,score:matchScore(a)})).filter(x=>x.score<999)
    .sort((x,y)=>x.score-y.score||x.a.rank-y.a.rank||x.a.symbol.localeCompare(y.a.symbol))
    .slice(0,10).map(x=>x.a);
  count.textContent=matches.length+' result'+(matches.length===1?'':'s');
  if(!matches.length){out.textContent='no matching assets';return;}
  matches.forEach(a=>{
    const row=document.createElement('div');row.className='mresult';
    const name=document.createElement('div');name.className='mresname';
    const strong=document.createElement('strong');strong.textContent=a.symbol;
    const small=document.createElement('small');small.textContent=a.name+(a.gold?' · spot gold':' · CoinLore #'+a.id);
    name.append(strong,small);row.appendChild(name);
    const add=document.createElement('button');add.className='madd';add.textContent=selectedIds.has((a.gold?'gold:':'coin:')+a.id)?'ADDED':'ADD';
    add.disabled=marketSelected.length>=5||selectedIds.has((a.gold?'gold:':'coin:')+a.id);add.onclick=()=>addMarket(a);row.appendChild(add);out.appendChild(row);
  });
}
async function saveMarket(){
  if(marketSaving){marketSaveQueued=true;return;}marketSaving=true;document.getElementById('mAuto').textContent='● SAVING...';
  try{const r=await fetch('/crypto/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({assets:marketSelected.map(a=>({id:a.id,symbol:a.symbol,name:a.name,gold:!!a.gold}))})});if(!r.ok)throw new Error();const j=await r.json();marketSelected=j.assets||marketSelected;document.getElementById('mAuto').textContent='● SAVED TO DEVICE';renderMarket();}
  catch(e){document.getElementById('mAuto').textContent='● SAVE FAILED';toast('save failed',false);}
  marketSaving=false;if(marketSaveQueued){marketSaveQueued=false;saveMarket();}
}
function addMarket(a){if(marketSelected.length>=5)return;marketSelected.push({...a,price:null,change:null});renderMarket();saveMarket();}
function removeMarket(i){if(marketSelected.length<=1){toast('keep at least one asset',false);return;}marketSelected.splice(i,1);renderMarket();saveMarket();}
function startMarketDrag(e,index,row){
  if(e.button!==undefined&&e.button!==0)return;e.preventDefault();
  const rows=[...document.querySelectorAll('#mSelected .mselrow')],box=document.getElementById('mSelected').getBoundingClientRect();
  marketDrag={index,target:index,row,startY:e.clientY,box,rowH:box.height/rows.length};
  row.classList.add('dragging');e.currentTarget.setPointerCapture(e.pointerId);
  document.addEventListener('pointermove',moveMarketDrag,{passive:false});document.addEventListener('pointerup',endMarketDrag,{once:true});
}
function moveMarketDrag(e){
  if(!marketDrag)return;e.preventDefault();const delta=e.clientY-marketDrag.startY;
  marketDrag.row.style.transform='translateY('+delta+'px)';
  marketDrag.target=Math.max(0,Math.min(marketSelected.length-1,Math.floor((e.clientY-marketDrag.box.top)/marketDrag.rowH)));
}
function endMarketDrag(){
  document.removeEventListener('pointermove',moveMarketDrag);
  if(!marketDrag)return;const from=marketDrag.index,to=marketDrag.target;marketDrag.row.classList.remove('dragging');marketDrag.row.style.transform='';
  marketDrag=null;if(from===to)return;const asset=marketSelected.splice(from,1)[0];marketSelected.splice(to,0,asset);renderMarket();saveMarket();
}
document.getElementById('mSearch').addEventListener('input',renderMarketResults);
function stockPrice(v){if(v==null)return'--';if(v>=10000)return Math.round(v).toString();if(v>=1000)return Number(v).toFixed(1);return Number(v).toFixed(2);}
function stockTime(){if(!stockUpdatedAt)return'UPDATED --:--';return'UPDATED '+stockUpdatedAt.toLocaleTimeString([],{hour:'2-digit',minute:'2-digit',hour12:false});}
function renderStocks(){
  document.getElementById('sCount').textContent=stockSelected.length+' / 5';
  document.getElementById('sTime').textContent=stockTime();
  const preview=document.getElementById('sPreview');preview.innerHTML='';
  const selected=document.getElementById('sSelected');selected.innerHTML='';
  stockSelected.forEach((a,i)=>{
    const row=document.createElement('div');row.className='mrow';
    const sym=document.createElement('span');sym.className='msym';sym.textContent=a.label||a.code;if(sym.textContent.length>4)sym.style.fontSize='11px';row.appendChild(sym);
    const price=document.createElement('span');price.className='mprice';price.textContent=stockPrice(a.price);row.appendChild(price);
    const change=document.createElement('span');change.className='mchg';change.textContent=marketChange(a.change);row.appendChild(change);preview.appendChild(row);
    const item=document.createElement('div');item.className='mselrow';item.dataset.index=i;
    const handle=document.createElement('button');handle.className='mdrag';handle.type='button';handle.textContent='⠿';handle.setAttribute('aria-label','Drag '+a.code);
    handle.addEventListener('pointerdown',e=>startStockDrag(e,i,item));item.appendChild(handle);
    const name=document.createElement('div');name.className='mselname';
    const strong=document.createElement('strong');strong.textContent=a.code;
    const label=document.createElement('span');label.textContent=a.name;
    name.append(strong,label);item.appendChild(name);
    const remove=document.createElement('button');remove.className='mmini remove';remove.textContent='×';remove.setAttribute('aria-label','Remove '+a.code);remove.onclick=()=>removeStock(i);item.appendChild(remove);
    selected.appendChild(item);
  });
}
async function loadStockConfig(){
  try{const r=await fetch('/market/config',{cache:'no-store'});const j=await r.json();stockSelected=Array.isArray(j.assets)?j.assets:[];stockUpdatedAt=typeof j.updatedAgeSec==='number'?new Date(Date.now()-j.updatedAgeSec*1000):null;renderStocks();}
  catch(e){toast('market config unavailable',false);}
}
async function searchStocksNow(){
  const q=document.getElementById('sSearch').value.trim(),out=document.getElementById('sResults'),count=document.getElementById('sResultCount'),seq=++stockSearchSeq;
  out.innerHTML='';count.textContent='';
  if(!q){out.textContent='type a stock code or Chinese name';return;}
  out.textContent='searching...';
  try{
    const r=await fetch('/market/search?q='+encodeURIComponent(q),{cache:'no-store'});if(!r.ok)throw new Error();const j=await r.json();if(seq!==stockSearchSeq)return;
    const results=Array.isArray(j.results)?j.results:[],selectedIds=new Set(stockSelected.map(a=>a.secid));
    out.innerHTML='';count.textContent=results.length+' result'+(results.length===1?'':'s');
    if(!results.length){out.textContent='no matching A-share stocks';return;}
    results.forEach(a=>{
      const row=document.createElement('div');row.className='mresult';
      const name=document.createElement('div');name.className='mresname';
      const strong=document.createElement('strong');strong.textContent=a.code;
      const small=document.createElement('small');small.textContent=a.name;
      name.append(strong,small);row.appendChild(name);
      const add=document.createElement('button');add.className='madd';add.textContent=selectedIds.has(a.secid)?'ADDED':'ADD';
      add.disabled=stockSelected.length>=5||selectedIds.has(a.secid);add.onclick=()=>addStock(a);row.appendChild(add);out.appendChild(row);
    });
  }catch(e){if(seq===stockSearchSeq){count.textContent='';out.textContent='stock search unavailable';}}
}
document.getElementById('sSearch').addEventListener('input',()=>{clearTimeout(stockSearchTimer);stockSearchTimer=setTimeout(searchStocksNow,280);});
async function saveStocks(){
  if(stockSaving){stockSaveQueued=true;return;}stockSaving=true;document.getElementById('sAuto').textContent='● SAVING...';
  try{const r=await fetch('/market/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({assets:stockSelected.map(a=>({secid:a.secid,code:a.code,label:a.label||a.code,name:a.name}))})});if(!r.ok)throw new Error();const j=await r.json();stockSelected=j.assets||stockSelected;document.getElementById('sAuto').textContent='● SAVED TO DEVICE';renderStocks();}
  catch(e){document.getElementById('sAuto').textContent='● SAVE FAILED';toast('save failed',false);}
  stockSaving=false;if(stockSaveQueued){stockSaveQueued=false;saveStocks();}
}
function addStock(a){if(stockSelected.length>=5)return;stockSelected.push({...a,price:null,change:null});renderStocks();searchStocksNow();saveStocks();}
function removeStock(i){if(stockSelected.length<=1){toast('keep at least one stock',false);return;}stockSelected.splice(i,1);renderStocks();searchStocksNow();saveStocks();}
function startStockDrag(e,index,row){
  if(e.button!==undefined&&e.button!==0)return;e.preventDefault();
  const rows=[...document.querySelectorAll('#sSelected .mselrow')],box=document.getElementById('sSelected').getBoundingClientRect();
  stockDrag={index,target:index,row,startY:e.clientY,box,rowH:box.height/rows.length};
  row.classList.add('dragging');e.currentTarget.setPointerCapture(e.pointerId);
  document.addEventListener('pointermove',moveStockDrag,{passive:false});document.addEventListener('pointerup',endStockDrag,{once:true});
}
function moveStockDrag(e){
  if(!stockDrag)return;e.preventDefault();const delta=e.clientY-stockDrag.startY;
  stockDrag.row.style.transform='translateY('+delta+'px)';
  stockDrag.target=Math.max(0,Math.min(stockSelected.length-1,Math.floor((e.clientY-stockDrag.box.top)/stockDrag.rowH)));
}
function endStockDrag(){
  document.removeEventListener('pointermove',moveStockDrag);
  if(!stockDrag)return;const from=stockDrag.index,to=stockDrag.target;stockDrag.row.classList.remove('dragging');stockDrag.row.style.transform='';
  stockDrag=null;if(from===to)return;const asset=stockSelected.splice(from,1)[0];stockSelected.splice(to,0,asset);renderStocks();saveStocks();
}
async function useSerialMode(){if(!confirm('Switch Claude status input to USB serial? Local Web control stays available.'))return;await req('/serial_mode');toast('serial mode active');}
async function loadWifiList(){const btn=document.getElementById('wscanBtn');btn.disabled=true;btn.textContent='scanning...';document.getElementById('wstatus').textContent='scanning...';document.getElementById('wlist').style.display='none';document.getElementById('wform').style.display='none';try{const r=await fetch('/wifi/scan');const nets=await r.json();const list=document.getElementById('wlist');list.innerHTML='';if(nets.length===0){document.getElementById('wstatus').textContent='no networks found';}else{nets.sort((a,b)=>b.rssi-a.rssi);nets.forEach(n=>{const btn=document.createElement('button');btn.className='cbtn';btn.style.textAlign='left';btn.style.padding='10px 12px';const sig=n.rssi>-50?'&#128267;':(n.rssi>-70?'&#128266;':'&#128268;');btn.innerHTML='<span style="color:#c96a3e">'+sig+'</span> '+n.ssid+(n.encrypted?' <span style="color:#5a5048">&#128274;</span>':'')+' <span style="color:#5a5048;font-size:9px">'+n.rssi+'dBm</span>';btn.onclick=()=>{document.getElementById('wssid').value=n.ssid;document.getElementById('wpass').focus();};list.appendChild(btn);});document.getElementById('wstatus').textContent='select network or type SSID:';document.getElementById('wlist').style.display='flex';document.getElementById('wform').style.display='flex';}}catch(e){document.getElementById('wstatus').textContent='scan failed';}btn.disabled=false;btn.textContent='🔍 scan networks';}
function wifiStatusHtml(j){const ap='http://'+(j.apIp||'192.168.4.1');const mdns=j.mdns||'http://clawd-mochi.local';const line='font-size:10px;color:#8a8278;margin-top:3px';if(j.connected){const lan='http://'+(j.lanIp||j.ip);return '<span style="color:#28b878">connected: '+j.ssid+'</span><div style="'+line+'">LAN: '+lan+'</div><div style="'+line+'">Name: '+mdns+'</div><div style="'+line+'">AP: '+ap+' ('+(j.apSsid||'ClaWD-Mochi')+')</div>';}if(j.configured&&j.savedSsid){return '<span style="color:#c96a3e">saved: '+j.savedSsid+'</span><div style="'+line+'">not connected - AP still works: '+ap+'</div>';}return 'not connected - scan to setup<div style="'+line+'">AP: '+ap+' ('+(j.apSsid||'ClaWD-Mochi')+')</div>';}
async function connectWifi(){const ssid=document.getElementById('wssid').value.trim();const pass=document.getElementById('wpass').value;if(!ssid){toast('enter SSID',false);return;}const fd=new FormData();fd.append('ssid',ssid);fd.append('password',pass);document.getElementById('wstatus').textContent='connecting...';try{await fetch('/wifi/connect',{method:'POST',body:fd});}catch(e){}let ok=false;for(let i=0;i<30;i++){await new Promise(r=>setTimeout(r,1000));try{const r=await fetch('/wifi/status');const j=await r.json();if(j.connected){ok=true;break;}document.getElementById('wstatus').textContent='connecting... ('+(i+1)+'s)';}catch(e){}}if(ok){await pollWifiStatus();document.getElementById('wlist').style.display='none';document.getElementById('wform').style.display='none';toast('wifi connected');}else{document.getElementById('wstatus').innerHTML='<span style="color:#c96a3e">connection failed, retry</span>';toast('wifi failed',false);}}
async function pollWifiStatus(){try{const r=await fetch('/wifi/status');const j=await r.json();const el=document.getElementById('wstatus');el.innerHTML=wifiStatusHtml(j);if(j.connected){document.getElementById('wlist').style.display='none';document.getElementById('wform').style.display='none';document.getElementById('wscanBtn').style.display='none';}}catch(e){document.getElementById('wstatus').textContent='status unavailable';}}
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
(async()=>{await loadPrefs();try{const r=await fetch('/state');const j=await r.json();const bv=typeof j.brightness==='number'?j.brightness:(j.bl===false?0:100);document.getElementById('bright').value=bv;document.getElementById('brightV').textContent=bv+'%';blOn=bv>0;updateBlButton();}catch(e){}pollWifiStatus();pollTimer();setInterval(pollTimer,1000);setInterval(()=>{if(activeView===9&&!marketSaving&&!marketDrag)loadMarketConfig();if(activeView===10&&!stockSaving&&!stockDrag)loadStockConfig();},30000);})();
</script>
</body>
</html>
)rawhtml";

// ── Constructor ────────────────────────────────────────────────
WebService::WebService(ClaudeCodeService* ccService, WifiConfigService* wifiService,
                       TimeService* timeService, DisplayService* displayService,
                       PreferenceService* preferenceService,
                       CryptoService* cryptoService,
                       MarketService* marketService)
    : _server(CFG_WIFI_WEB_PORT)
    , _started(false)
    , _ccService(ccService), _wifiService(wifiService)
    , _timeService(timeService), _displayService(displayService)
    , _preferenceService(preferenceService)
    , _cryptoService(cryptoService)
    , _marketService(marketService)
{
}

void WebService::init() {
    if (_started) return;
    setupRoutes();
    _server.begin();
    _started = true;
    LOG_INFO("Web", "HTTP 服务器启动 端口: %d", CFG_WIFI_WEB_PORT);
}

void WebService::update() {
    if (!_started) return;
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
    _server.on("/brightness",  HTTP_GET, [this]() { handleBrightness(); });
    _server.on("/timer/status", HTTP_GET, [this]() { handleTimerStatus(); });
    _server.on("/timer/start",  HTTP_GET, [this]() { handleTimerStart(); });
    _server.on("/timer/pause",  HTTP_GET, [this]() { handleTimerPause(); });
    _server.on("/timer/reset",  HTTP_GET, [this]() { handleTimerReset(); });
    _server.on("/timer/config", HTTP_GET, [this]() { handleTimerConfig(); });
    _server.on("/prefs",       HTTP_GET, [this]() { handlePrefs(); });
    _server.on("/state",       HTTP_GET, [this]() { handleState(); });
    _server.on("/serial_mode", HTTP_GET, [this]() { handleSerialMode(); });
    _server.on("/crypto/config", HTTP_GET, [this]() { handleCryptoConfig(); });
    _server.on("/crypto/config", HTTP_POST, [this]() { handleCryptoUpdate(); });
    _server.on("/crypto/refresh", HTTP_POST, [this]() { handleCryptoRefresh(); });
    _server.on("/market/config", HTTP_GET, [this]() { handleMarketConfig(); });
    _server.on("/market/config", HTTP_POST, [this]() { handleMarketUpdate(); });
    _server.on("/market/refresh", HTTP_POST, [this]() { handleMarketRefresh(); });
    _server.on("/market/search", HTTP_GET, [this]() { handleMarketSearch(); });

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
        case 'c': _displayService->setInteractiveView(VIEW_CLOCK); break;
        case 'p': _displayService->setInteractiveView(VIEW_POMODORO); break;
        case 'e': _displayService->setInteractiveView(VIEW_WEATHER); break;
        case 'm': _displayService->setInteractiveView(VIEW_CRYPTO); break;
        case 'k': _displayService->setInteractiveView(VIEW_MARKET); break;
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
        const uint8_t speed = constrain(_server.arg("v").toInt(), 1, 3);
        _displayService->setAnimSpeed(speed);
        if (_preferenceService) _preferenceService->setAnimSpeed(speed);
    }
    _server.send(200, "application/json", "{\"ok\":1}");
}

void WebService::handleRedraw() {
    if (_server.hasArg("bg")) {
        const String bg = _server.arg("bg");
        _displayService->setAnimBgColor(_displayService->hexToRgb565(bg));
        if (_preferenceService) _preferenceService->setDefaultBgHex(bg);
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
        const uint8_t brightness = _server.arg("on") == "1" ? 100 : 0;
        _displayService->setBrightnessPercent(brightness);
        if (_preferenceService) _preferenceService->setBrightnessPercent(brightness);
        _server.send(200, "application/json", "{\"ok\":true}");
    } else {
        _server.send(400, "application/json", "{\"error\":\"missing on parameter\"}");
    }
}

void WebService::handleBrightness() {
    if (_server.hasArg("v")) {
        const uint8_t brightness = constrain(_server.arg("v").toInt(), 0, 100);
        _displayService->setBrightnessPercent(brightness);
        if (_preferenceService) _preferenceService->setBrightnessPercent(brightness);
    }
    String json = "{\"ok\":true,\"brightness\":";
    json += _displayService->getBrightnessPercent();
    json += "}";
    _server.send(200, "application/json", json);
}

void WebService::handleTimerStatus() {
    const bool isBreak = _displayService->getPomodoroPhase() == PomodoroPhase::BREAK;
    String json = "{\"phase\":\"";
    json += isBreak ? "break" : "focus";
    json += "\",\"running\":";
    json += _displayService->isPomodoroRunning() ? "true" : "false";
    json += ",\"paused\":";
    json += _displayService->isPomodoroPaused() ? "true" : "false";
    json += ",\"remaining\":";
    json += _displayService->getPomodoroRemainingSec();
    json += ",\"duration\":";
    json += _displayService->getPomodoroDurationSec();
    json += ",\"focus\":";
    json += _displayService->getFocusMinutes();
    json += ",\"break\":";
    json += _displayService->getBreakMinutes();
    json += "}";
    _server.send(200, "application/json", json);
}

void WebService::handleTimerStart() {
    const String phase = _server.hasArg("phase") ? _server.arg("phase") : "focus";
    _displayService->startPomodoro(phase == "break" ? PomodoroPhase::BREAK : PomodoroPhase::FOCUS);
    handleTimerStatus();
}

void WebService::handleTimerPause() {
    _displayService->pausePomodoro();
    handleTimerStatus();
}

void WebService::handleTimerReset() {
    _displayService->resetPomodoro();
    handleTimerStatus();
}

void WebService::handleTimerConfig() {
    const uint16_t focus = _server.hasArg("focus") ? _server.arg("focus").toInt() : _displayService->getFocusMinutes();
    const uint16_t breakMinutes = _server.hasArg("break") ? _server.arg("break").toInt() : _displayService->getBreakMinutes();
    _displayService->setPomodoroDurations(focus, breakMinutes);
    handleTimerStatus();
}

void WebService::handlePrefs() {
    if (!_preferenceService) {
        _server.send(500, "application/json", "{\"error\":\"preferences unavailable\"}");
        return;
    }

    if (_server.hasArg("bg")) {
        const String bg = _server.arg("bg");
        _preferenceService->setDefaultBgHex(bg);
        const uint16_t color = _displayService->hexToRgb565(bg);
        _displayService->setAnimBgColor(color);
        _displayService->setDrawBgColor(color);
    }
    if (_server.hasArg("speed")) {
        const uint8_t speed = constrain(_server.arg("speed").toInt(), 1, 3);
        _preferenceService->setAnimSpeed(speed);
        _displayService->setAnimSpeed(speed);
    }
    if (_server.hasArg("startup")) {
        _preferenceService->setStartupView(constrain(_server.arg("startup").toInt(), 0, 7));
    }
    if (_server.hasArg("brightness")) {
        const uint8_t brightness = constrain(_server.arg("brightness").toInt(), 0, 100);
        _preferenceService->setBrightnessPercent(brightness);
        _displayService->setBrightnessPercent(brightness);
    }
    if (_server.hasArg("claudeStatus")) {
        const bool enabled = _server.arg("claudeStatus") == "1" ||
                             _server.arg("claudeStatus") == "true";
        _preferenceService->setClaudeStatusEnabled(enabled);
        _displayService->setClaudeStatusEnabled(enabled);
    }
    if (_server.hasArg("carousel")) {
        const bool enabled = _server.arg("carousel") == "1" ||
                             _server.arg("carousel") == "true";
        _preferenceService->setCarouselEnabled(enabled);
    }
    if (_server.hasArg("carouselSpeed")) {
        _preferenceService->setCarouselSpeedSeconds(
            constrain(_server.arg("carouselSpeed").toInt(), 5, 60));
    }
    if (_server.hasArg("carouselFixed")) {
        _preferenceService->setCarouselFixedView(
            static_cast<uint8_t>(_server.arg("carouselFixed").toInt()));
    }
    if (_server.hasArg("carouselOrder")) {
        const String value = _server.arg("carouselOrder");
        uint8_t order[3] = {};
        uint8_t index = 0;
        int start = 0;
        while (index < 3 && start >= 0) {
            const int comma = value.indexOf(',', start);
            const String part = value.substring(start,
                comma < 0 ? value.length() : comma);
            order[index++] = static_cast<uint8_t>(part.toInt());
            start = comma < 0 ? -1 : comma + 1;
        }
        if (index != 3 || !_preferenceService->setCarouselOrder(order)) {
            _server.send(400, "application/json", "{\"error\":\"invalid carousel order\"}");
            return;
        }
    }
    if (_server.hasArg("night")) {
        _preferenceService->setNightDimEnabled(_server.arg("night") == "1" ||
                                               _server.arg("night") == "true");
    }
    if (_server.hasArg("nightStart") || _server.hasArg("nightEnd")) {
        const uint8_t startHour = _server.hasArg("nightStart")
            ? _server.arg("nightStart").toInt()
            : _preferenceService->getNightStartHour();
        const uint8_t endHour = _server.hasArg("nightEnd")
            ? _server.arg("nightEnd").toInt()
            : _preferenceService->getNightEndHour();
        _preferenceService->setNightHours(startHour, endHour);
    }
    if (_server.hasArg("nightBrightness")) {
        _preferenceService->setNightBrightnessPercent(
            constrain(_server.arg("nightBrightness").toInt(), 0, 100));
    }

    _displayService->setBrightnessPercent(_preferenceService->getBrightnessPercent());
    _displayService->reloadIdleDisplayPreferences();

    String json = _preferenceService->getJson();
    json.remove(json.length() - 1);
    json += ",\"nightActive\":";
    json += _preferenceService->isNightDimActive(_timeService) ? "true" : "false";
    json += "}";
    _server.send(200, "application/json", json);
}

void WebService::handleState() {
    String j = "{\"view\":"; j += _displayService->getInteractiveView();
    j += ",\"busy\":";   j += _displayService->isBusy()       ? "true" : "false";
    j += ",\"term\":";   j += _displayService->isTermMode()   ? "true" : "false";
    j += ",\"bl\":";     j += _displayService->getBrightnessPercent() > 0 ? "true" : "false";
    j += ",\"brightness\":"; j += _displayService->getBrightnessPercent();
    j += ",\"speed\":";  j += _displayService->getAnimSpeed();
    j += ",\"claudeStatus\":"; j += _displayService->isClaudeStatusEnabled() ? "true" : "false";
    j += ",\"carousel\":"; j += _displayService->isCarouselEnabled() ? "true" : "false";
    j += ",\"serial\":"; j += _wifiService->isSerialMode() ? "true" : "false";
    j += "}";
    _server.send(200, "application/json", j);
}

void WebService::handleSerialMode() {
    _wifiService->skipProvisioning();
    _server.send(200, "application/json", "{\"ok\":true,\"mode\":\"serial\"}");
}

void WebService::handleCryptoConfig() {
    if (!_cryptoService) {
        _server.send(503, "application/json", "{\"error\":\"crypto unavailable\"}");
        return;
    }
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", _cryptoService->getJson());
}

void WebService::handleCryptoUpdate() {
    if (!_cryptoService) {
        _server.send(503, "application/json", "{\"error\":\"crypto unavailable\"}");
        return;
    }

    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, _server.arg("plain"));
    JsonArray list = doc["assets"].as<JsonArray>();
    if (error || list.isNull() || list.size() == 0 ||
        list.size() > CryptoService::MAX_ASSETS) {
        _server.send(400, "application/json", "{\"error\":\"choose 1 to 5 assets\"}");
        return;
    }

    CryptoAsset assets[CryptoService::MAX_ASSETS] = {};
    uint8_t count = 0;
    for (JsonObject item : list) {
        const char* id = item["id"] | "";
        const char* symbol = item["symbol"] | "";
        const char* name = item["name"] | symbol;
        if (id[0] == '\0' || symbol[0] == '\0') {
            _server.send(400, "application/json", "{\"error\":\"invalid asset\"}");
            return;
        }
        for (uint8_t previous = 0; previous < count; previous++) {
            if (strcmp(assets[previous].id, id) == 0) {
                _server.send(409, "application/json", "{\"error\":\"duplicate asset\"}");
                return;
            }
        }
        strlcpy(assets[count].id, id, sizeof(assets[count].id));
        strlcpy(assets[count].symbol, symbol, sizeof(assets[count].symbol));
        strlcpy(assets[count].name, name, sizeof(assets[count].name));
        assets[count].isGold = item["gold"] | false;
        count++;
    }

    if (!_cryptoService->setAssets(assets, count)) {
        _server.send(400, "application/json", "{\"error\":\"invalid asset configuration\"}");
        return;
    }
    if (_displayService->getInteractiveView() == VIEW_CRYPTO) {
        _displayService->redrawCurrentView();
    }
    handleCryptoConfig();
}

void WebService::handleCryptoRefresh() {
    if (!_cryptoService) {
        _server.send(503, "application/json", "{\"error\":\"crypto unavailable\"}");
        return;
    }
    _cryptoService->requestRefresh();
    _server.send(202, "application/json", "{\"ok\":true}");
}

void WebService::handleMarketConfig() {
    if (!_marketService) {
        _server.send(503, "application/json", "{\"error\":\"market unavailable\"}");
        return;
    }
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", _marketService->getJson());
}

void WebService::handleMarketUpdate() {
    if (!_marketService) {
        _server.send(503, "application/json", "{\"error\":\"market unavailable\"}");
        return;
    }

    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, _server.arg("plain"));
    JsonArray list = doc["assets"].as<JsonArray>();
    if (error || list.isNull() || list.size() == 0 ||
        list.size() > MarketService::MAX_ASSETS) {
        _server.send(400, "application/json", "{\"error\":\"choose 1 to 5 stocks\"}");
        return;
    }

    MarketAsset assets[MarketService::MAX_ASSETS] = {};
    uint8_t count = 0;
    for (JsonObject item : list) {
        const char* secid = item["secid"] | "";
        const char* code = item["code"] | "";
        const char* label = item["label"] | code;
        const char* name = item["name"] | code;
        if (secid[0] == '\0' || code[0] == '\0') {
            _server.send(400, "application/json", "{\"error\":\"invalid stock\"}");
            return;
        }
        for (uint8_t previous = 0; previous < count; previous++) {
            if (strcmp(assets[previous].secid, secid) == 0) {
                _server.send(409, "application/json", "{\"error\":\"duplicate stock\"}");
                return;
            }
        }
        strlcpy(assets[count].secid, secid, sizeof(assets[count].secid));
        strlcpy(assets[count].code, code, sizeof(assets[count].code));
        strlcpy(assets[count].label, label, sizeof(assets[count].label));
        strlcpy(assets[count].name, name, sizeof(assets[count].name));
        count++;
    }

    if (!_marketService->setAssets(assets, count)) {
        _server.send(400, "application/json",
                     "{\"error\":\"invalid market configuration\"}");
        return;
    }
    if (_displayService->getInteractiveView() == VIEW_MARKET) {
        _displayService->redrawCurrentView();
    }
    handleMarketConfig();
}

void WebService::handleMarketRefresh() {
    if (!_marketService) {
        _server.send(503, "application/json", "{\"error\":\"market unavailable\"}");
        return;
    }
    _marketService->requestRefresh();
    _server.send(202, "application/json", "{\"ok\":true}");
}

void WebService::handleMarketSearch() {
    if (!_marketService) {
        _server.send(503, "application/json", "{\"error\":\"market unavailable\"}");
        return;
    }
    const String query = _server.hasArg("q") ? _server.arg("q") : "";
    if (query.isEmpty() || query.length() > 48) {
        _server.send(400, "application/json", "{\"error\":\"invalid query\"}");
        return;
    }
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", _marketService->searchJson(query));
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

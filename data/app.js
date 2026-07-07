const API_BASE = '';
async function fetchJson(url) { try { return await (await fetch(API_BASE + url)).json(); } catch(e) { return null; } }
async function fetchText(url) { try { return await (await fetch(API_BASE + url)).text(); } catch(e) { return ''; } }
async function toggleBacklight() { const el = document.getElementById('status-indicator'); const on = el && el.style.opacity !== '0.3'; await fetch(`/backlight?on=${on?0:1}`); }
function formatElapsed(ms) { const s = Math.floor(ms/1000), m = Math.floor(s/60); return m > 0 ? `${m}:${String(s%60).padStart(2,'0')}` : `${s}s`; }
function updateWifiInfo() { fetchJson('/wifi/status').then(d => { if(!d) return; const s=document.getElementById('wifi-status'),i=document.getElementById('wifi-ssid'),p=document.getElementById('wifi-ip'); if(s) s.textContent=d.connected?'Connected':'AP mode'; if(i) i.textContent=d.ssid||'--'; if(p) p.textContent=d.ip||'--'; }); }
document.addEventListener('DOMContentLoaded', () => { updateWifiInfo(); setInterval(updateWifiInfo, 10000); });

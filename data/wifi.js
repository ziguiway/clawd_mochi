let selectedSSID = '';
async function scanWifi() {
    const list = document.getElementById('wifi-list');
    if (list) list.innerHTML = '<li class="wifi-item"><span>扫描中...</span></li>';
    const data = await fetchJson('/wifi/scan');
    if (!data || !Array.isArray(data)) { if (list) list.innerHTML = '<li class="wifi-item"><span>扫描失败</span></li>'; return; }
    if (list) list.innerHTML = '';
    data.forEach(net => {
        const li = document.createElement('li'); li.className = 'wifi-item';
        li.innerHTML = `<span class="ssid">${net.ssid||'(隐藏)'}</span><span><span class="rssi">${net.rssi}dBm</span>${net.encrypted?' <span class="encrypted">🔒</span>':''}</span>`;
        li.onclick = () => selectNetwork(net.ssid);
        if (list) list.appendChild(li);
    });
}
function selectNetwork(ssid) { selectedSSID = ssid; const i = document.getElementById('ssid-input'); if (i) i.value = ssid; const f = document.getElementById('connect-form'); if (f) f.style.display = 'block'; }
async function connectWifi() {
    const ssid = document.getElementById('ssid-input').value, pw = document.getElementById('password-input').value;
    const msg = document.getElementById('connect-msg');
    if (!ssid) { if (msg) { msg.textContent = '请选择网络'; msg.className = 'status-msg error'; } return; }
    if (msg) { msg.textContent = '连接中...'; msg.className = 'status-msg info'; }
    const data = await fetch('/wifi/connect', { method: 'POST', headers: {'Content-Type':'application/x-www-form-urlencoded'}, body: `ssid=${encodeURIComponent(ssid)}&password=${encodeURIComponent(pw)}` }).then(r=>r.json()).catch(()=>null);
    if (data && data.status === 'connecting') { if (msg) { msg.textContent = '正在连接，设备将重启...'; msg.className = 'status-msg success'; } setTimeout(()=>{ window.location.href='/'; }, 5000); }
    else { if (msg) { msg.textContent = '连接失败'; msg.className = 'status-msg error'; } }
}
document.addEventListener('DOMContentLoaded', scanWifi);

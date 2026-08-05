let selectedSSID = '';
async function scanWifi() {
    const list = document.getElementById('wifi-list');
    if (list) list.innerHTML = '<li class="wifi-item loading"><span>Scanning...</span></li>';
    const data = await fetchJson('/wifi/scan');
    if (!data || !Array.isArray(data)) { if (list) list.innerHTML = '<li class="wifi-item loading"><span>Scan failed</span></li>'; return; }
    if (list) list.innerHTML = '';
    if (data.length === 0) {
        if (list) list.innerHTML = '<li class="wifi-item loading"><span>No networks found</span></li>';
        return;
    }
    data.sort((a, b) => b.rssi - a.rssi);
    data.forEach(net => {
        const li = document.createElement('li'); li.className = 'wifi-item';
        const ssid = net.ssid || '(hidden)';
        const level = net.rssi > -55 ? 'strong' : (net.rssi > -72 ? 'ok' : 'weak');
        li.innerHTML = `<span class="ssid">${ssid}</span><span class="net-meta"><span class="signal ${level}"></span><span class="rssi">${net.rssi} dBm</span>${net.encrypted ? '<span class="encrypted">LOCK</span>' : '<span class="open">OPEN</span>'}</span>`;
        li.onclick = () => selectNetwork(net.ssid);
        if (list) list.appendChild(li);
    });
}
function selectNetwork(ssid) {
    selectedSSID = ssid;
    const i = document.getElementById('ssid-input');
    if (i) i.value = ssid;
    const f = document.getElementById('connect-form');
    if (f) f.style.display = 'block';
    document.querySelectorAll('.wifi-item').forEach(item => item.classList.toggle('selected', item.querySelector('.ssid')?.textContent === (ssid || '(hidden)')));
    document.getElementById('password-input')?.focus();
}
function showManualEntry() {
    const f = document.getElementById('connect-form');
    if (f) f.style.display = 'block';
    document.getElementById('ssid-input')?.focus();
}
async function connectWifi() {
    const ssid = document.getElementById('ssid-input').value, pw = document.getElementById('password-input').value;
    const msg = document.getElementById('connect-msg');
    if (!ssid) { if (msg) { msg.textContent = 'Select a network first.'; msg.className = 'status-msg error'; } return; }
    if (pw.length > 0 && pw.length < 8) { if (msg) { msg.textContent = 'Password must be at least 8 characters.'; msg.className = 'status-msg error'; } return; }
    if (msg) { msg.textContent = 'Verifying the new network. If this page disconnects, join ClaWD-Mochi and open 192.168.4.1.'; msg.className = 'status-msg info'; }
    const data = await fetch('/wifi/connect', { method: 'POST', headers: {'Content-Type':'application/x-www-form-urlencoded'}, body: `ssid=${encodeURIComponent(ssid)}&password=${encodeURIComponent(pw)}` }).then(r=>r.json()).catch(()=>null);
    if (!data || data.status !== 'connecting') {
        if (msg) { msg.textContent = 'Could not start the connection. Try again.'; msg.className = 'status-msg error'; }
        return;
    }
    for (let i = 0; i < 50; i++) {
        await new Promise(resolve => setTimeout(resolve, 1000));
        const status = await fetchJson('/wifi/status');
        if (!status) continue;
        if (status.connected && status.ssid === ssid) {
            if (msg) { msg.textContent = 'Connected. Opening the controller...'; msg.className = 'status-msg success'; }
            setTimeout(() => { window.location.href = '/'; }, 1000);
            return;
        }
        if (status.connected && status.ssid !== ssid) {
            if (msg) { msg.textContent = `Could not join ${ssid}. Reverted to ${status.ssid}.`; msg.className = 'status-msg info'; }
            return;
        }
        if (msg) {
            // 设备可能正在回退重连旧网络:只有彻底放弃(retryExhausted 或不再验证新凭据且未连接)
            // 才视为最终失败;否则继续轮询,给旧网络恢复留出时间。
            const finalFailure = status.retryExhausted
                || (status.lastError && !status.changingNetwork && !status.connected);
            msg.textContent = finalFailure
                ? `${status.lastError || 'Connection failed'}. The previous saved network was not overwritten.`
                : (status.lastError
                    ? `${status.lastError}. Restoring the previous network...`
                    : `${status.phase || 'Connecting'}...`);
            msg.className = finalFailure ? 'status-msg error' : 'status-msg info';
            if (finalFailure) return;
        }
    }
    if (msg) {
        msg.textContent = 'The device is no longer reachable here. Join ClaWD-Mochi and open 192.168.4.1 to continue.';
        msg.className = 'status-msg error';
    }
}
document.addEventListener('DOMContentLoaded', scanWifi);

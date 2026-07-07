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
async function connectWifi() {
    const ssid = document.getElementById('ssid-input').value, pw = document.getElementById('password-input').value;
    const msg = document.getElementById('connect-msg');
    if (!ssid) { if (msg) { msg.textContent = 'Select a network first.'; msg.className = 'status-msg error'; } return; }
    if (msg) { msg.textContent = 'Connecting...'; msg.className = 'status-msg info'; }
    const data = await fetch('/wifi/connect', { method: 'POST', headers: {'Content-Type':'application/x-www-form-urlencoded'}, body: `ssid=${encodeURIComponent(ssid)}&password=${encodeURIComponent(pw)}` }).then(r=>r.json()).catch(()=>null);
    if (data && data.status === 'connecting') { if (msg) { msg.textContent = 'Joining network. Reopen the controller at the new IP when connected.'; msg.className = 'status-msg success'; } setTimeout(()=>{ window.location.href='/'; }, 5000); }
    else { if (msg) { msg.textContent = 'Connection failed. Check the password and try again.'; msg.className = 'status-msg error'; } }
}
document.addEventListener('DOMContentLoaded', scanWifi);

(function() {
    function updateStatus() {
        fetchJson('/cc/status').then(data => {
            if (!data) return;
            const ind = document.getElementById('status-indicator');
            if (ind) ind.className = 'status-indicator ' + data.status.toLowerCase();
            const t = document.getElementById('status-text'); if (t) t.textContent = data.status;
            const h = document.getElementById('hook-name'); if (h) h.textContent = data.hook || '--';
            const tl = document.getElementById('tool-name'); if (tl) tl.textContent = data.tool || '--';
            const d = document.getElementById('detail'); if (d) d.textContent = data.detail || '--';
            const m = document.getElementById('model'); if (m) m.textContent = data.model || '--';
            const e = document.getElementById('elapsed'); if (e) e.textContent = data.elapsed ? formatElapsed(data.elapsed) : '--';
        });
    }
    setInterval(updateStatus, 1000);
    document.addEventListener('DOMContentLoaded', updateStatus);
})();

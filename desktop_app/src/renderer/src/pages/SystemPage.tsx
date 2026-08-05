import React from "react";
import type { StreamState } from "../types";

interface Props { state: Partial<StreamState>; }

export default function SystemPage({ state }: Props) {
  return (
    <section className="page active">
      <header className="page-head">
        <h1>System</h1>
        <p className="page-sub">Connection &amp; device health — more device settings arrive with the web-console migration</p>
      </header>
      <div className="card">
        <div className="card-title">CONNECTION</div>
        <div className="kv"><span>Device</span><b>{state.ip ?? "—"}</b></div>
        <div className="kv"><span>Streaming</span><b>{state.running ? "yes" : "no"}</b></div>
        <div className="kv"><span>Frames pushed</span><b>{state.frames ?? 0}</b></div>
      </div>
      <div className="card" style={{ marginTop: 16 }}>
        <div className="card-title">COMING HERE SOON</div>
        <div className="soon-grid">
          <span>Themes ×5</span><span>Brightness &amp; night dim</span><span>Idle carousel</span>
          <span>WiFi setup</span><span>OTA update</span><span>Device logs</span>
        </div>
      </div>
    </section>
  );
}

import React from "react";
import type { DeviceInfo, DisplayInfo, StreamState } from "../types";
import { useI18n } from "../i18n/I18nContext";

type Mode = "cursor" | "full" | "region";

const MODES: { id: Mode; nameKey: string; descKey: string; def?: boolean }[] = [
  { id: "cursor", nameKey: "stream.modeCursor", descKey: "stream.modeCursorDesc", def: true },
  { id: "full", nameKey: "stream.modeFull", descKey: "stream.modeFullDesc" },
  { id: "region", nameKey: "stream.modeRegion", descKey: "stream.modeRegionDesc" }
];

interface Props {
  devices: DeviceInfo[];
  state: Partial<StreamState>;
  frameB64: string | null;
  onRefreshDevices: () => void;
  onStateChange: (s: Partial<StreamState>) => void;
}

export default function StreamPage({ devices, state, frameB64, onRefreshDevices, onStateChange }: Props) {
  const { t } = useI18n();
  const [selectedIp, setSelectedIp] = React.useState<string | null>(null);
  const [manualIp, setManualIp] = React.useState("");
  const [mode, setMode] = React.useState<Mode>("cursor");
  const [fps, setFps] = React.useState(8);
  const [quality, setQuality] = React.useState(50);
  const [displays, setDisplays] = React.useState<DisplayInfo[]>([]);
  const [sourceId, setSourceId] = React.useState<string | null>(null);
  const [permission, setPermission] = React.useState<string>("granted");
  const running = !!state.running;

  React.useEffect(() => {
    window.mochi.listDisplays().then(d => {
      setDisplays(d);
      if (d.length && !sourceId) setSourceId(d[0].id);
    });
    window.mochi.checkScreenPermission().then(setPermission);
  }, []);

  React.useEffect(() => {
    if (!selectedIp && devices.length) setSelectedIp(devices[0].ip);
  }, [devices]);

  const start = async () => {
    if (!selectedIp) return;
    await window.mochi.startStream({ ip: selectedIp, mode, fps, quality, sourceId });
  };
  const stop = () => window.mochi.stopStream();

  const addManual = async () => {
    const ip = manualIp.trim();
    if (!ip) return;
    const ok = await window.mochi.checkDevice(ip);
    if (ok) { setSelectedIp(ip); setManualIp(""); }
    else alert(t("stream.noDeviceAt", { ip }));
  };

  return (
    <section className="page active">
      <header className="page-head">
        <h1>{t("stream.title")}</h1>
        <p className="page-sub">{t("stream.subtitle")}</p>
      </header>

      {permission !== "granted" && (
        <div className="perm-banner">
          {t("stream.permBanner")}
          <button className="mini-btn" onClick={() => window.mochi.openScreenPermissionSettings()}>
            {t("stream.openSettings")}
          </button>
        </div>
      )}

      <div className="stream-grid">
        <div className="card preview-card">
          <div className="card-title">
            {t("stream.livePreview")}
            <span className="fps-badge">{running ? `${fps} FPS` : t("stream.idle")}</span>
          </div>
          <div className="mochi-screen">
            <div className="mochi-screen-inner">
              {frameB64
                ? <img className="preview-img" src={`data:image/jpeg;base64,${frameB64}`} alt="stream preview" />
                : <div className="preview-idle">◉ ‿ ◉<span>{t("stream.waitingToStream")}</span></div>}
            </div>
            <div className="mochi-chin">MOCHI</div>
          </div>
          <div className="preview-meta">
            <span>240 × 240 · q{quality}</span>
            <span className="meta-dim">{t(MODES.find(m => m.id === mode)!.nameKey as never)}</span>
          </div>
        </div>

        <div className="card">
          <div className="card-title">{t("stream.captureMode")}</div>
          <div className="mode-list" role="radiogroup">
            {MODES.map(m => (
              <label key={m.id} className={"mode-item" + (mode === m.id ? " selected" : "")}>
                <input type="radio" name="mode" checked={mode === m.id}
                       onChange={() => setMode(m.id)} disabled={running} />
                <span className="mode-box">
                  <span className="mode-name">{t(m.nameKey as never)}{m.def && <em>{t("stream.defaultTag")}</em>}</span>
                  <span className="mode-desc">{t(m.descKey as never)}</span>
                </span>
              </label>
            ))}
          </div>

          <div className="card-title" style={{ marginTop: 18 }}>{t("stream.parameters")}</div>
          <div className="param-row">
            <label>{t("stream.frameRate")}</label>
            <input type="range" min={3} max={15} value={fps} disabled={running}
                   onChange={e => setFps(+e.target.value)} />
            <span className="param-val">{fps} fps</span>
          </div>
          <div className="param-row">
            <label>{t("stream.jpegQuality")}</label>
            <input type="range" min={30} max={80} value={quality} disabled={running}
                   onChange={e => setQuality(+e.target.value)} />
            <span className="param-val">{quality}</span>
          </div>
          <div className="param-row">
            <label>{t("stream.display")}</label>
            <select className="sel" value={sourceId ?? ""} disabled={running}
                    onChange={e => setSourceId(e.target.value)}>
              {displays.map(d => <option key={d.id} value={d.id}>{d.label} · {d.size.width}×{d.size.height}</option>)}
            </select>
          </div>

          <button className={"cta" + (running ? " stop" : "")}
                  onClick={running ? stop : start}
                  disabled={!running && !selectedIp}>
            {running ? `■ ${t("stream.stopStreaming")}` : `▶ ${t("stream.startStreaming")}`}
          </button>
        </div>
      </div>

      <div className="card" style={{ marginTop: 16 }}>
        <div className="card-title">
          {t("stream.devices")}
          <button className="mini-btn" onClick={onRefreshDevices}>{t("stream.rescan")}</button>
        </div>
        <div className="dev-list">
          {devices.map(d => (
            <div key={d.ip}
                 className={"dev-row" + (selectedIp === d.ip ? " connected" : "")}
                 onClick={() => !running && setSelectedIp(d.ip)}>
              <span className={"dev-eyes" + (selectedIp === d.ip ? "" : " idle")}>
                {selectedIp === d.ip ? "◉‿◉" : "◉_◉"}
              </span>
              <div className="dev-info">
                <b>{d.name}</b>
                <span>{d.ip} · found via broadcast</span>
              </div>
              {selectedIp === d.ip && running
                ? <span className="dev-badge">STREAMING</span>
                : selectedIp === d.ip && <span className="dev-badge">SELECTED</span>}
            </div>
          ))}
          {!devices.length && (
            <div className="dev-row dim">
              <span className="dev-eyes idle">◉x◉</span>
              <div className="dev-info"><b>Searching…</b><span>{t("app.noDevice")}</span></div>
            </div>
          )}
          <div className="dev-row dim">
            <span className="dev-eyes idle">◉+◉</span>
            <div className="dev-info"><b>{t("stream.manualDevice")}</b><span>{t("stream.manualHint")}</span></div>
            <input className="ip-input" placeholder="192.168.1.___" value={manualIp}
                   onChange={e => setManualIp(e.target.value)}
                   onKeyDown={e => e.key === "Enter" && addManual()} />
            <button className="mini-btn" onClick={addManual}>{t("stream.add")}</button>
          </div>
        </div>
      </div>
    </section>
  );
}

import React from "react";
import { NAV } from "../lib/nav";
import { useI18n, type MsgKey } from "../i18n/I18nContext";

interface Props {
  page: string;
  onNavigate: (id: string) => void;
  deviceLabel: string | null;
  streaming: boolean;
}

export default function Sidebar({ page, onNavigate, deviceLabel, streaming }: Props) {
  const { t } = useI18n();
  return (
    <aside className="sidebar">
      <div className="brand">
        <PixelEyes />
        <div className="brand-txt">
          <span className="brand-name">MOCHI</span>
          <span className="brand-sub">{t("app.subtitle")}</span>
        </div>
      </div>
      <nav className="nav">
        {NAV.map(group => (
          <div className="nav-group" key={group.labelKey}>
            <div className="nav-label">{t(group.labelKey as MsgKey)}</div>
            {group.pages.map(p => (
              <button
                key={p.id}
                className={"nav-item" + (page === p.id ? " active" : "")}
                onClick={() => onNavigate(p.id)}
              >
                <span className="nav-ico">{p.icon}</span> {t(p.labelKey as MsgKey)}
                {p.soon && <span className="nav-soon">{t("app.soon")}</span>}
                {p.id === "stream" && streaming && <span className="nav-dot on" />}
              </button>
            ))}
          </div>
        ))}
      </nav>
      <div className="sidebar-foot">
        <span className="dev-pill">
          <span className={"dev-dot" + (deviceLabel ? "" : " off")} />
          {deviceLabel ?? t("app.noDevice")}
        </span>
      </div>
    </aside>
  );
}

function PixelEyes() {
  const ref = React.useRef<HTMLCanvasElement>(null);
  React.useEffect(() => {
    const ctx = ref.current?.getContext("2d");
    if (!ctx) return;
    let timer: number;
    const draw = (open: boolean) => {
      ctx.clearRect(0, 0, 48, 24);
      ctx.fillStyle = "#fb6b10";
      if (open) { ctx.fillRect(4, 2, 12, 20); ctx.fillRect(32, 2, 12, 20); }
      else { ctx.fillRect(4, 10, 12, 4); ctx.fillRect(32, 10, 12, 4); }
    };
    const blink = () => {
      draw(true);
      timer = window.setTimeout(() => {
        draw(false);
        setTimeout(() => draw(true), 140);
      }, 1800 + Math.random() * 2600);
      timer = window.setTimeout(blink, 1800 + Math.random() * 2600);
    };
    blink();
    return () => clearTimeout(timer);
  }, []);
  return <canvas ref={ref} width={48} height={24} aria-label="Mochi eyes" />;
}

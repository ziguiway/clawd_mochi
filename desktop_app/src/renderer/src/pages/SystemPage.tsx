import React from "react";
import type { StreamState } from "../types";
import { useI18n, LOCALES } from "../i18n/I18nContext";
import { useTheme, THEMES, type ThemeId } from "../theme/ThemeContext";

interface Props { state: Partial<StreamState>; }

export default function SystemPage({ state }: Props) {
  const { t, locale, setLocale } = useI18n();
  const { theme, setTheme } = useTheme();

  return (
    <section className="page active">
      <header className="page-head">
        <h1>{t("system.title")}</h1>
        <p className="page-sub">{t("system.subtitle")}</p>
      </header>

      <div className="card">
        <div className="card-title">{t("system.appearance")}</div>
        <div className="param-row">
          <label>{t("system.language")}</label>
          <select className="sel" value={locale}
                  onChange={e => setLocale(e.target.value as typeof locale)}>
            {LOCALES.map(l => <option key={l.id} value={l.id}>{l.label}</option>)}
          </select>
        </div>
        <div className="param-row" style={{ alignItems: "flex-start" }}>
          <label style={{ paddingTop: 6 }}>{t("system.theme")}</label>
          <div className="theme-grid">
            {THEMES.map(th => (
              <button
                key={th.id}
                className={"theme-chip" + (theme === th.id ? " selected" : "")}
                onClick={() => setTheme(th.id as ThemeId)}
              >
                <span className="theme-swatch" style={{ background: th.swatch }} />
                {th.id === "system" ? t("system.themeSystem") : th.label}
              </button>
            ))}
          </div>
        </div>
      </div>

      <div className="card" style={{ marginTop: 16 }}>
        <div className="card-title">{t("system.connection")}</div>
        <div className="kv"><span>{t("system.device")}</span><b>{state.ip ?? "—"}</b></div>
        <div className="kv"><span>{t("system.streaming")}</span><b>{state.running ? t("system.yes") : t("system.no")}</b></div>
        <div className="kv"><span>{t("system.framesPushed")}</span><b>{state.frames ?? 0}</b></div>
      </div>

      <div className="card" style={{ marginTop: 16 }}>
        <div className="card-title">{t("system.comingSoon")}</div>
        <div className="soon-grid">
          <span>Themes ×5</span><span>Brightness &amp; night dim</span><span>Idle carousel</span>
          <span>WiFi setup</span><span>OTA update</span><span>Device logs</span>
        </div>
      </div>
    </section>
  );
}

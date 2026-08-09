import React from "react";
import { DeviceClient, type UsageStatus } from "../lib/DeviceClient";
import { useI18n } from "../i18n/I18nContext";

function resetText(minutes: number, t: (key: any) => string) {
  if (!Number.isFinite(minutes) || minutes < 0) return t("usage.unknown");
  const days = Math.floor(minutes / 1440), hours = Math.floor((minutes % 1440) / 60), mins = minutes % 60;
  return `${days ? `${days}${t("usage.days")} ` : ""}${hours ? `${hours}${t("usage.hours")} ` : ""}${mins}${t("usage.minutes")}`;
}

export default function UsagePage({ ip }: { ip: string | null }) {
  const { t } = useI18n();
  const [state, setState] = React.useState<UsageStatus | null>(null);
  const [token, setToken] = React.useState("");
  const [error, setError] = React.useState<string | null>(null);
  const load = React.useCallback(() => { if (ip) DeviceClient.usage(ip).then(setState).catch(() => setError(t("usage.loadFailed"))); }, [ip, t]);
  React.useEffect(() => { load(); const timer = window.setInterval(load, 15000); return () => window.clearInterval(timer); }, [load]);
  if (!ip) return <section className="page active"><header className="page-head"><h1>{t("usage.title")}</h1><p className="page-sub">{t("usage.noDevice")}</p></header></section>;
  const save = () => { if (!token.trim()) return; DeviceClient.saveUsageToken(ip, token.trim()).then(s => { setState(s); setToken(""); setError(null); }).catch(() => setError(t("usage.saveFailed"))); };
  const clear = () => DeviceClient.clearUsageToken(ip).then(setState).catch(() => setError(t("usage.saveFailed")));
  const refresh = () => DeviceClient.refreshUsage(ip).then(load).catch(() => setError(t("usage.loadFailed")));
  const quota = (left: number, reset: number, label: string) => <div className="usage-card"><div className="card-title">{label}<b>{left >= 0 ? `${left.toFixed(1)}%` : "--"}</b></div><div className="usage-track"><span style={{ width: `${Math.max(0, Math.min(100, left))}%` }} /></div><small>{t("usage.resetsIn")} {resetText(reset, t)}</small></div>;
  return <section className="page active"><header className="page-head"><h1>{t("usage.title")}</h1><p className="page-sub">{t("usage.subtitle")}</p></header><div className="card usage-console"><div className="kv"><span>{t("usage.status")}</span><b>{state?.authError ? t("usage.tokenError") : state?.loading ? t("usage.loading") : state?.valid ? t("usage.live") : state?.configured ? t("usage.saved") : t("usage.notSet")}</b></div><div className="usage-grid">{quota(state?.sessionLeft ?? -1, state?.sessionResetMins ?? -1, t("usage.fiveHour"))}{quota(state?.weeklyLeft ?? -1, state?.weeklyResetMins ?? -1, t("usage.sevenDay"))}</div><label className="usage-field"><span>{t("usage.credential")}</span><input type="password" value={token} onChange={e => setToken(e.target.value)} placeholder={t("usage.placeholder")} autoComplete="off" /></label><div className="param-row"><button className="mini-btn" onClick={save}>{t("usage.save")}</button><button className="mini-btn" onClick={clear}>{t("usage.clear")}</button><button className="mini-btn" onClick={refresh}>{t("usage.refresh")}</button></div><p className="page-sub">{state?.authError ? t("usage.replaceToken") : t("usage.deviceOnly")}</p>{error && <p className="page-sub">{error}</p>}</div></section>;
}

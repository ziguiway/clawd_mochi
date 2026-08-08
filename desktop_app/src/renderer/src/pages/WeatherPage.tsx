import React from "react";
import { DeviceClient, type WeatherLocation } from "../lib/DeviceClient";
import { useI18n } from "../i18n/I18nContext";

interface Result { name: string; latitude: number; longitude: number; admin1?: string; country?: string; }

export default function WeatherPage({ ip }: { ip: string | null }) {
  const { t } = useI18n();
  const [location, setLocation] = React.useState<WeatherLocation | null>(null);
  const [query, setQuery] = React.useState("");
  const [results, setResults] = React.useState<Result[]>([]);
  const [error, setError] = React.useState<string | null>(null);

  React.useEffect(() => { if (ip) DeviceClient.weatherLocation(ip).then(setLocation).catch(() => setError(t("weather.loadFailed"))); }, [ip, t]);
  React.useEffect(() => { if (!query.trim()) { setResults([]); return; } const timer = window.setTimeout(() => { fetch(`https://geocoding-api.open-meteo.com/v1/search?name=${encodeURIComponent(query)}&count=8&language=zh&format=json`).then(r => r.json()).then(j => setResults(j.results ?? [])).catch(() => setError(t("weather.searchFailed"))); }, 280); return () => window.clearTimeout(timer); }, [query, t]);
  const save = (lat: number, lon: number, city: string, source: "gps" | "manual") => { if (!ip) return; DeviceClient.saveWeatherLocation(ip, lat, lon, city, source).then(setLocation).catch(() => setError(t("weather.saveFailed"))); setQuery(""); setResults([]); };
  const gps = () => { if (!navigator.geolocation) { setError(t("weather.gpsUnavailable")); return; } navigator.geolocation.getCurrentPosition(async p => { let city = t("weather.gpsLocation"); try { const r = await fetch(`https://nominatim.openstreetmap.org/reverse?format=jsonv2&lat=${p.coords.latitude}&lon=${p.coords.longitude}&zoom=10&accept-language=zh-CN`); const j = await r.json(); const a = j.address ?? {}; city = a.city ?? a.town ?? a.municipality ?? a.county ?? city; } catch {} save(p.coords.latitude, p.coords.longitude, city, "gps"); }, () => setError(t("weather.gpsFailed")), { enableHighAccuracy: true, timeout: 12000, maximumAge: 300000 }); };
  if (!ip) return <section className="page active"><header className="page-head"><h1>{t("weather.title")}</h1><p className="page-sub">{t("weather.noDevice")}</p></header></section>;
  return <section className="page active"><header className="page-head"><h1>{t("weather.title")}</h1><p className="page-sub">{t("weather.subtitle")}</p></header><div className="card"><div className="param-row"><label>{t("weather.search")}</label><input className="ip-input" value={query} onChange={e => setQuery(e.target.value)} placeholder={t("weather.searchPlaceholder")} /><button className="mini-btn" onClick={gps}>{t("weather.autoLocate")}</button></div><div className="location-results">{results.map((r, i) => <button className="location-result" key={`${r.latitude}-${r.longitude}-${i}`} onClick={() => save(r.latitude, r.longitude, r.name, "manual")}><strong>{r.name}</strong><span>{[r.admin1, r.country].filter(Boolean).join(" · ")}</span></button>)}</div>{location && <div className="kv"><span>{t("weather.current")}</span><b>{location.label || location.city} · {location.source.toUpperCase()}</b></div>}<button className="mini-btn" onClick={() => ip && DeviceClient.resetWeatherLocation(ip).then(setLocation)}>{t("weather.useIp")}</button>{error && <p className="page-sub">{error}</p>}</div></section>;
}

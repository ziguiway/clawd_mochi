import React from "react";
import { DeviceClient, type CarouselPrefs } from "../lib/DeviceClient";
import { useI18n } from "../i18n/I18nContext";

const PAGES = [
  [6, "carousel.clock"], [7, "carousel.pomodoro"], [8, "carousel.weather"],
  [9, "carousel.crypto"], [10, "carousel.market"], [17, "carousel.salary"],
  [18, "carousel.timetable"],
  [20, "carousel.usage"],
] as const;
const DEFAULT_ORDER = [8, 9, 10, 6, 17, 7, 18, 20];

export default function CarouselPage({ ip }: { ip: string | null }) {
  const { t } = useI18n();
  const [prefs, setPrefs] = React.useState<CarouselPrefs | null>(null);
  const [dragged, setDragged] = React.useState<number | null>(null);
  const [error, setError] = React.useState<string | null>(null);
  const names = new Map<number, string>(PAGES.map(([id, key]) => [id, t(key)]));

  React.useEffect(() => {
    if (!ip) return;
    DeviceClient.prefs(ip).then(p => setPrefs({ ...p, carouselOrder: p.carouselOrder?.length ? p.carouselOrder : DEFAULT_ORDER }))
      .catch(() => setError(t("carousel.loadFailed")));
  }, [ip, t]);

  const save = (next: CarouselPrefs) => {
    setPrefs(next); setError(null);
    if (ip) DeviceClient.saveCarousel(ip, next).catch(() => setError(t("carousel.saveFailed")));
  };
  if (!ip) return <section className="page active"><header className="page-head"><h1>{t("carousel.title")}</h1><p className="page-sub">{t("carousel.noDevice")}</p></header></section>;
  if (!prefs) return <section className="page active"><header className="page-head"><h1>{t("carousel.title")}</h1><p className="page-sub">{t("carousel.loading")}</p></header></section>;
  const order = prefs.carouselOrder;
  const availableIds = prefs.carouselPages?.length ? prefs.carouselPages : PAGES.map(([id]) => id);
  const pageName = (id: number) => names.get(id) ?? `${t("carousel.unknown")} (${id})`;
  return <section className="page active">
    <header className="page-head"><h1>{t("carousel.title")}</h1><p className="page-sub">{t("carousel.subtitle")}</p></header>
    <div className="card">
      <div className="param-row"><label>{t("carousel.enabled")}</label><button className="mini-btn" onClick={() => save({ ...prefs, carousel: !prefs.carousel })}>{prefs.carousel ? t("carousel.on") : t("carousel.off")}</button></div>
      <div className="param-row"><label>{t("carousel.interval")}</label><input type="range" min="5" max="60" value={prefs.carouselSpeed} onChange={e => save({ ...prefs, carouselSpeed: Number(e.target.value) })}/><b>{prefs.carouselSpeed}s</b></div>
      <div className="param-row"><label>{t("carousel.fixed")}</label><select className="sel" value={prefs.carouselFixed} onChange={e => save({ ...prefs, carouselFixed: Number(e.target.value) })}>{availableIds.map(id => <option key={id} value={id}>{pageName(id)}</option>)}</select></div>
    </div>
    <div className="card" style={{ marginTop: 16 }}><div className="card-title">{t("carousel.order")}</div>
      <div className="carousel-list">{order.map((id, index) => <div key={id} className="carousel-item" draggable onDragStart={() => setDragged(index)} onDragOver={e => e.preventDefault()} onDrop={() => { if (dragged === null || dragged === index) return; const next = [...order]; const [item] = next.splice(dragged, 1); next.splice(index, 0, item); setDragged(null); save({ ...prefs, carouselOrder: next }); }}><span>{String(index + 1).padStart(2, "0")}</span><strong>{pageName(id)}</strong><button className="icon-btn" disabled={order.length <= 1} aria-label={t("carousel.remove")} title={t("carousel.remove")} onClick={() => save({ ...prefs, carouselOrder: order.filter(v => v !== id) })}>×</button><span className="drag-handle">⠿</span></div>)}</div>
      <div className="param-row" style={{ marginTop: 12 }}><label>{t("carousel.add")}</label><select className="sel" defaultValue="" onChange={e => { const id = Number(e.target.value); if (id && !order.includes(id)) save({ ...prefs, carouselOrder: [...order, id] }); e.currentTarget.value = ""; }}><option value="">{t("carousel.choose")}</option>{availableIds.filter(id => !order.includes(id)).map(id => <option key={id} value={id}>{pageName(id)}</option>)}</select></div>
    </div>
    {error && <p className="page-sub">{error}</p>}
  </section>;
}

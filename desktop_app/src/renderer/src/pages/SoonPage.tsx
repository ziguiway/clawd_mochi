import React from "react";
import { useI18n, type MsgKey } from "../i18n/I18nContext";
import { NAV } from "../lib/nav";

const ARTS: Record<string, string> = {
  expressions: "◉ ‿ ◉", clock: "◷", weather: "☁", market: "↗",
  stats: "∑", timetable: "☷", arcade: "▶", media: "▤",
};
const DESC_KEYS: Record<string, MsgKey> = {
  expressions: "soon.expressionsDesc",
  arcade: "soon.arcadeDesc",
  media: "soon.mediaDesc",
};

export default function SoonPage({ id }: { id: string }) {
  const { t } = useI18n();
  const page = NAV.flatMap(g => g.pages).find(p => p.id === id);
  return (
    <section className="page active soon-page">
      <div className="soon-art">{ARTS[id] ?? "◉_◉"}</div>
      <h2>{page ? t(page.labelKey as MsgKey) : id}</h2>
      <p>{t(DESC_KEYS[id] ?? "soon.comingSoon")}</p>
    </section>
  );
}

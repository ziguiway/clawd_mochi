import React from "react";

const COPY: Record<string, { art: string; title: string; desc: string }> = {
  expressions: { art: "◉ ‿ ◉", title: "Expressions", desc: "8 pixel faces, manual / auto mode and blink speed — coming in the web-console migration." },
  clock: { art: "◷", title: "Clock & Pomodoro", desc: "Coming soon." },
  weather: { art: "☁", title: "Weather", desc: "Coming soon." },
  market: { art: "↗", title: "Stocks & Crypto", desc: "Coming soon." },
  stats: { art: "∑", title: "Focus Stats", desc: "Coming soon." },
  timetable: { art: "☷", title: "Timetable", desc: "Coming soon." },
  arcade: { art: "▶", title: "Arcade", desc: "6 games — coming soon." },
  media: { art: "▤", title: "Media Cast", desc: "Images & GIF — coming soon." }
};

export default function SoonPage({ id }: { id: string }) {
  const c = COPY[id] ?? { art: "◉_◉", title: id, desc: "Coming soon." };
  return (
    <section className="page active soon-page">
      <div className="soon-art">{c.art}</div>
      <h2>{c.title}</h2>
      <p>{c.desc}</p>
    </section>
  );
}

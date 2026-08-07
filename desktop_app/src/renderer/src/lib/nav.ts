// 导航分组镜像固件 InteractiveView 枚举,后续迁入网页功能=加页面模块。
// label 为 i18n key(nav.*),由 Sidebar 通过 useI18n 翻译。
export interface NavPage { id: string; labelKey: string; icon: string; soon?: boolean; }
export interface NavGroup { labelKey: string; pages: NavPage[]; }

export const NAV: NavGroup[] = [
  { labelKey: "nav.stream", pages: [
    { id: "stream", labelKey: "nav.desktopStream", icon: "▣" },
    { id: "keyboardPet", labelKey: "nav.keyboardPet", icon: "◎" },
  ]},
  { labelKey: "nav.face", pages: [
    { id: "expressions", labelKey: "nav.expressions", icon: "◉", soon: true },
  ]},
  { labelKey: "nav.time", pages: [
    { id: "clock", labelKey: "nav.clock", icon: "◷", soon: true },
  ]},
  { labelKey: "nav.info", pages: [
    { id: "weather", labelKey: "nav.weather", icon: "☁", soon: true },
    { id: "market", labelKey: "nav.market", icon: "↗", soon: true },
    { id: "stats", labelKey: "nav.stats", icon: "∑", soon: true },
    { id: "timetable", labelKey: "nav.timetable", icon: "☷", soon: true },
  ]},
  { labelKey: "nav.play", pages: [
    { id: "arcade", labelKey: "nav.arcade", icon: "▶", soon: true },
    { id: "media", labelKey: "nav.media", icon: "▤", soon: true },
  ]},
  { labelKey: "nav.device", pages: [
    { id: "system", labelKey: "nav.system", icon: "⚙" },
  ]},
];

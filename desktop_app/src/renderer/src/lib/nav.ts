// 导航分组镜像固件 InteractiveView 枚举,后续迁入网页功能=加页面模块。
export interface NavPage { id: string; label: string; icon: string; soon?: boolean; }
export interface NavGroup { label: string; pages: NavPage[]; }

export const NAV: NavGroup[] = [
  { label: "STREAM", pages: [
    { id: "stream", label: "Desktop Stream", icon: "▣" },
  ]},
  { label: "FACE", pages: [
    { id: "expressions", label: "Expressions", icon: "◉", soon: true },
  ]},
  { label: "TIME", pages: [
    { id: "clock", label: "Clock & Pomodoro", icon: "◷", soon: true },
  ]},
  { label: "INFO PANELS", pages: [
    { id: "weather", label: "Weather", icon: "☁", soon: true },
    { id: "market", label: "Stocks & Crypto", icon: "↗", soon: true },
    { id: "stats", label: "Focus Stats", icon: "∑", soon: true },
    { id: "timetable", label: "Timetable", icon: "☷", soon: true },
  ]},
  { label: "PLAY", pages: [
    { id: "arcade", label: "Arcade", icon: "▶", soon: true },
    { id: "media", label: "Media Cast", icon: "▤", soon: true },
  ]},
  { label: "DEVICE", pages: [
    { id: "system", label: "System", icon: "⚙" },
  ]},
];

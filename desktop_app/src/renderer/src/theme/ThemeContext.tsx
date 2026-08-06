import React from "react";

// 主题呼应设备端 5 套配色(orange-black / orange-white / dark-orange / mint / pink)。
// "system" 跟随 prefers-color-scheme,在 dark / light 间切换。
export type ThemeId = "system" | "dark" | "light" | "dark-orange" | "mint" | "pink";
export const THEMES: { id: ThemeId; label: string; swatch: string }[] = [
  { id: "system",      label: "System",       swatch: "linear-gradient(135deg,#14100c 50%,#f5ede0 50%)" },
  { id: "dark",        label: "Orange Black", swatch: "#14100c" },
  { id: "light",       label: "Orange White", swatch: "#f5ede0" },
  { id: "dark-orange", label: "Dark Orange",  swatch: "#2a1608" },
  { id: "mint",        label: "Mint",         swatch: "#0f1f1a" },
  { id: "pink",        label: "Pink",         swatch: "#241118" },
];
const STORAGE_KEY = "mochi.theme";

interface ThemeCtx {
  theme: ThemeId;
  setTheme: (t: ThemeId) => void;
}

const Ctx = React.createContext<ThemeCtx | null>(null);

function applyTheme(theme: ThemeId) {
  const root = document.documentElement;
  root.dataset.theme = theme;
  // system 时 data-theme=system,CSS 用 media query 兜底;其余直接命中变量块。
}

export function ThemeProvider({ children }: { children: React.ReactNode }) {
  const [theme, setThemeState] = React.useState<ThemeId>(() => {
    const saved = localStorage.getItem(STORAGE_KEY) as ThemeId | null;
    return saved && THEMES.some(t => t.id === saved) ? saved : "system";
  });

  React.useEffect(() => { applyTheme(theme); }, [theme]);

  const setTheme = React.useCallback((t: ThemeId) => {
    setThemeState(t);
    localStorage.setItem(STORAGE_KEY, t);
  }, []);

  const value = React.useMemo(() => ({ theme, setTheme }), [theme, setTheme]);
  return <Ctx.Provider value={value}>{children}</Ctx.Provider>;
}

export function useTheme(): ThemeCtx {
  const ctx = React.useContext(Ctx);
  if (!ctx) throw new Error("useTheme must be used inside ThemeProvider");
  return ctx;
}

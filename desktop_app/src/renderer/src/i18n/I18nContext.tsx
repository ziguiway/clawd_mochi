import React from "react";
import { zh, type Messages } from "./zh";
import { en } from "./en";

export type Locale = "zh" | "en";
export const LOCALES: { id: Locale; label: string }[] = [
  { id: "zh", label: "简体中文" },
  { id: "en", label: "English" },
];
// 新增语言: 加一份语言包并在此注册即可。
const BUNDLES: Record<Locale, Messages> = { zh, en };
const STORAGE_KEY = "mochi.locale";

type Path<T> = T extends object
  ? { [K in keyof T & string]: T[K] extends object ? `${K}.${Path<T[K]>}` : K }[keyof T & string]
  : never;
export type MsgKey = Path<Messages>;

function resolve(bundle: Messages, key: string): string {
  let node: unknown = bundle;
  for (const part of key.split(".")) {
    node = (node as Record<string, unknown>)?.[part];
    if (node == null) return key;
  }
  return String(node);
}

interface I18nCtx {
  locale: Locale;
  setLocale: (l: Locale) => void;
  /** t("stream.title") / t("stream.noDeviceAt", { ip }) */
  t: (key: MsgKey, vars?: Record<string, string | number>) => string;
}

const Ctx = React.createContext<I18nCtx | null>(null);

export function I18nProvider({ children }: { children: React.ReactNode }) {
  const [locale, setLocaleState] = React.useState<Locale>(() => {
    const saved = localStorage.getItem(STORAGE_KEY);
    if (saved === "zh" || saved === "en") return saved;
    return navigator.language.toLowerCase().startsWith("zh") ? "zh" : "en";
  });

  const setLocale = React.useCallback((l: Locale) => {
    setLocaleState(l);
    localStorage.setItem(STORAGE_KEY, l);
  }, []);

  const t = React.useCallback((key: MsgKey, vars?: Record<string, string | number>) => {
    let s = resolve(BUNDLES[locale], key);
    if (vars) for (const [k, v] of Object.entries(vars)) s = s.replace(`{{${k}}}`, String(v));
    return s;
  }, [locale]);

  const value = React.useMemo(() => ({ locale, setLocale, t }), [locale, setLocale, t]);
  return <Ctx.Provider value={value}>{children}</Ctx.Provider>;
}

export function useI18n(): I18nCtx {
  const ctx = React.useContext(Ctx);
  if (!ctx) throw new Error("useI18n must be used inside I18nProvider");
  return ctx;
}

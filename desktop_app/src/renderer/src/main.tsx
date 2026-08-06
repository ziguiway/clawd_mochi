import React from "react";
import { createRoot } from "react-dom/client";
import App from "./App";
import { I18nProvider } from "./i18n/I18nContext";
import { ThemeProvider } from "./theme/ThemeContext";
import "./styles.css";

// 防主题闪烁: 渲染前先从 localStorage 恢复 data-theme。
const savedTheme = localStorage.getItem("mochi.theme");
if (savedTheme) document.documentElement.dataset.theme = savedTheme;

createRoot(document.getElementById("root")!).render(
  <React.StrictMode>
    <ThemeProvider>
      <I18nProvider>
        <App />
      </I18nProvider>
    </ThemeProvider>
  </React.StrictMode>
);

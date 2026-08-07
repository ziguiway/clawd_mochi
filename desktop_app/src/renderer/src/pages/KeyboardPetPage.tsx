import React from "react";
import type { DeviceInfo, KeyboardPetState } from "../types";
import { useI18n } from "../i18n/I18nContext";

export default function KeyboardPetPage({ devices, state, onStateChange }: { devices: DeviceInfo[]; state: KeyboardPetState; onStateChange: (s: KeyboardPetState) => void }) {
  const { t } = useI18n();
  const [ip, setIp] = React.useState(state.ip ?? devices[0]?.ip ?? "");
  React.useEffect(() => { if (!ip && devices[0]) setIp(devices[0].ip); }, [devices, ip]);
  const toggle = async () => {
    if (state.running) await window.mochi.stopKeyboardPet();
    else if (ip) await window.mochi.startKeyboardPet(ip);
  };
  return <section className="page active">
    <header className="page-head"><h1>{t("pet.title")}</h1><p className="page-sub">{t("pet.subtitle")}</p></header>
    <div className="card">
      <div className="card-title">{t("pet.device")}</div>
      <select className="sel" value={ip} onChange={e => setIp(e.target.value)}>
        <option value="">{t("pet.selectDevice")}</option>{devices.map(d => <option key={d.ip} value={d.ip}>{d.name} · {d.ip}</option>)}
      </select>
      <button className="primary-btn" style={{ marginTop: 18 }} onClick={toggle} disabled={!ip && !state.running}>{state.running ? t("pet.stop") : t("pet.start")}</button>
      <p className="page-sub" style={{ marginTop: 14 }}>{state.running ? t("pet.active") : t("pet.permission")}</p>
    </div>
  </section>;
}

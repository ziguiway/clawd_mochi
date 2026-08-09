import React from "react";
import Sidebar from "./components/Sidebar";
import StreamPage from "./pages/StreamPage";
import SystemPage from "./pages/SystemPage";
import SoonPage from "./pages/SoonPage";
import type { DeviceInfo, StreamState } from "./types";
import KeyboardPetPage from "./pages/KeyboardPetPage";
import CarouselPage from "./pages/CarouselPage";
import WeatherPage from "./pages/WeatherPage";
import UsagePage from "./pages/UsagePage";

export default function App() {
  const [page, setPage] = React.useState("stream");
  const [devices, setDevices] = React.useState<DeviceInfo[]>([]);
  const [streamState, setStreamState] = React.useState<Partial<StreamState>>({});
  const [frameB64, setFrameB64] = React.useState<string | null>(null);
  const [petState, setPetState] = React.useState({ running: false, ip: null as string | null });

  React.useEffect(() => {
    window.mochi.getDevices().then(setDevices);
    window.mochi.getStreamState().then(setStreamState);
    window.mochi.onDevices(setDevices);
    window.mochi.onStreamState(s => setStreamState(prev => ({ ...prev, ...s })));
    window.mochi.onStreamFrame(setFrameB64);
    window.mochi.getKeyboardPetState().then(setPetState);
    window.mochi.onKeyboardPetState(setPetState);
  }, []);

  const deviceLabel = streamState.ip ?? (devices[0] ? `${devices[0].name} · ${devices[0].ip}` : null);

  return (
    <div className="app">
      <Sidebar page={page} onNavigate={setPage} deviceLabel={deviceLabel}
               streaming={!!streamState.running} />
      <main className="main">
        {page === "stream" && (
          <StreamPage devices={devices} state={streamState} frameB64={frameB64}
                      onRefreshDevices={() => window.mochi.getDevices().then(setDevices)}
                      onStateChange={s => setStreamState(prev => ({ ...prev, ...s }))} />
        )}
        {page === "system" && <SystemPage state={streamState} />}
        {page === "keyboardPet" && <KeyboardPetPage devices={devices} state={petState} onStateChange={setPetState} />}
        {page === "carousel" && <CarouselPage ip={streamState.ip ?? devices[0]?.ip ?? null} />}
        {page === "weather" && <WeatherPage ip={streamState.ip ?? devices[0]?.ip ?? null} />}
        {page === "usage" && <UsagePage ip={streamState.ip ?? devices[0]?.ip ?? null} />}
        {page !== "stream" && page !== "system" && page !== "keyboardPet" && page !== "carousel" && page !== "weather" && page !== "usage" && <SoonPage id={page} />}
      </main>
    </div>
  );
}

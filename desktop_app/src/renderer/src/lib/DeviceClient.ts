// 与 Mochi 设备通信的统一封装: HTTP REST(视图切换/状态) +
// 推流通道由主进程持有(TCP)。后续网页功能迁入时在此加方法。
const DEFAULT_TIMEOUT_MS = 4000;

async function request<T>(ip: string, path: string, init?: RequestInit): Promise<T> {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), DEFAULT_TIMEOUT_MS);
  try {
    const res = await fetch(`http://${ip}${path}`, {
      ...init,
      signal: controller.signal,
      cache: "no-store"
    });
    if (!res.ok) throw new Error(`${path} -> HTTP ${res.status}`);
    return (await res.json()) as T;
  } finally {
    clearTimeout(timer);
  }
}

export interface StreamStatus {
  active: boolean; connected: boolean; fps: number; frames: number;
}
export interface CarouselPrefs {
  carousel: boolean; carouselSpeed: number; carouselOrder: number[]; carouselFixed: number; carouselPages?: number[];
}

export const DeviceClient = {
  streamStatus: (ip: string) => request<StreamStatus>(ip, "/stream/status"),
  ccStatus: (ip: string) => request<Record<string, unknown>>(ip, "/cc/status"),
  state: (ip: string) => request<Record<string, unknown>>(ip, "/state"),
  prefs: (ip: string) => request<CarouselPrefs>(ip, "/prefs"),
  saveCarousel: (ip: string, prefs: CarouselPrefs) => request<CarouselPrefs>(
    ip,
    `/prefs?carousel=${prefs.carousel ? "1" : "0"}&carouselSpeed=${prefs.carouselSpeed}` +
      `&carouselFixed=${prefs.carouselFixed}&carouselOrder=${prefs.carouselOrder.join(",")}`
  )
};

// Mochi Desktop — Electron 主进程
// 职责: UDP 4211 设备发现 / desktopCapturer 截屏 / sharp JPEG 编码 /
//       TCP 3333 推流 / 托盘常驻 / 断线重连。渲染进程经 IPC 驱动。
const {
  app, BrowserWindow, Tray, Menu, ipcMain, desktopCapturer, screen,
  systemPreferences, nativeImage, shell
} = require("electron");
const path = require("node:path");
const dgram = require("node:dgram");
const net = require("node:net");
const uiohook = require("uiohook-napi");

const FRAME_W = 240;
const FRAME_H = 240;
const STREAM_PORT = 3333;
const DISCOVERY_PORT = 4211;
const MAGIC = Buffer.from("ESPF");
const MAX_JPEG_BYTES = 32 * 1024;
const CURSOR_CROP_DIP = 240;

let win = null;
let tray = null;
let sharp = null;
try { sharp = require("sharp"); } catch (e) { console.error("sharp unavailable", e); }

const KEYBOARD_PET_PORT = 4212;
const pet = { running: false, ip: null, socket: null };
let petHooksBound = false;
const LEFT_KEYS = new Set([16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 30, 31, 32, 33, 34, 35, 41, 42, 44, 45, 46, 47]);
function petSide(keycode) {
  if (keycode === 57419 || keycode === 57416) return "left";
  if (keycode === 57421 || keycode === 57424) return "right";
  return LEFT_KEYS.has(keycode) ? "left" : "right";
}
function sendPet(action, keycode) {
  if (!pet.running || !pet.socket || !pet.ip) return;
  const packet = Buffer.from(`KP:${action}:${petSide(keycode)}`);
  pet.socket.send(packet, KEYBOARD_PET_PORT, pet.ip);
}
function setPetHooks(active) {
  if (active) {
    if (!petHooksBound) {
      uiohook.uIOhook.on("keydown", e => sendPet("down", e.keycode));
      uiohook.uIOhook.on("keyup", e => sendPet("up", e.keycode));
      petHooksBound = true;
    }
    uiohook.uIOhook.start();
  } else {
    try { uiohook.uIOhook.stop(); } catch (_) {}
  }
}
async function startKeyboardPet(ip) {
  stopKeyboardPet();
  pet.ip = ip;
  try { await httpPost(ip, "/keyboard_pet/start"); } catch (e) { console.error("keyboard pet start failed", e); return; }
  pet.socket = dgram.createSocket("udp4");
  pet.running = true;
  setPetHooks(true);
  if (win) win.webContents.send("pet:state", { running: true, ip });
}
function stopKeyboardPet() {
  pet.running = false;
  setPetHooks(false);
  if (pet.socket) { pet.socket.close(); pet.socket = null; }
  if (pet.ip) fetch(`http://${pet.ip}/keyboard_pet/stop`, { method: "POST" }).catch(() => {});
  if (win) win.webContents.send("pet:state", { running: false, ip: pet.ip });
}

// ── Streamer ──────────────────────────────────────────────────
const streamer = {
  running: false,
  ip: null,
  socket: null,
  timer: null,
  sourceId: null,
  mode: "cursor",          // cursor | full | region
  fps: 8,
  quality: 70,
  frames: 0,
  startedAt: 0,
  reconnectDelay: 1000,
  reconnectTimer: null,
  region: null             // {x,y,w,h} 固定区域(屏幕物理像素)
};

function sendState() {
  if (!win) return;
  win.webContents.send("stream:state", {
    running: streamer.running,
    ip: streamer.ip,
    frames: streamer.frames,
    mode: streamer.mode,
    fps: streamer.fps,
    quality: streamer.quality
  });
}

async function httpPost(ip, p) {
  const res = await fetch(`http://${ip}${p}`, { method: "POST" });
  if (!res.ok) throw new Error(`${p} -> HTTP ${res.status}`);
  return res.json().catch(() => ({}));
}

async function captureJpeg() {
  const displays = screen.getAllDisplays();
  const target = displays.find(d => String(d.id) === streamer.sourceId) || displays[0];
  const scale = target.scaleFactor || 1;
  const size = target.size; // DIP
  const thumbW = Math.round(size.width * scale);
  const thumbH = Math.round(size.height * scale);

  const sources = await desktopCapturer.getSources({
    types: ["screen"],
    thumbnailSize: { width: thumbW, height: thumbH }
  });
  const src = sources.find(s => String(s.display_id) === String(target.id)) || sources[0];
  if (!src || src.thumbnail.isEmpty()) throw new Error("empty capture");

  let image = sharp(src.thumbnail.toPNG());
  if (streamer.mode === "cursor") {
    const pt = screen.getCursorScreenPoint(); // DIP
    const relX = Math.round((pt.x - target.bounds.x) * scale);
    const relY = Math.round((pt.y - target.bounds.y) * scale);
    // 截取 240x240 逻辑点区域；Retina 屏可保留源细节，避免把过大的
    // 480 DIP 画面压到 240 像素后导致文字无法辨认。
    const side = Math.min(CURSOR_CROP_DIP * scale, thumbW, thumbH);
    const left = Math.max(0, Math.min(relX - side / 2, thumbW - side));
    const top = Math.max(0, Math.min(relY - side / 2, thumbH - side));
    image = image.extract({ left, top, width: side, height: side })
                 .resize(FRAME_W, FRAME_H, { kernel: "lanczos3" });
  } else if (streamer.mode === "region" && streamer.region) {
    const r = streamer.region;
    image = image.extract({
      left: Math.max(0, Math.round(r.x * scale)),
      top: Math.max(0, Math.round(r.y * scale)),
      width: Math.min(Math.round(r.w * scale), thumbW),
      height: Math.min(Math.round(r.h * scale), thumbH)
    }).resize(FRAME_W, FRAME_H, { kernel: "lanczos3" });
  } else {
    // full: 居中裁方形再缩放, 避免拉伸
    const side = Math.min(thumbW, thumbH);
    const left = Math.round((thumbW - side) / 2);
    const top = Math.round((thumbH - side) / 2);
    image = image.extract({ left, top, width: side, height: side })
                 .resize(FRAME_W, FRAME_H, { kernel: "lanczos3" });
  }
  image = image.sharpen(0.6);

  // 默认 4:2:0 优先保证 C3 解码帧率；质量 80 以上切换到 4:4:4，
  // 供需要彩色文字边缘清晰度的场景使用。Sharp 不支持 4:2:2。
  const requestedQuality = Math.max(30, Math.min(90, streamer.quality));
  const requestedSubsampling = requestedQuality >= 80 ? "4:4:4" : "4:2:0";
  const attempts = [
    { quality: requestedQuality, chromaSubsampling: requestedSubsampling },
    { quality: Math.max(30, requestedQuality - 10), chromaSubsampling: "4:2:0" },
    { quality: Math.max(30, requestedQuality - 20), chromaSubsampling: "4:2:0" }
  ];
  for (const options of attempts) {
    const jpeg = await image.clone().jpeg({
      ...options,
      progressive: false,
      optimiseCoding: true
    }).toBuffer();
    if (jpeg.length <= MAX_JPEG_BYTES) return jpeg;
  }
  throw new Error(`JPEG frame exceeds ${MAX_JPEG_BYTES} bytes`);
}

async function streamTick() {
  if (!streamer.running || !streamer.socket) return;
  const startedAt = Date.now();
  try {
    const jpeg = await captureJpeg();
    const header = Buffer.alloc(8);
    MAGIC.copy(header, 0);
    header.writeUInt32LE(jpeg.length, 4);
    streamer.socket.write(Buffer.concat([header, jpeg]));
    streamer.frames++;
    if (streamer.frames % 16 === 0) sendState();
    // 本地预览
    if (win) win.webContents.send("stream:frame", jpeg.toString("base64"));
  } catch (e) {
    console.error("capture failed", e);
  } finally {
    if (streamer.running && streamer.socket) {
      const interval = Math.round(1000 / streamer.fps);
      const delay = Math.max(0, interval - (Date.now() - startedAt));
      streamer.timer = setTimeout(streamTick, delay);
    }
  }
}

function scheduleTick() {
  clearTimeout(streamer.timer);
  streamer.timer = setTimeout(streamTick, 0);
}

async function startStreaming(opts) {
  await stopStreaming(true);
  streamer.ip = opts.ip;
  streamer.mode = opts.mode || "cursor";
  streamer.fps = opts.fps || 8;
  streamer.quality = opts.quality || 70;
  streamer.sourceId = opts.sourceId || null;
  streamer.region = opts.region || null;
  await httpPost(streamer.ip, "/stream/enter");
  connectSocket();
  streamer.running = true;
  streamer.frames = 0;
  streamer.startedAt = Date.now();
  sendState();
  updateTray();
}

function connectSocket() {
  const s = net.createConnection({ host: streamer.ip, port: STREAM_PORT });
  s.setNoDelay(true);
  streamer.socket = s;
  s.on("connect", () => {
    streamer.reconnectDelay = 1000;
    scheduleTick();
    sendState();
  });
  s.on("error", () => {});
  s.on("close", () => {
    clearTimeout(streamer.timer);
    if (streamer.running) {
      // 指数退避重连
      streamer.reconnectTimer = setTimeout(() => {
        streamer.reconnectDelay = Math.min(streamer.reconnectDelay * 2, 15000);
        connectSocket();
      }, streamer.reconnectDelay);
    }
  });
}

async function stopStreaming(silent) {
  streamer.running = false;
  clearTimeout(streamer.timer);
  clearTimeout(streamer.reconnectTimer);
  if (streamer.socket) { streamer.socket.destroy(); streamer.socket = null; }
  if (streamer.ip && !silent) {
    try { await httpPost(streamer.ip, "/stream/exit"); } catch (_) {}
  }
  sendState();
  updateTray();
}

// ── UDP discovery ─────────────────────────────────────────────
const devices = new Map(); // ip -> {name, ip, lastSeen}
function startDiscovery() {
  const udp = dgram.createSocket({ type: "udp4", reuseAddr: true });
  udp.on("error", () => {});
  udp.on("message", (msg, rinfo) => {
    const text = msg.toString();
    // CC_DISCOVER:ClawdMochi:<ip>
    if (text.startsWith("CC_DISCOVER:")) {
      const parts = text.split(":");
      const name = parts[1] || "ClawdMochi";
      const ip = parts[2] || rinfo.address;
      devices.set(ip, { name, ip, lastSeen: Date.now() });
      if (win) win.webContents.send("devices:list", [...devices.values()]);
    }
  });
  udp.bind(DISCOVERY_PORT, () => udp.setBroadcast(true));
}

// ── Window / Tray ─────────────────────────────────────────────
function createWindow() {
  win = new BrowserWindow({
    width: 1120, height: 760,
    minWidth: 960, minHeight: 640,
    backgroundColor: "#14100c",
    title: "Mochi Desktop",
    webPreferences: {
      preload: path.join(__dirname, "../preload/preload.js"),
      contextIsolation: true,
      nodeIntegration: false
    }
  });
  if (process.env.VITE_DEV_SERVER_URL || !app.isPackaged) {
    win.loadURL("http://localhost:5173");
  } else {
    win.loadFile(path.join(__dirname, "../../dist/renderer/index.html"));
  }
  win.on("close", e => {
    // 托盘常驻: 关窗不退出
    if (!app.isQuitting) { e.preventDefault(); win.hide(); }
  });
}

function updateTray() {
  if (!tray) return;
  const label = streamer.running ? "■ Stop streaming" : "▶ Open Mochi Desktop";
  tray.setContextMenu(Menu.buildFromTemplate([
    { label: streamer.running ? `Streaming → ${streamer.ip}` : "Idle", enabled: false },
    { type: "separator" },
    { label, click: () => {
        if (streamer.running) stopStreaming();
        else { win.show(); win.focus(); }
      } },
    { label: "Mode: Mouse Follow", click: () => setMode("cursor") },
    { label: "Mode: Full Screen", click: () => setMode("full") },
    { type: "separator" },
    { label: "Quit", click: () => { app.isQuitting = true; app.quit(); } }
  ]));
}

function setMode(mode) {
  streamer.mode = mode;
  if (win) win.webContents.send("stream:state", { mode });
}

function createTray() {
  // 16x16 模板图标: 简单橙块
  const img = nativeImage.createFromDataURL(
    "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAKElEQVQ4T2NkYGD4z0AEYBxVSFAcMDIwMPCjCxdGbGxsDJqQBAUAAJEfQ/F+GseQAAAAAElFTkSuQmCC");
  tray = new Tray(img.resize({ width: 16, height: 16 }));
  tray.setToolTip("Mochi Desktop");
  tray.on("click", () => { win.show(); win.focus(); });
  updateTray();
}

// ── IPC ───────────────────────────────────────────────────────
ipcMain.handle("stream:start", (_e, opts) => startStreaming(opts));
ipcMain.handle("stream:stop", () => stopStreaming());
ipcMain.handle("stream:getState", () => ({
  running: streamer.running, ip: streamer.ip, frames: streamer.frames,
  mode: streamer.mode, fps: streamer.fps, quality: streamer.quality
}));
ipcMain.handle("devices:get", () => [...devices.values()]);
ipcMain.handle("devices:check", async (_e, ip) => {
  try {
    const res = await fetch(`http://${ip}/stream/status`, { signal: AbortSignal.timeout(2500) });
    return res.ok;
  } catch (_) { return false; }
});
ipcMain.handle("pet:start", (_e, ip) => startKeyboardPet(ip));
ipcMain.handle("pet:stop", () => stopKeyboardPet());
ipcMain.handle("pet:getState", () => ({ running: pet.running, ip: pet.ip }));
ipcMain.handle("displays:list", () =>
  screen.getAllDisplays().map(d => ({ id: String(d.id), label: d.label || `Display ${d.id}`, size: d.size })));
ipcMain.handle("screen:checkPermission", () => {
  if (process.platform !== "darwin") return "granted";
  return systemPreferences.getMediaAccessStatus("screen");
});
ipcMain.handle("screen:openPermissionSettings", () => {
  shell.openExternal("x-apple.systempreferences:com.apple.preference.security?Privacy_ScreenCapture");
});

app.whenReady().then(() => {
  createWindow();
  createTray();
  startDiscovery();
  app.on("activate", () => win.show());
});
app.on("window-all-closed", () => { /* tray keeps app alive */ });
app.on("before-quit", () => { app.isQuitting = true; stopStreaming(true); stopKeyboardPet(); });

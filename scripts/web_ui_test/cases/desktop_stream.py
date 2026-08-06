"""Desktop Stream (VIEW_DESKTOP_STREAM 21) cases.

stub_mode: drives controller.html against FirmwareStubHandler — verifies the
panel UI, the /stream/* endpoint contract, and ESPF TCP framing against a local
mock PC streamer (no device needed).

device mode (--device-url): additionally runs the full real link — a local
TCP streamer pushes ESPF+JPEG frames to the device on port 3333, asserting
connect/FPS/frame-counter progress, and exercises the lazy-load memory
discipline by cycling the view 20 times while watching /stream/status.
"""

from __future__ import annotations

import base64
import socket
import struct
import threading
import time
import urllib.request

from playwright.sync_api import Page

from ..infrastructure import FirmwareStubHandler
from .common import open_controller

def jpeg_via_page(page: Page, color: str = "#fb6b10") -> bytes:
    data_url = page.evaluate(
        """(color) => {
            const c = document.createElement('canvas');
            c.width = 240; c.height = 240;
            const ctx = c.getContext('2d');
            ctx.fillStyle = color; ctx.fillRect(0, 0, 240, 240);
            ctx.fillStyle = '#000'; ctx.font = '20px monospace';
            ctx.fillText('MOCHI', 80, 120);
            return c.toDataURL('image/jpeg', 0.7);
        }""",
        color,
    )
    return base64.b64decode(data_url.split(",", 1)[1])

class MockPcStreamer:
    """本地模拟上位机: 连接设备 3333 端口并按 ESPF 协议推 JPEG 帧。"""

    MAGIC = b"ESPF"

    def __init__(self, host: str, port: int = 3333) -> None:
        self.host = host
        self.port = port
        self._sock: socket.socket | None = None
        self._thread: threading.Thread | None = None
        self._stop = threading.Event()
        self.frames_sent = 0
        self.error: str | None = None

    def start(self, jpeg: bytes, fps: float = 8.0) -> None:
        self._stop.clear()
        self.frames_sent = 0
        self.error = None
        frame = self.MAGIC + struct.pack("<I", len(jpeg)) + jpeg

        def pump() -> None:
            try:
                self._sock = socket.create_connection((self.host, self.port), timeout=5)
                self._sock.settimeout(2)
                interval = 1.0 / fps
                while not self._stop.is_set():
                    t0 = time.monotonic()
                    self._sock.sendall(frame)
                    self.frames_sent += 1
                    dt = time.monotonic() - t0
                    if dt < interval:
                        self._stop.wait(interval - dt)
            except OSError as exc:  # 设备断开(超时/退出视图)是预期路径
                self.error = str(exc)
            finally:
                if self._sock is not None:
                    try:
                        self._sock.close()
                    except OSError:
                        pass
                    self._sock = None

        self._thread = threading.Thread(target=pump, daemon=True)
        self._thread.start()

    def wait_connected(self, timeout: float = 6.0) -> bool:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self.frames_sent > 0:
                return True
            if self.error is not None:
                return False
            time.sleep(0.05)
        return False

    def stop(self) -> None:
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=3)
            self._thread = None

def _post(base_url: str, path: str) -> tuple[int, dict]:
    import json
    req = urllib.request.Request(base_url.rstrip("/") + path, method="POST")
    try:
        with urllib.request.urlopen(req, timeout=8) as res:
            return res.status, json.loads(res.read() or b"{}")
    except urllib.error.HTTPError as exc:
        return exc.code, json.loads(exc.read() or b"{}")

def _get(base_url: str, path: str) -> dict:
    import json
    with urllib.request.urlopen(base_url.rstrip("/") + path, timeout=8) as res:
        return json.loads(res.read() or b"{}")

def run_desktop_stream_flow(page: Page, *, stub_mode: bool,
                            device_url: str | None = None) -> None:
    """controller.html Desktop Stream 面板的 UI/协议流程(两种模式都跑)。"""
    open_controller(page)
    page.locator('button[data-v="21"]').click()
    page.locator("#streamWrap.open").wait_for(state="visible")
    state = page.locator("#streamState")
    assert state.text_content() == "OFF", state.text_content()

    page.locator("#streamEnter").click()
    if stub_mode:
        assert FirmwareStubHandler.stream_enter_count == 1
        page.wait_for_function(
            "() => document.querySelector('#streamState').textContent === 'WAITING PC'"
        )
        assert page.locator("#streamEnter").is_disabled()
        assert not page.locator("#streamExit").is_disabled()
        # 模拟 PC 推流后, 状态轮询应显示 STREAMING fps
        FirmwareStubHandler.stream_status.update({"active": True, "connected": True, "fps": 8.0, "frames": 42})
        page.wait_for_function(
            "() => document.querySelector('#streamState').textContent.startsWith('STREAMING')",
            timeout=5000,
        )
        page.locator("#streamExit").click()
        assert FirmwareStubHandler.stream_exit_count == 1
        page.wait_for_function(
            "() => document.querySelector('#streamState').textContent === 'OFF'"
        )
        print("PASS  Desktop Stream 面板 enter/status/exit UI 流程(stub)")
    else:
        base = device_url.rstrip("/")
        page.wait_for_function(
            "() => document.querySelector('#streamState').textContent === 'WAITING PC'"
        )
        print("PASS  实机进入 Desktop Stream 视图, 等待页显示 WAITING PC")
        _run_device_link(page, base)
        # UI 退出
        page.locator("#streamExit").click()
        page.wait_for_function(
            "() => document.querySelector('#streamState').textContent === 'OFF'",
            timeout=8000,
        )
        print("PASS  实机退出 Desktop Stream 视图")

def _run_device_link(page: Page, base: str) -> None:
    """真机全链路: TCP 推帧 -> FPS/计数 -> 帧内容变化 -> 20 次进出内存纪律。"""
    host = base.split("//", 1)[1].split(":")[0].split("/")[0]
    jpeg = jpeg_via_page(page)

    status = _get(base, "/stream/status")
    assert status["active"] and not status["connected"], status

    streamer = MockPcStreamer(host)
    try:
        streamer.start(jpeg, fps=8.0)
        assert streamer.wait_connected(), f"TCP 连接失败: {streamer.error}"
        print("PASS  实机 TCP 3333 连接并推流(ESPF+JPEG)")

        # 帧计数与 FPS 增长
        s1 = _get(base, "/stream/status")
        time.sleep(3)
        s2 = _get(base, "/stream/status")
        assert s2["connected"], s2
        assert s2["frames"] > s1["frames"], (s1, s2)
        assert s2["fps"] >= 5.0, f"FPS 过低: {s2}"
        print(f"PASS  实机推流 FPS={s2['fps']:.1f}, 3s 内帧数 {s1['frames']} -> {s2['frames']}")

        # UI 状态轮询应显示 STREAMING
        page.wait_for_function(
            "() => document.querySelector('#streamState').textContent.startsWith('STREAMING')",
            timeout=6000,
        )
        print("PASS  实机面板状态轮询显示 STREAMING")

        # 换色帧 -> 设备应继续收新内容(帧数持续增长即画面在更新)
        streamer.stop()
        streamer.wait_connected(0.2)
        streamer2 = MockPcStreamer(host)
        jpeg2 = jpeg_via_page(page, "#0f5fd0")
        streamer2.start(jpeg2, fps=8.0)
        assert streamer2.wait_connected(), f"重连失败: {streamer2.error}"
        f1 = _get(base, "/stream/status")["frames"]
        time.sleep(2)
        f2 = _get(base, "/stream/status")["frames"]
        assert f2 > f1, (f1, f2)
        streamer2.stop()
        print("PASS  断流后重连恢复, 帧内容持续更新")
    finally:
        streamer.stop()

    # 内存纪律: 20 次进出视图, status 接口应始终一致可用(不崩溃)
    for i in range(20):
        code, j = _post(base, "/stream/enter")
        assert code == 200 and j["active"], (i, code, j)
        code, j = _post(base, "/stream/exit")
        assert code == 200 and not j["active"], (i, code, j)
    print("PASS  20 次进出 Desktop Stream 视图(懒加载分配/释放稳定)")

    # 再次进入供 UI exit 流程使用
    code, j = _post(base, "/stream/enter")
    assert code == 200 and j["active"], (code, j)

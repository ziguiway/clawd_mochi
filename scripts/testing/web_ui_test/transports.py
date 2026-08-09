"""Serial capture and live-device request throttling."""

from __future__ import annotations

import threading
import time
from typing import TYPE_CHECKING, Any

if TYPE_CHECKING:
    from playwright.sync_api import Route

class SerialLogCapture:
    def __init__(self, port: str) -> None:
        import serial

        self._serial = serial.Serial(port, 115200, timeout=0.2)
        self._lines: list[str] = []
        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._read, daemon=True)

    def start(self) -> None:
        self._serial.reset_input_buffer()
        self._thread.start()

    def _read(self) -> None:
        while not self._stop.is_set():
            try:
                raw = self._serial.readline()
            except OSError:
                return
            if raw:
                self._lines.append(
                    raw.decode("utf-8", errors="replace").strip()
                )

    def text(self) -> str:
        return "\n".join(self._lines)

    def write(self, command: str) -> None:
        self._serial.write(command.encode())
        self._serial.flush()

    def close(self) -> None:
        self._stop.set()
        self._thread.join(timeout=1)
        self._serial.close()

class DeviceRequestThrottle:
    def __init__(self, interval_seconds: float) -> None:
        self._interval = max(0.0, interval_seconds)
        self._last_request = 0.0
        self._lock = threading.Lock()

    def __call__(self, route: "Route | Any") -> None:
        with self._lock:
            wait_seconds = self._interval - (time.monotonic() - self._last_request)
            if wait_seconds > 0:
                time.sleep(wait_seconds)
            self._last_request = time.monotonic()
        route.continue_()

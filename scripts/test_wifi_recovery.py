#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = ["pyserial>=3.5"]
# ///
"""验证串口 WiFi 配置在错误凭据后可恢复。

运行前通过环境变量提供测试网络，凭据不会写入脚本或测试报告：
MOCHI_WIFI_SSID='...' MOCHI_WIFI_PASSWORD='...' \\
    uv run scripts/test_wifi_recovery.py --serial-port /dev/cu.usbmodem...
"""

from __future__ import annotations

import argparse
import os
import time

import serial


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Clawd Mochi WiFi 恢复功能测试")
    parser.add_argument("--serial-port", required=True, help="设备 USB 串口路径")
    parser.add_argument("--timeout", type=float, default=15.0, help="连接等待秒数")
    return parser.parse_args()


class SerialConsole:
    def __init__(self, port: str) -> None:
        self._serial = serial.Serial(port, 115200, timeout=0.2)
        self._text = ""

    def close(self) -> None:
        self._serial.close()

    def command(self, command: str, seconds: float) -> str:
        self._serial.reset_input_buffer()
        self._serial.write((command + "\n").encode())
        self._serial.flush()
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            chunk = self._serial.read_all()
            if chunk:
                self._text += chunk.decode("utf-8", errors="replace")
            time.sleep(0.1)
        return self._text


def main() -> int:
    args = parse_args()
    ssid = os.environ.get("MOCHI_WIFI_SSID")
    password = os.environ.get("MOCHI_WIFI_PASSWORD")
    if not ssid or not password:
        raise SystemExit("需要设置 MOCHI_WIFI_SSID 和 MOCHI_WIFI_PASSWORD")

    console = SerialConsole(args.serial_port)
    try:
        logs = console.command(f"wifi {ssid} clawd-mochi-invalid-password", 8)
        logs = console.command("wifi status", 1)
        assert "WiFi: 未连接" in logs, logs
        print("PASS  错误 WiFi 凭据不会伪造连接成功")

        logs = console.command(f"wifi {ssid} {password}", args.timeout)
        logs = console.command("wifi status", 1)
        assert "WiFi: 已连接" in logs, logs
        assert f"SSID: {ssid}" in logs, logs
        assert "[WiFi] 已连接:" in logs, logs
        print("PASS  串口重新配置正确凭据后恢复 WiFi 连接")
    finally:
        console.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

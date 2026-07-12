#!/usr/bin/env python3
"""Clawd Mochi 串口守护进程 — 常开 COM 口，hook 通过本地 UDP 转发，避免反复开关触发 ESP32-C3 复位。

# /// script
# requires-python = ">=3.10"
# dependencies = [
#   "pyserial>=3.5",
# ]
# ///

背景：ESP32-C3 内置 USB-Serial-JTAG 在 Windows 上每次关闭 COM 句柄都会触发
CORE_USB_UART 硬件复位(rst:0x15)。hook 是每次工具调用起独立进程，必然开关
端口 → 每次都复位。本进程常开端口，hook 改发本地 UDP 给本进程转发，端口只
开一次，不再复位。

用法:
    uv run --script cc_serial_daemon.py            # 前台跑
    uv run --script cc_serial_daemon.py --install   # 装开机自启(Windows 任务计划)
    uv run --script cc_serial_daemon.py --uninstall # 卸载开机自启
"""

from __future__ import annotations

import argparse
import json
import os
import socket
import subprocess
import sys
import time
from pathlib import Path

# ========== 配置 ==========
LISTEN_HOST = "127.0.0.1"
LISTEN_PORT = 4211          # hook → daemon 的本地 UDP 端口
ESP32_PORT = 4210           # 不用，保留与 cc_hook 一致
SERIAL_BAUD = 115200
SERIAL_TIMEOUT = 0.2
SERIAL_PROBE_TIMEOUT = 0.4
REOPEN_INTERVAL = 2.0       # 端口掉线后重试间隔(秒)
SWEEP_INTERVAL = 30.0       # 无端口时重新扫描的间隔(秒)
CACHE_FILE_NAME = "cc_hook_cache.json"
# ==========================

PROBE_MSG = b"CC:ping\n"


def app_cache_dir() -> Path:
    override = os.environ.get("CLAWD_MOCHI_CACHE_DIR")
    if override:
        return Path(override).expanduser()
    if os.name == "nt":
        base = os.environ.get("LOCALAPPDATA") or os.environ.get("APPDATA")
        if base:
            return Path(base) / "ClawdMochi"
        return Path.home() / "AppData" / "Local" / "ClawdMochi"
    if sys.platform == "darwin":
        return Path.home() / "Library" / "Caches" / "ClawdMochi"
    base = os.environ.get("XDG_CACHE_HOME")
    if base:
        return Path(base) / "clawd-mochi"
    return Path.home() / ".cache" / "clawd-mochi"


def cache_file() -> Path:
    return app_cache_dir() / CACHE_FILE_NAME


def load_cache() -> dict:
    try:
        with cache_file().open("r", encoding="utf-8") as f:
            data = json.load(f)
            return data if isinstance(data, dict) else {}
    except (FileNotFoundError, json.JSONDecodeError, OSError):
        return {}


def save_cache(data: dict) -> None:
    path = cache_file()
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        tmp = path.with_suffix(path.suffix + ".tmp")
        with tmp.open("w", encoding="utf-8") as f:
            json.dump(data, f, ensure_ascii=False, indent=2, sort_keys=True)
            f.write("\n")
        os.replace(tmp, path)
    except OSError:
        pass


def read_cache_value(key: str) -> str | None:
    entry = load_cache().get(key)
    if not isinstance(entry, dict):
        return None
    value = entry.get("value")
    updated_at = entry.get("updated_at", 0)
    try:
        if isinstance(value, str) and value and time.time() - float(updated_at) < 86400:
            return value
    except (TypeError, ValueError):
        return None
    return None


def write_cache_value(key: str, value: str | None) -> None:
    data = load_cache()
    if value:
        data[key] = {"value": value, "updated_at": time.time()}
    else:
        data.pop(key, None)
    save_cache(data)


def serial_candidates() -> list[str]:
    env_port = os.environ.get("CLAWD_MOCHI_PORT") or os.environ.get("ESP32_PORT")
    try:
        from serial.tools import list_ports
        found = [p.device for p in list_ports.comports()]
    except Exception:
        found = []
    ports = ([env_port] if env_port else []) + found
    seen: set[str] = set()
    return [p for p in ports if not (p in seen or seen.add(p))]


def probe_serial_port(ser) -> bool:
    try:
        ser.reset_input_buffer()
        ser.write(PROBE_MSG)
        ser.flush()
        return ser.readline().strip() == b"CC:pong"
    except Exception:
        return False


def find_serial_port() -> str | None:
    """复用 cc_hook 的探测逻辑，命中后写缓存。"""
    if os.environ.get("CLAWD_MOCHI_NO_SERIAL"):
        return None
    try:
        import serial
    except Exception:
        return None

    cached = read_cache_value("serial_port")
    if cached:
        try:
            with serial.Serial(cached, SERIAL_BAUD, timeout=SERIAL_PROBE_TIMEOUT, write_timeout=SERIAL_PROBE_TIMEOUT) as ser:
                if probe_serial_port(ser):
                    return cached
        except Exception:
            pass
        write_cache_value("serial_port", "")

    for port in serial_candidates():
        try:
            with serial.Serial(port, SERIAL_BAUD, timeout=SERIAL_PROBE_TIMEOUT, write_timeout=SERIAL_PROBE_TIMEOUT) as ser:
                if probe_serial_port(ser):
                    write_cache_value("serial_port", port)
                    return port
        except Exception:
            continue
    return None


def open_serial(port: str):
    """打开串口常驻。打开本身会触发一次复位，属正常启动成本，之后常开不再复位。"""
    import serial
    ser = serial.Serial(port, SERIAL_BAUD, timeout=SERIAL_TIMEOUT, write_timeout=SERIAL_TIMEOUT)
    # 给固件一点时间走完 BOOT → SERIAL_IDLE，期间吃掉启动日志
    t0 = time.time()
    while time.time() - t0 < 2.5:
        try:
            ser.readline()
        except Exception:
            break
    return ser


def run_daemon() -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((LISTEN_HOST, LISTEN_PORT))
    sock.settimeout(1.0)
    print(f"[daemon] listening udp {LISTEN_HOST}:{LISTEN_PORT}", flush=True)

    ser = None
    port: str | None = None
    last_sweep = 0.0
    buf = bytearray()

    while True:
        # 1) 没有端口就周期性找
        if port is None and time.time() - last_sweep > SWEEP_INTERVAL:
            last_sweep = time.time()
            port = find_serial_port()
            if port:
                print(f"[daemon] found device on {port}", flush=True)
            else:
                print("[daemon] no device yet, waiting...", flush=True)

        # 2) 有端口但串口没开/掉了，尝试开
        if port and ser is None:
            try:
                ser = open_serial(port)
                print(f"[daemon] serial open on {port} (kept open, no more resets)", flush=True)
            except Exception as exc:
                print(f"[daemon] open failed {port}: {exc}", flush=True)
                write_cache_value("serial_port", "")
                port = None
                ser = None
                time.sleep(REOPEN_INTERVAL)
                continue

        # 3) 收 hook 的 UDP，转发到串口
        try:
            data, addr = sock.recvfrom(2048)
        except socket.timeout:
            continue
        except OSError:
            time.sleep(0.5)
            continue

        if ser is None:
            continue  # 没设备，丢弃

        # data 末尾若没 \n 补一个
        if not data.endswith(b"\n"):
            data = data + b"\n"
        try:
            ser.write(data)
            ser.flush()
            # 给 hook 回个 ack，让它知道 daemon 在(否则 hook 会回退自己开串口→复位)
            try:
                sock.sendto(b"CC:ok\n", addr)
            except OSError:
                pass
        except Exception as exc:
            print(f"[daemon] write failed: {exc}, will reopen", flush=True)
            try:
                ser.close()
            except Exception:
                pass
            ser = None
            write_cache_value("serial_port", "")
            port = None


def exe_dir() -> Path:
    return Path(__file__).resolve().parent


def uv_path() -> str:
    found = os.environ.get("UV") or shutil_which("uv")
    return found or "uv"


def shutil_which(name: str) -> str | None:
    import shutil
    return shutil.which(name)


# ---------- Windows 开机自启 ----------
SCHTASK_NAME = "ClawdMochiSerialDaemon"


def install_autostart() -> None:
    if os.name != "nt":
        print("--install only supported on Windows (use your DE's autostart for unix)")
        sys.exit(1)
    uv = uv_path()
    script = exe_dir() / "cc_serial_daemon.py"
    # 任务计划：用户登录时启动，无窗口(/B)，失败重启
    cmd = [
        "schtasks", "/Create", "/TN", SCHTASK_NAME, "/F",
        "/SC", "ONLOGON", "/RL", "HIGHEST",
        "/TR", f'"{uv}" run --script "{script}"',
    ]
    print(" ".join(cmd))
    rc = subprocess.call(cmd, shell=False)
    if rc == 0:
        print(f"Installed scheduled task '{SCHTASK_NAME}' (runs on user logon)")
        # 立即起一次
        subprocess.Popen([uv, "run", "--script", str(script)],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                         stdin=subprocess.DEVNULL, close_fds=True,
                         creationflags=getattr(subprocess, "DETACHED_PROCESS", 0) | getattr(subprocess, "CREATE_NO_WINDOW", 0))
        print("Started daemon in background")
    else:
        print(f"schtasks failed (rc={rc})", file=sys.stderr)
        sys.exit(rc)


def uninstall_autostart() -> None:
    if os.name != "nt":
        print("--uninstall only supported on Windows")
        sys.exit(1)
    subprocess.call(["schtasks", "/End", "/TN", SCHTASK_NAME])
    rc = subprocess.call(["schtasks", "/Delete", "/TN", SCHTASK_NAME, "/F"])
    print(f"Removed scheduled task '{SCHTASK_NAME}' (rc={rc})")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--install", action="store_true", help="装为 Windows 开机自启并立即后台启动")
    ap.add_argument("--uninstall", action="store_true", help="卸载开机自启")
    args = ap.parse_args()
    if args.install:
        install_autostart(); return
    if args.uninstall:
        uninstall_autostart(); return
    try:
        run_daemon()
    except KeyboardInterrupt:
        print("\n[daemon] stopped")


if __name__ == "__main__":
    main()

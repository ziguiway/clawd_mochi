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
SERIAL_BAUD = 115200
SERIAL_TIMEOUT = 0.2
SERIAL_PROBE_TIMEOUT = 0.4
REOPEN_INTERVAL = 2.0       # 端口掉线后重试间隔(秒)
SWEEP_INTERVAL = 30.0       # 无端口时重新扫描的间隔(秒)
CACHE_FILE_NAME = "cc_hook_cache.json"
# ==========================

PROBE_MSG = b"CC:ping"  # 不带换行:固件 strcmp(data+3,"ping") 精确匹配,带\n 会让 ping 拦截失效
ESP32_PORT = 4210           # ESP32 的 UDP 状态端口(hook 发状态 + ping 探测用)
UDP_PROBE_TIMEOUT = 0.3     # 单个 IP 的 UDP 探测超时(秒)


def parse_pong(data: bytes) -> str | None:
    """解析 CC:pong[:<mode>] 握手响应，返回模式名(None 表示不是 Clawd Mochi)。

    兼容旧固件只回 CC:pong(无模式后缀)的情况——视为 unknown 模式。
    先 strip 掉串口 readline 带的换行,再判断。
    """
    text = data.decode("ascii", "ignore").strip()
    if text == "CC:pong":
        return "unknown"
    if text.startswith("CC:pong:"):
        mode = text[len("CC:pong:"):].strip().lower()
        return mode or "unknown"
    return None


def probe_host_udp(ip: str) -> str | None:
    """UDP ping 设备拿模式,不开串口(避免触发 ESP32-C3 复位)。

    返回模式(lan/serial/unknown),探测不到返回 None。
    """
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(UDP_PROBE_TIMEOUT)
        sock.sendto(PROBE_MSG, (ip, ESP32_PORT))
        data, _ = sock.recvfrom(1024)
        sock.close()
        return parse_pong(data)
    except (socket.timeout, OSError):
        try:
            sock.close()
        except Exception:
            pass
        return None


def find_device_mode() -> tuple[str | None, str | None]:
    """探测设备当前模式,UDP 优先(不碰串口),失败才回退串口探测。

    返回 (port, mode)。port 在 LAN 模式下为 None(设备走 UDP,无串口),
    SERIAL 模式下是串口名。探测不到返回 (None, None)。

    关键:LAN 模式必须用 UDP 探测拿到——开串口探测会触发 ESP32-C3 复位,
    复位后 MODE_SELECT 检测到串口活跃会把设备拉回 SERIAL 模式,永远探不到 LAN。
    """
    # 1) 先用缓存 IP 做 UDP 探测(最快,不碰串口)
    cached_ip = read_cache_value("ip")
    if cached_ip:
        mode = probe_host_udp(cached_ip)
        if mode is not None:
            write_cache_value("ip_mode", mode)
            if mode == "lan":
                return None, "lan"   # LAN 模式:设备走 UDP,不需要串口
            # serial/unknown:继续开串口,但模式已确认
            port = read_cache_value("serial_port")
            if port:
                return port, mode
            # 没有缓存串口,fallback 到串口扫描(此时设备已在 serial 模式,开串口不会破坏)
            return find_serial_port() or (None, mode)

    # 2) UDP 探测不到(没缓存 IP 或超时):回退串口探测
    #    此时设备很可能在 SERIAL 模式(没连 WiFi),开串口探测安全
    return find_serial_port()


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


def probe_serial_port(ser) -> str | None:
    """串口握手：发 CC:ping，期望 CC:pong[:<mode>] 确认是 Clawd Mochi 设备。

    返回设备当前模式(lan/serial/unknown),非本设备返回 None。
    """
    try:
        ser.reset_input_buffer()
        ser.write(PROBE_MSG)
        ser.flush()
        return parse_pong(ser.readline())
    except Exception:
        return None


def find_serial_port() -> tuple[str, str] | None:
    """复用 cc_hook 的探测逻辑，命中后写端口+模式缓存。

    返回 (port, mode)。模式写入 serial_mode 缓存,供 hook 的 device_mode() 读取,
    这样 hook 在 LAN 模式时能跳过 daemon 直接走 UDP。
    """
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
                mode = probe_serial_port(ser)
                if mode is not None:
                    write_cache_value("serial_mode", mode)
                    return cached, mode
        except Exception:
            pass
        write_cache_value("serial_port", "")
        write_cache_value("serial_mode", None)

    for port in serial_candidates():
        try:
            with serial.Serial(port, SERIAL_BAUD, timeout=SERIAL_PROBE_TIMEOUT, write_timeout=SERIAL_PROBE_TIMEOUT) as ser:
                mode = probe_serial_port(ser)
                if mode is not None:
                    write_cache_value("serial_port", port)
                    write_cache_value("serial_mode", mode)
                    return port, mode
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
    mode: str | None = None      # 设备当前模式: lan/serial/unknown
    last_sweep = 0.0

    while True:
        # 1) 周期性(重新)探测设备:没端口,或处于 LAN 待机(设备可能切回 SERIAL)
        need_probe = port is None or mode == "lan"
        if need_probe and time.time() - last_sweep > SWEEP_INTERVAL:
            last_sweep = time.time()
            found = find_device_mode()
            if found and found[1] is not None:
                port, mode = found
                if mode == "lan":
                    # 设备在 LAN 模式:不开串口(开了会触发复位把设备拉回 SERIAL),
                    # 也不接管 hook 的转发(不回 CC:ok → hook 超时 → 走 UDP)。
                    # 关掉可能已开的串口,让设备安心待在 LAN 模式。
                    # 清掉 serial_mode 缓存:device_mode() 优先读 IP 模式(lan),
                    # 避免残留的 serial 让 hook 继续走 daemon。
                    if ser is not None:
                        try:
                            ser.close()
                        except Exception:
                            pass
                        ser = None
                    write_cache_value("serial_mode", None)
                    print("[daemon] device in LAN mode, standing by (hook uses UDP)", flush=True)
                else:
                    print(f"[daemon] found device on {port} (mode={mode})", flush=True)
            else:
                print("[daemon] no device yet, waiting...", flush=True)

        # 2) SERIAL 模式:有端口但串口没开/掉了,尝试开
        if port and mode != "lan" and ser is None:
            try:
                ser = open_serial(port)
                print(f"[daemon] serial open on {port} (kept open, no more resets)", flush=True)
            except Exception as exc:
                print(f"[daemon] open failed {port}: {exc}", flush=True)
                write_cache_value("serial_port", "")
                write_cache_value("serial_mode", None)
                port = None
                mode = None
                ser = None
                time.sleep(REOPEN_INTERVAL)
                continue

        # 3) 收 hook 的 UDP
        try:
            data, addr = sock.recvfrom(2048)
        except socket.timeout:
            continue
        except OSError:
            time.sleep(0.5)
            continue

        # LAN 模式:不接管,不回 ack → hook 的 send_to_daemon 超时 → 回退走 UDP
        if mode == "lan" or ser is None:
            continue

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
            write_cache_value("serial_mode", None)
            port = None
            mode = None


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

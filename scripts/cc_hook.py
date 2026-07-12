#!/usr/bin/env python3
"""Claude Code Hook — send status events to Clawd Mochi over USB serial or UDP."""

# /// script
# requires-python = ">=3.10"
# dependencies = [
#   "pyserial>=3.5",
# ]
# ///

import sys
import json
import socket
import os
import time
import ipaddress
import concurrent.futures
from pathlib import Path

# ========== 配置 ==========
ESP32_PORT = 4210
DAEMON_HOST = "127.0.0.1"
DAEMON_PORT = 4211          # 本地串口守护进程(cc_serial_daemon.py)的 UDP 端口
DAEMON_REPLY_TIMEOUT = 0.15  # 等 daemon 回 CC:ok 的超时(秒)
CACHE_TTL = 86400  # 24 小时
SCAN_TIMEOUT = 0.3  # 每个 IP 的 UDP 探测超时(秒)
SCAN_WORKERS = 64   # 并发扫描线程数
SERIAL_BAUD = 115200
SERIAL_TIMEOUT = 0.2
SERIAL_PROBE_TIMEOUT = 0.4  # 串口握手 CC:ping→CC:pong 超时
CACHE_FILE_NAME = "cc_hook_cache.json"
# ==========================

HOOK_MAP = {
    # 会话生命周期
    "SessionStart":      "session_start",
    "Setup":             "session_start",
    "SessionEnd":        "session_end",
    # 用户交互
    "UserPromptSubmit":  "thinking",
    "PermissionRequest": "permission",
    # 工具调用
    "PreToolUse":        "working",
    "PostToolUse":       "working",
    "PostToolUseFailure":"error",
    "PostToolBatch":     "working",
    # 子代理
    "SubagentStart":     "working",
    "SubagentStop":      "working",
    # 任务
    "TaskCreated":       "working",
    "TaskCompleted":     "working",
    # 上下文压缩
    "PreCompact":        "COMPACTING",
    "PostCompact":       "COMPACTING",
    # 工作树
    "WorktreeCreate":    "working",
    "WorktreeRemove":    "working",
    # 响应结束
    "Stop":              "done",
    "StopFailure":       "error",
}

# CC:ping 探测包，ESP32 收到后只刷新计时不改状态
PROBE_MSG = b"CC:ping"


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
    path = cache_file()
    try:
        with path.open("r", encoding="utf-8") as f:
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
        if isinstance(value, str) and value and time.time() - float(updated_at) < CACHE_TTL:
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


def clean_field(value: object, limit: int = 48) -> str:
    """Keep the firmware's comma-delimited packet parseable and compact."""
    if value is None:
        return ""
    text = str(value).replace("\n", " ").replace("\r", " ").replace(",", " ")
    return text[:limit]


def summarize_tool_input(value: object) -> str:
    if value is None:
        return ""
    if isinstance(value, dict):
        for key in ("command", "file_path", "path", "pattern", "url", "prompt"):
            if key in value:
                return clean_field(value[key], 64)
        return clean_field(json.dumps(value, ensure_ascii=False), 64)
    return clean_field(value, 64)


def extract_tool_name(data: dict) -> str:
    """Return Claude Code's actual tool for this hook, not the matcher pattern."""
    for key in ("tool_name", "toolName", "tool"):
        value = data.get(key)
        if isinstance(value, str) and value:
            return value
    tool_input = data.get("tool_input")
    if isinstance(tool_input, dict):
        value = tool_input.get("tool_name") or tool_input.get("toolName")
        if isinstance(value, str) and value:
            return value
    return ""


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
    """串口握手：发 CC:ping，期望 CC:pong 确认是 Clawd Mochi 设备"""
    try:
        ser.reset_input_buffer()
        ser.write(b"CC:ping\n")
        ser.flush()
        line = ser.readline()
        return line.strip() == b"CC:pong"
    except Exception:
        return False


def read_serial_cache() -> str | None:
    return read_cache_value("serial_port")


def write_serial_cache(port: str) -> None:
    write_cache_value("serial_port", port)


def find_serial_port() -> str | None:
    """查找 Clawd Mochi 串口：缓存 → 探测所有候选端口"""
    if os.environ.get("CLAWD_MOCHI_NO_SERIAL"):
        return None

    try:
        import serial
    except Exception:
        return None

    cached = read_serial_cache()
    if cached:
        try:
            with serial.Serial(cached, SERIAL_BAUD, timeout=SERIAL_PROBE_TIMEOUT, write_timeout=SERIAL_PROBE_TIMEOUT) as ser:
                if probe_serial_port(ser):
                    return cached
        except Exception:
            pass
        write_serial_cache("")  # 缓存失效，清空

    for port in serial_candidates():
        try:
            with serial.Serial(port, SERIAL_BAUD, timeout=SERIAL_PROBE_TIMEOUT, write_timeout=SERIAL_PROBE_TIMEOUT) as ser:
                if probe_serial_port(ser):
                    write_serial_cache(port)
                    return port
        except Exception:
            continue
    return None


def send_to_daemon(msg: str) -> bool:
    """走常驻串口守护进程转发，避免反复开关 COM 触发 ESP32-C3 复位。

    daemon 收到后会回 CC:ok。命中返回 True，未命中(daemon 没跑/超时)返回 False，
    由调用方回退到 send_serial() 自己开串口。
    """
    if os.environ.get("CLAWD_MOCHI_NO_DAEMON"):
        return False
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(DAEMON_REPLY_TIMEOUT)
        sock.connect((DAEMON_HOST, DAEMON_PORT))
        sock.sendall((msg + "\n").encode())
        try:
            data = sock.recv(64)
        except (socket.timeout, OSError):
            return False
        finally:
            sock.close()
        return data.strip() == b"CC:ok"
    except OSError:
        return False


def send_serial(msg: str) -> bool:
    port = find_serial_port()
    if not port:
        return False
    try:
        import serial
        with serial.Serial(port, SERIAL_BAUD, timeout=SERIAL_TIMEOUT, write_timeout=SERIAL_TIMEOUT) as ser:
            ser.write((msg + "\n").encode())
            ser.flush()
        return True
    except Exception:
        write_serial_cache("")  # 端口失效，下次重新探测
        return False


def get_local_subnet() -> str | None:
    """获取本机所在 /24 子网"""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        local_ip = s.getsockname()[0]
        s.close()
        net = ipaddress.ip_network(f"{local_ip}/24", strict=False)
        return str(net)
    except Exception:
        return None


def probe_host(ip: str) -> bool:
    """UDP 探测：发 CC:ping，等待 CC:pong 响应确认是 ESP32"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(SCAN_TIMEOUT)
        sock.sendto(PROBE_MSG, (ip, ESP32_PORT))
        data, _ = sock.recvfrom(1024)
        sock.close()
        return data == b"CC:pong"
    except (socket.timeout, OSError):
        return False


def scan_for_esp32() -> str | None:
    """并发扫描局域网，找开放 UDP:4210 的设备"""
    subnet = get_local_subnet()
    if not subnet:
        return None

    hosts = [str(h) for h in ipaddress.ip_network(subnet).hosts()]

    try:
        with concurrent.futures.ThreadPoolExecutor(max_workers=SCAN_WORKERS) as pool:
            futures = {pool.submit(probe_host, ip): ip for ip in hosts}
            for future in concurrent.futures.as_completed(futures, timeout=5):
                ip = futures[future]
                try:
                    if future.result():
                        # 找到一个就返回（UDP 无连接确认，但局域网内够用）
                        return ip
                except Exception:
                    continue
    except concurrent.futures.TimeoutError:
        return None
    return None


def read_ip_cache() -> str | None:
    """读取缓存 IP，过期返回 None"""
    return read_cache_value("ip")


def write_ip_cache(ip: str) -> None:
    """写入缓存"""
    write_cache_value("ip", ip)


def find_esp32() -> str | None:
    """查找 ESP32 IP：缓存 → 扫描"""
    if os.environ.get("CLAWD_MOCHI_NO_NETWORK"):
        return None

    ip = read_ip_cache()
    if ip:
        return ip

    ip = scan_for_esp32()
    if ip:
        write_ip_cache(ip)
    return ip


def main() -> None:
    data: dict = {}
    if not sys.stdin.isatty():
        try:
            data = json.load(sys.stdin)
        except json.JSONDecodeError:
            pass

    hook = sys.argv[1] if len(sys.argv) > 1 else clean_field(data.get("hook_event_name"), 48)
    event = HOOK_MAP.get(hook)
    if not event:
        sys.exit(0)

    tool = extract_tool_name(data)
    detail = summarize_tool_input(data.get("tool_input") or data.get("prompt") or data.get("message"))
    model = data.get("model", "") or os.environ.get("CLAUDE_MODEL", "") or os.environ.get("ANTHROPIC_MODEL", "")
    msg = ",".join([
        f"CC:{event}",
        clean_field(hook, 31),
        clean_field(tool, 31),
        clean_field(detail, 63),
        clean_field(model, 31),
    ])

    if send_to_daemon(msg):
        return

    if send_serial(msg):
        return

    ip = find_esp32()
    if not ip:
        sys.exit(0)

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.sendto(msg.encode(), (ip, ESP32_PORT))
        sock.close()
    except OSError:
        pass


if __name__ == "__main__":
    main()

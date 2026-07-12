#!/usr/bin/env python3
"""Claude Code Hook — send status events to Clawd Mochi over WiFi UDP.

固件默认 LAN(WiFi)模式:监听 UDP:4210。本脚本把 Claude Code 的 hook 事件
压缩成短包发过去。串口路径已移除(ESP32-C3 USB-CDC 在 Windows 关 COM 会触发
硬件复位,且 USB 插着会让设备进串口模式,与 LAN 默认模式冲突)。
"""

# /// script
# requires-python = ">=3.10"
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
CACHE_TTL = 86400  # 24 小时
SCAN_TIMEOUT = 0.3  # 每个 IP 的 UDP 探测超时(秒)
SCAN_WORKERS = 64   # 并发扫描线程数
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

# CC:ping 探测包，ESP32 收到后只刷新计时不改状态,回 CC:pong[:<mode>]
# 注意:不能带换行。固件用 strcmp(data+3,"ping") 精确匹配,带 \n 会变成
# "ping\n" 匹配失败,ping 拦截失效,被当普通状态包解析(误置 WORKING)且不回 pong。
PROBE_MSG = b"CC:ping"


def parse_pong(data: bytes) -> str | None:
    """解析 CC:pong[:<mode>] 握手响应，返回模式名(None 表示不是 Clawd Mochi)。

    兼容旧固件只回 CC:pong(无模式后缀)的情况——视为 unknown 模式。
    """
    text = data.decode("ascii", "ignore").strip()
    if text == "CC:pong":
        return "unknown"
    if text.startswith("CC:pong:"):
        mode = text[len("CC:pong:"):].strip().lower()
        return mode or "unknown"
    return None


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


def probe_host(ip: str) -> str | None:
    """UDP 探测：发 CC:ping，等待 CC:pong[:<mode>] 响应确认是 ESP32。

    返回设备当前模式(lan/serial/unknown),非本设备返回 None。
    """
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(SCAN_TIMEOUT)
        sock.sendto(PROBE_MSG, (ip, ESP32_PORT))
        data, _ = sock.recvfrom(1024)
        sock.close()
        return parse_pong(data)
    except (socket.timeout, OSError):
        return None


def scan_for_esp32() -> tuple[str, str] | None:
    """并发扫描局域网，找开放 UDP:4210 的 Clawd Mochi 设备。

    返回 (ip, mode),找不到返回 None。
    """
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
                    mode = future.result()
                    if mode is not None:
                        # 找到一个就返回（UDP 无连接确认，但局域网内够用）
                        return ip, mode
                except Exception:
                    continue
    except concurrent.futures.TimeoutError:
        return None
    return None


def find_esp32() -> tuple[str, str] | None:
    """查找 ESP32：缓存 IP → 扫描。返回 (ip, mode) 或 None。

    缓存命中时直接用缓存的 IP 发包(UDP 无连接,发包本身就是探测),
    缓存过期或为空时才扫描子网。
    """
    if os.environ.get("CLAWD_MOCHI_NO_NETWORK"):
        return None

    ip = read_cache_value("ip")
    mode = read_cache_value("ip_mode")
    if ip:
        return ip, (mode or "unknown")

    found = scan_for_esp32()
    if found:
        ip, mode = found
        write_cache_value("ip", ip)
        write_cache_value("ip_mode", mode)
    return found


def send_to_esp32(msg: str) -> bool:
    """走 UDP 把状态包发给 ESP32。找不到设备返回 False。"""
    found = find_esp32()
    if not found:
        return False
    ip, _mode = found
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.sendto(msg.encode(), (ip, ESP32_PORT))
        sock.close()
        return True
    except OSError:
        # 缓存 IP 失效,清掉让下次重新扫描
        write_cache_value("ip", None)
        write_cache_value("ip_mode", None)
        return False


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

    send_to_esp32(msg)


if __name__ == "__main__":
    main()

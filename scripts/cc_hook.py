#!/usr/bin/env python3
"""
Claude Code Hook — 通过 UDP 向 Cube-X 发送状态事件

自动发现：首次运行扫描局域网找 ESP32，结果缓存到 ~/.cc_cube_x_ip
缓存有效期 24h，过期自动重新扫描。

用法：python3 cc_hook.py <HookEventName>
"""

import sys
import json
import socket
import os
import time
import ipaddress
import concurrent.futures

# ========== 配置 ==========
ESP32_PORT = 4210
CACHE_FILE = os.path.expanduser("~/.cc_cube_x_ip")
CACHE_TTL = 86400  # 24 小时
SCAN_TIMEOUT = 0.3  # 每个 IP 的 UDP 探测超时(秒)
SCAN_WORKERS = 64   # 并发扫描线程数
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
    "PreCompact":        "sweeping",
    "PostCompact":       "working",
    # 工作树
    "WorktreeCreate":    "working",
    "WorktreeRemove":    "working",
    # 响应结束
    "Stop":              "done",
    "StopFailure":       "error",
}

# CC:ping 探测包，ESP32 收到后只刷新计时不改状态
PROBE_MSG = b"CC:ping"


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
    return None


def read_cache() -> str | None:
    """读取缓存 IP，过期返回 None"""
    try:
        with open(CACHE_FILE, "r") as f:
            ts_str, ip = f.read().strip().split(",", 1)
            if time.time() - float(ts_str) < CACHE_TTL:
                return ip
    except (FileNotFoundError, ValueError):
        pass
    return None


def write_cache(ip: str) -> None:
    """写入缓存"""
    try:
        with open(CACHE_FILE, "w") as f:
            f.write(f"{time.time()},{ip}\n")
    except OSError:
        pass


def find_esp32() -> str | None:
    """查找 ESP32 IP：缓存 → 扫描"""
    ip = read_cache()
    if ip:
        return ip

    ip = scan_for_esp32()
    if ip:
        write_cache(ip)
    return ip


def main() -> None:
    hook = sys.argv[1] if len(sys.argv) > 1 else ""
    event = HOOK_MAP.get(hook)
    if not event:
        sys.exit(0)

    data: dict = {}
    if not sys.stdin.isatty():
        try:
            data = json.load(sys.stdin)
        except json.JSONDecodeError:
            pass

    detail = data.get("tool_name", "")
    model = data.get("model", "") or os.environ.get("CLAUDE_MODEL", "") or os.environ.get("ANTHROPIC_MODEL", "")
    msg = f"CC:{event}"
    if detail or model:
        msg += f",{detail}"
    if model:
        msg += f",{model}"

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

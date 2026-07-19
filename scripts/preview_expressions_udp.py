#!/usr/bin/env python3
"""Preview Clawd Mochi Claude status faces over UDP.

Firmware packet format:
  CC:<event>,<hook>,<tool>,<detail>,<model>

The device listens on UDP port 4210 in LAN mode. This script sends a short
sequence that enters the Claude status panel, then walks through each status.
"""

from __future__ import annotations

import argparse
import ipaddress
import socket
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed


PORT = 4210
PING = b"CC:ping"
DEFAULT_DELAY_SEC = 2.5
PROBE_TIMEOUT_SEC = 0.35
SCAN_WORKERS = 64

STATUS_SEQUENCE = [
    ("thinking", "thinking eye offset"),
    ("working", "focused working eyes"),
    ("permission", "waiting narrow eyes"),
    ("sweeping", "compacting eyes"),
    ("done", "done squish eyes"),
    ("working", "prime before error"),
    ("error", "error cross eyes"),
    ("thinking", "prime before sleep"),
    ("sleeping", "sleeping closed eyes"),
]


def clean_field(value: str, limit: int) -> str:
    return value.replace("\n", " ").replace("\r", " ").replace(",", " ")[:limit]


def packet(event: str) -> bytes:
    fields = [
        f"CC:{clean_field(event, 15)}",
        "preview",
        "udp",
        clean_field(f"face {event}", 63),
        "codex",
    ]
    return ",".join(fields).encode("utf-8")


def parse_pong(data: bytes) -> str | None:
    text = data.decode("ascii", "ignore").strip()
    if text == "CC:pong":
        return "unknown"
    if text.startswith("CC:pong:"):
        return text[len("CC:pong:") :].strip() or "unknown"
    return None


def local_subnet() -> ipaddress.IPv4Network | None:
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.connect(("8.8.8.8", 80))
            local_ip = sock.getsockname()[0]
        return ipaddress.ip_network(f"{local_ip}/24", strict=False)
    except OSError:
        return None


def probe(host: str, port: int) -> tuple[str, str] | None:
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.settimeout(PROBE_TIMEOUT_SEC)
            sock.sendto(PING, (host, port))
            data, addr = sock.recvfrom(1024)
        mode = parse_pong(data)
        if mode is None:
            return None
        return addr[0], mode
    except OSError:
        return None


def resolve_host(host: str) -> str:
    return socket.gethostbyname(host)


def discover(port: int) -> list[tuple[str, str]]:
    candidates: list[str] = []

    for name in ("clawd-mochi.local", "clawd-mochi"):
        try:
            candidates.append(resolve_host(name))
        except OSError:
            pass

    subnet = local_subnet()
    if subnet is not None:
        candidates.extend(str(ip) for ip in subnet.hosts())

    seen: set[str] = set()
    unique_candidates = [ip for ip in candidates if not (ip in seen or seen.add(ip))]
    if not unique_candidates:
        return []

    found: dict[str, str] = {}
    with ThreadPoolExecutor(max_workers=SCAN_WORKERS) as pool:
        futures = {pool.submit(probe, ip, port): ip for ip in unique_candidates}
        for future in as_completed(futures):
            result = future.result()
            if result is None:
                continue
            ip, mode = result
            found[ip] = mode
    return sorted(found.items())


def send(host: str, port: int, event: str) -> None:
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.sendto(packet(event), (host, port))


def run_sequence(targets: list[str], port: int, delay: float, repeat: int) -> None:
    loop = 0
    while repeat == 0 or loop < repeat:
        loop += 1
        if repeat != 1:
            print(f"\nloop {loop}" if repeat else f"\nloop {loop} (Ctrl+C to stop)")

        for event, note in STATUS_SEQUENCE:
            print(f"send {event:<10} {note}")
            for host in targets:
                send(host, port, event)
            time.sleep(delay)

        # Return to idle between finite loops. Idle will leave the info panel.
        if repeat != 0:
            print("send idle       return to normal eyes")
            for host in targets:
                send(host, port, "idle")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Preview all Clawd Mochi Claude status expressions via UDP."
    )
    parser.add_argument("--host", "-H", action="append", help="Device IP/hostname. Can be passed more than once.")
    parser.add_argument("--port", "-p", type=int, default=PORT, help=f"UDP port, default {PORT}.")
    parser.add_argument("--delay", "-d", type=float, default=DEFAULT_DELAY_SEC, help="Seconds to hold each face.")
    parser.add_argument("--repeat", "-r", type=int, default=1, help="Repeat count. Use 0 for infinite loop.")
    parser.add_argument("--discover-only", action="store_true", help="Only discover devices, do not send preview packets.")
    args = parser.parse_args()

    devices: list[tuple[str, str]]
    if args.host:
        devices = []
        for host in args.host:
            ip = resolve_host(host)
            mode = "specified"
            probed = probe(ip, args.port)
            if probed is not None:
                ip, mode = probed
            devices.append((ip, mode))
    else:
        print("discovering Clawd Mochi devices over UDP...")
        devices = discover(args.port)

    if not devices:
        print("No Clawd Mochi device found over UDP.", file=sys.stderr)
        print("Try: python scripts/preview_expressions_udp.py --host <device-ip>", file=sys.stderr)
        return 2

    for ip, mode in devices:
        print(f"device {ip} mode={mode}")

    if args.discover_only:
        return 0

    targets = [ip for ip, _mode in devices]
    try:
        run_sequence(targets, args.port, max(0.2, args.delay), max(0, args.repeat))
    except KeyboardInterrupt:
        print("\nstopped")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

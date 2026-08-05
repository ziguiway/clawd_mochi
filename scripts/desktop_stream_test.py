#!/usr/bin/env python3
"""桌面推流链路验证脚本 (M1)。

在 Electron 上位机完成前, 用这个脚本验证 ESP32 固件的桌面投流视图:
POST /stream/enter -> TCP 3333 推 JPEG 帧 -> Ctrl+C 退出时 POST /stream/exit。

用法:
    uv run scripts/desktop_stream_test.py --ip 192.168.1.42
    uv run scripts/desktop_stream_test.py --ip 192.168.1.42 --fps 8 --quality 50 --mode full
    uv run scripts/desktop_stream_test.py --ip 192.168.1.42 --mode cursor

依赖: mss, Pillow (uv run --with mss,pillow ...)
协议: 每帧 = b"ESPF" + uint32 LE 长度 + JPEG 数据 (240x240)。
模式: cursor=鼠标跟随裁切(默认) / full=全屏缩放 / region=固定左上角 480x480 区域。
"""
# /// script
# dependencies = ["mss", "pillow"]
# ///

import argparse
import io
import socket
import struct
import sys
import time
import urllib.request

import mss
from PIL import Image

FRAME_W = 240
FRAME_H = 240
MAGIC = b"ESPF"


def post(ip: str, path: str) -> None:
    req = urllib.request.Request(f"http://{ip}{path}", method="POST")
    with urllib.request.urlopen(req, timeout=5) as resp:
        resp.read()


def grab_frame(sct, monitor, mode: str, quality: int) -> bytes:
    shot = sct.grab(monitor)
    img = Image.frombytes("RGB", shot.size, shot.bgra, "raw", "BGRX")
    if mode == "cursor":
        # 源区域取 480x480 再缩到 240x240, 比 1:1 裁切视野更大
        half = 240
        cx, cy = img.width // 2, img.height // 2
        left = max(0, min(cx - half, img.width - half * 2))
        top = max(0, min(cy - half, img.height - half * 2))
        img = img.crop((left, top, left + half * 2, top + half * 2)).resize(
            (FRAME_W, FRAME_H), Image.LANCZOS)
    elif mode == "region":
        side = min(480, img.width, img.height)
        img = img.crop((0, 0, side, side)).resize((FRAME_W, FRAME_H), Image.LANCZOS)
    else:  # full
        side = min(img.width, img.height)
        left = (img.width - side) // 2
        top = (img.height - side) // 2
        img = img.crop((left, top, left + side, top + side)).resize(
            (FRAME_W, FRAME_H), Image.LANCZOS)
    buf = io.BytesIO()
    img.save(buf, "JPEG", quality=quality)
    return buf.getvalue()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ip", required=True, help="Mochi 设备 IP")
    ap.add_argument("--fps", type=float, default=8.0)
    ap.add_argument("--quality", type=int, default=50)
    ap.add_argument("--mode", choices=["cursor", "full", "region"], default="cursor")
    ap.add_argument("--duration", type=float, default=0, help="秒, 0=直到 Ctrl+C")
    args = ap.parse_args()

    print(f"enter stream mode on {args.ip} ...")
    post(args.ip, "/stream/enter")

    sock = socket.create_connection((args.ip, 3333), timeout=5)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    print(f"streaming {args.mode} @ {args.fps} fps q{args.quality}, Ctrl+C to stop")

    frame_interval = 1.0 / args.fps
    frames = 0
    started = time.monotonic()
    try:
        with mss.mss() as sct:
            monitor = sct.monitors[1]
            while True:
                t0 = time.monotonic()
                payload = grab_frame(sct, monitor, args.mode, args.quality)
                sock.sendall(MAGIC + struct.pack("<I", len(payload)) + payload)
                frames += 1
                if frames % 50 == 0:
                    print(f"  {frames} frames, {len(payload)} B last, "
                          f"{frames / (time.monotonic() - started):.1f} fps avg")
                if args.duration and time.monotonic() - started >= args.duration:
                    break
                dt = frame_interval - (time.monotonic() - t0)
                if dt > 0:
                    time.sleep(dt)
    except KeyboardInterrupt:
        pass
    finally:
        sock.close()
        try:
            post(args.ip, "/stream/exit")
        except OSError:
            pass
    print(f"done: {frames} frames in {time.monotonic() - started:.1f}s")
    return 0


if __name__ == "__main__":
    sys.exit(main())

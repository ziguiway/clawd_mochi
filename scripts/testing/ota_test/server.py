from __future__ import annotations

import hashlib
import json
import threading
from dataclasses import dataclass
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


@dataclass
class Release:
    version: str
    firmware: Path
    filesystem: Path | None = None
    corrupt_hash: bool = False
    truncate: bool = False

    def manifest(self, host: str, port: int) -> bytes:
        firmware = self.firmware.read_bytes()
        sha = hashlib.sha256(firmware).hexdigest()
        if self.corrupt_hash:
            sha = "0" * 64
        payload = {
            "schema": 1,
            "product": "clawd-mochi",
            "board": "esp32-c3-devkitc-02",
            "channel": "stable",
            "version": self.version,
            "firmware": {
                "url": f"http://{host}:{port}/firmware.bin",
                "size": len(firmware),
                "sha256": sha,
            },
            "releaseNotes": [f"OTA test release {self.version}"],
        }
        if self.filesystem:
            fs = self.filesystem.read_bytes()
            payload["filesystem"] = {
                "url": f"http://{host}:{port}/littlefs.bin",
                "size": len(fs),
                "sha256": hashlib.sha256(fs).hexdigest(),
            }
        return json.dumps(payload).encode()


class ManifestServer:
    def __init__(self, host: str, port: int, advertise_host: str) -> None:
        self.release: Release | None = None
        self.advertise_host = advertise_host

        owner = self

        class Handler(BaseHTTPRequestHandler):
            def do_GET(self) -> None:
                release = owner.release
                if not release:
                    self.send_error(404)
                    return
                if self.path == "/manifest.json":
                    body = release.manifest(owner.advertise_host, owner.server.server_port)
                    self.send_response(200)
                    self.send_header("Content-Type", "application/json")
                elif self.path == "/firmware.bin":
                    body = release.firmware.read_bytes()
                    if release.truncate:
                        body = body[: max(1, len(body) // 3)]
                    self.send_response(200)
                    self.send_header("Content-Type", "application/octet-stream")
                elif self.path == "/littlefs.bin" and release.filesystem:
                    body = release.filesystem.read_bytes()
                    self.send_response(200)
                    self.send_header("Content-Type", "application/octet-stream")
                else:
                    self.send_error(404)
                    return
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def log_message(self, *_args) -> None:
                return

        class ReusableHTTPServer(ThreadingHTTPServer):
            allow_reuse_address = True

        self.server = ReusableHTTPServer((host, port), Handler)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)

    @property
    def port(self) -> int:
        return self.server.server_port

    def start(self) -> None:
        self.thread.start()

    def close(self) -> None:
        self.server.shutdown()
        self.server.server_close()

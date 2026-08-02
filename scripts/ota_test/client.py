from __future__ import annotations

import http.client
import json
import time
import urllib.error
import urllib.request
from pathlib import Path


class OtaClient:
    def __init__(self, base_url: str, timeout: float = 12.0) -> None:
        self.base_url = base_url.rstrip("/")
        self.timeout = timeout

    def request(self, path: str, method: str = "GET", body: bytes | None = None,
                content_type: str = "application/json") -> tuple[int, dict]:
        request = urllib.request.Request(
            self.base_url + path, data=body, method=method,
            headers={"Content-Type": content_type, "Cache-Control": "no-store"},
        )
        try:
            with urllib.request.urlopen(request, timeout=self.timeout) as response:
                payload = response.read()
                return response.status, json.loads(payload or b"{}")
        except (urllib.error.URLError, ConnectionError, TimeoutError) as exc:
            raise RuntimeError(str(exc)) from exc

    def status(self) -> dict:
        return self.request("/ota/status")[1]

    def fetch(self, path: str) -> bytes:
        with urllib.request.urlopen(self.base_url + path, timeout=self.timeout) as response:
            if response.status != 200:
                raise AssertionError(f"GET {path} returned {response.status}")
            return response.read()

    def check(self) -> dict:
        return self.request("/ota/check", "POST", b"{}")[1]

    def install(self) -> dict:
        return self.request("/ota/install", "POST", b"{}")[1]

    def upload(self, path: Path) -> dict:
        boundary = "----clawd-mochi-ota-test"
        data = path.read_bytes()
        body = (
            f"--{boundary}\r\nContent-Disposition: form-data; name=\"file\"; "
            f"filename=\"{path.name}\"\r\nContent-Type: application/octet-stream\r\n\r\n"
        ).encode() + data + f"\r\n--{boundary}--\r\n".encode()
        return self.request(
            "/ota/upload", "POST", body,
            f"multipart/form-data; boundary={boundary}",
        )[1]

    def interrupt_upload(self, path: Path, cutoff: int = 65536) -> None:
        parsed = urllib.request.urlparse(self.base_url)
        connection = http.client.HTTPConnection(parsed.hostname, parsed.port, timeout=5)
        data = path.read_bytes()[:cutoff]
        boundary = "----clawd-mochi-ota-interrupt"
        prefix = (
            f"--{boundary}\r\nContent-Disposition: form-data; name=\"file\"; "
            f"filename=\"{path.name}\"\r\nContent-Type: application/octet-stream\r\n\r\n"
        ).encode()
        suffix = f"\r\n--{boundary}--\r\n".encode()
        connection.putrequest("POST", "/ota/upload")
        connection.putheader("Content-Type", f"multipart/form-data; boundary={boundary}")
        connection.putheader("Content-Length", str(len(prefix) + len(data) + len(suffix) + path.stat().st_size - cutoff))
        connection.endheaders()
        connection.send(prefix + data)
        connection.close()

    def wait_for(self, predicate, timeout: float = 35.0, interval: float = 1.0) -> dict:
        deadline = time.monotonic() + timeout
        last: dict = {}
        while time.monotonic() < deadline:
            try:
                last = self.status()
                if predicate(last):
                    return last
            except RuntimeError:
                pass
            time.sleep(interval)
        raise AssertionError(f"OTA state timeout; last status={last}")

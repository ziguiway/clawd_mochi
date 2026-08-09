from __future__ import annotations

import urllib.error
import urllib.request

from ota_test.server import ManifestServer, Release


def test_manifest_server_serves_release_and_rejects_unknown_paths(tmp_path):
    firmware = tmp_path / "firmware.bin"
    firmware.write_bytes(b"0123456789")
    server = ManifestServer("127.0.0.1", 0, "127.0.0.1")
    server.release = Release("test", firmware)
    server.start()
    try:
        base = f"http://127.0.0.1:{server.port}"
        assert urllib.request.urlopen(base + "/firmware.bin").read() == b"0123456789"
        try:
            urllib.request.urlopen(base + "/missing")
        except urllib.error.HTTPError as error:
            assert error.code == 404
        else:
            raise AssertionError("unknown OTA path should return 404")
    finally:
        server.close()


def test_manifest_server_exposes_truncated_artifact_for_failure_case(tmp_path):
    firmware = tmp_path / "firmware.bin"
    firmware.write_bytes(b"0123456789")
    server = ManifestServer("127.0.0.1", 0, "127.0.0.1")
    server.release = Release("short", firmware, truncate=True)
    server.start()
    try:
        body = urllib.request.urlopen(f"http://127.0.0.1:{server.port}/firmware.bin").read()
        assert body == b"012"
    finally:
        server.close()

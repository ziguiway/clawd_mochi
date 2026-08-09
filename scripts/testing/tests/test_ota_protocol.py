from __future__ import annotations

import json

from ota_test.server import Release


def test_release_manifest_contains_verified_artifact_metadata(tmp_path):
    firmware = tmp_path / "firmware.bin"
    filesystem = tmp_path / "littlefs.bin"
    firmware.write_bytes(b"firmware")
    filesystem.write_bytes(b"filesystem")
    payload = json.loads(Release("1.2.3", firmware, filesystem).manifest("127.0.0.1", 8000))
    assert payload["schema"] == 1
    assert payload["version"] == "1.2.3"
    assert payload["firmware"]["size"] == firmware.stat().st_size
    assert len(payload["firmware"]["sha256"]) == 64
    assert payload["filesystem"]["url"].endswith("/littlefs.bin")


def test_release_can_model_checksum_and_truncated_downloads(tmp_path):
    firmware = tmp_path / "firmware.bin"
    firmware.write_bytes(b"firmware")
    corrupt = json.loads(Release("bad", firmware, corrupt_hash=True).manifest("host", 1))
    assert corrupt["firmware"]["sha256"] == "0" * 64
    truncated = Release("short", firmware, truncate=True)
    assert truncated.truncate is True

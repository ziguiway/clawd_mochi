from __future__ import annotations

import time
from html.parser import HTMLParser
from pathlib import Path

from .client import OtaClient
from .server import ManifestServer, Release


class _FirmwarePanelParser(HTMLParser):
    _void_elements = {"area", "base", "br", "col", "embed", "hr", "img",
                      "input", "link", "meta", "param", "source", "track",
                      "wbr"}

    def __init__(self) -> None:
        super().__init__()
        self._ancestors: list[tuple[str, str]] = []
        self.found = False
        self.inside_profile = False

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        attributes = dict(attrs)
        classes = (attributes.get("class") or "").split()
        if "ota-panel" in classes:
            self.found = True
            self.inside_profile = any(
                element_id == "profileWrap" for _, element_id in self._ancestors
            )
        if tag not in self._void_elements:
            self._ancestors.append((tag, attributes.get("id") or ""))

    def handle_endtag(self, tag: str) -> None:
        for index in range(len(self._ancestors) - 1, -1, -1):
            if self._ancestors[index][0] == tag:
                del self._ancestors[index:]
                break


def run_firmware_panel_layout(client: OtaClient) -> None:
    parser = _FirmwarePanelParser()
    parser.feed(client.fetch("/").decode("utf-8"))
    assert parser.found, "Firmware panel is missing from the controller"
    assert not parser.inside_profile, "Firmware panel is hidden in profile settings"
    print("PASS  Firmware panel is a visible top-level section")


def check_available(client: OtaClient, timeout: float = 35.0) -> dict:
    deadline = time.monotonic() + timeout
    last: dict = {}
    while time.monotonic() < deadline:
        try:
            last = client.check()
        except RuntimeError as error:
            last = {"error": str(error)}
            time.sleep(2)
            continue
        if last.get("available") is True:
            return last
        if last.get("error") not in {"network busy", "network unavailable"}:
            break
        time.sleep(2)
    raise AssertionError(f"update did not become available: {last}")


def run_upgrade(client: OtaClient, server: ManifestServer, release: Release) -> None:
    server.release = release
    final: dict = {}
    for attempt in range(2):
        check_available(client)
        if attempt == 0:
            print(f"PASS  OTA manifest detects {release.version}")
        try:
            client.install()
        except RuntimeError:
            pass  # Device closes the HTTP connection while rebooting.
        final = client.wait_for(
            lambda state: state.get("version") == release.version or
            state.get("state") == "failed",
            timeout=45,
        )
        if final.get("version") == release.version:
            break
        if attempt == 0:
            print(f"INFO  remote OTA retry after: {final.get('error', 'failed')}")
            time.sleep(3)
    assert final["version"] == release.version, final
    confirmed = client.wait_for(
        lambda state: state.get("version") == release.version and
        state.get("bootPending") is False,
        timeout=45,
    )
    assert confirmed["bootPending"] is False, confirmed
    if release.filesystem:
        page = client.fetch("/")
        assert b"Clawd Mochi" in page, "LittleFS controller did not mount after OTA"
        print("PASS  LittleFS OTA mounts the updated Web controller")
    print(f"PASS  remote OTA upgrade reaches {release.version}")


def run_checksum_failure(client: OtaClient, server: ManifestServer,
                         firmware: Path, current_version: str) -> None:
    release = Release("1.0.2-checksum", firmware, corrupt_hash=True)
    server.release = release
    check_available(client)
    try:
        client.install()
    except RuntimeError:
        pass
    time.sleep(2)
    status = client.wait_for(lambda state: state.get("state") == "failed")
    assert status["version"] == current_version, status
    print("PASS  checksum failure is rejected without switching firmware")


def run_interrupt(client: OtaClient, release: Release) -> None:
    client.interrupt_upload(release.firmware)
    time.sleep(3)
    status = client.wait_for(lambda state: state.get("state") != "uploading")
    assert status["state"] in {"failed", "idle", "up_to_date", "available"}, status
    print("PASS  interrupted local upload leaves device running")


def run_rollback(client: OtaClient, server: ManifestServer, bad: Release, good_version: str) -> None:
    server.release = bad
    check_available(client)
    try:
        client.install()
    except RuntimeError:
        pass
    status = client.wait_for(
        lambda state: state.get("version") == good_version,
        timeout=45,
    )
    assert status["version"] == good_version, status
    print(f"PASS  failed boot rolls back to {good_version}")


def run_offline_upload(client: OtaClient, firmware: Path, expected_version: str) -> None:
    try:
        client.upload(firmware)
    except RuntimeError:
        pass
    status = client.wait_for(
        lambda state: state.get("version") == expected_version and
        state.get("bootPending") is False,
        timeout=45,
    )
    assert status["version"] == expected_version, status
    print("PASS  offline browser upload installs firmware")

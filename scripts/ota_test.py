#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
"""End-to-end OTA test runner; separate from scripts/web_ui_test."""

from __future__ import annotations

import argparse
from pathlib import Path
import time

from ota_test.cases import (
    run_checksum_failure,
    run_firmware_panel_layout,
    run_interrupt,
    run_offline_upload,
    run_rollback,
    run_upgrade,
)
from ota_test.client import OtaClient
from ota_test.server import ManifestServer, Release
from ota_test.build import IsolatedBuilds, flash, restore_production


def main() -> int:
    parser = argparse.ArgumentParser(description="Clawd Mochi OTA E2E test")
    parser.add_argument("--device-url", required=True)
    parser.add_argument("--advertise-host", required=True)
    parser.add_argument("--serial-port", required=True)
    parser.add_argument("--good-version", default="1.0.1-test")
    parser.add_argument("--bad-version", default="1.0.2-test")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()

    source = Path(__file__).resolve().parents[1]
    manifest_url = f"http://{args.advertise_host}:{args.port}/manifest.json"
    builds = IsolatedBuilds(source, manifest_url)
    server: ManifestServer | None = None
    client = OtaClient(args.device_url)
    try:
        baseline = builds.build("baseline", "1.0.0-test", build_filesystem=True)
        good_artifacts = builds.build(
            "upgrade", args.good_version, build_filesystem=True
        )
        bad_artifacts = builds.build("bad", args.bad_version, fail_boot=True)
        flash(baseline.project, args.serial_port)
        flash(baseline.project, args.serial_port, filesystem=True)
        time.sleep(8)

        server = ManifestServer("0.0.0.0", args.port, args.advertise_host)
        server.start()
        initial = client.status()
        print(f"INFO  device starts at {initial.get('version')} ({initial.get('state')})")
        assert initial.get("version") == "1.0.0-test", initial
        run_firmware_panel_layout(client)
        good = Release(
            args.good_version, good_artifacts.firmware,
            good_artifacts.filesystem,
        )
        bad = Release(args.bad_version, bad_artifacts.firmware)
        if initial.get("version") != args.good_version:
            run_upgrade(client, server, good)
        else:
            print(f"INFO  valid upgrade {args.good_version} already installed")
        run_checksum_failure(
            client, server, good_artifacts.firmware, args.good_version
        )
        run_interrupt(client, good)
        run_rollback(client, server, bad, args.good_version)
        run_offline_upload(client, good_artifacts.firmware, args.good_version)
    finally:
        if server:
            server.close()
        try:
            restore_production(source, args.serial_port)
            restored = client.wait_for(
                lambda state: state.get("version") == builds.production_version,
                timeout=30,
            )
            assert restored.get("version") == builds.production_version, restored
            print(
                f"PASS  production firmware {builds.production_version} restored"
            )
        finally:
            builds.close()
            print("PASS  temporary OTA projects and artifacts removed")
    print("PASS  OTA end-to-end suite complete")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

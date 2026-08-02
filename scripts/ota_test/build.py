from __future__ import annotations

import shutil
import subprocess
import tempfile
import re
from dataclasses import dataclass
from pathlib import Path


ENVIRONMENT = "esp32-c3-devkitc-02"
APP_VERSION_PATTERN = re.compile(
    r'^(#define\s+APP_VERSION\s+)"([^"]+)"', re.MULTILINE
)


@dataclass(frozen=True)
class BuildArtifacts:
    project: Path
    firmware: Path
    filesystem: Path


class IsolatedBuilds:
    """Build OTA variants in disposable project copies."""

    def __init__(self, source: Path, manifest_url: str) -> None:
        self.source = source.resolve()
        self.manifest_url = manifest_url
        self.production_version = self._read_app_version(
            self.source / "src/config/app_config.h"
        )
        self._temporary = tempfile.TemporaryDirectory(prefix="clawd-ota-e2e-")
        self.root = Path(self._temporary.name)

    def close(self) -> None:
        self._temporary.cleanup()

    def _copy_project(self, name: str) -> Path:
        target = self.root / name
        target.mkdir()
        for filename in ("platformio.ini", "ota_4mb.csv"):
            shutil.copy2(self.source / filename, target / filename)
        for dirname in ("src", "data", "lib"):
            source_dir = self.source / dirname
            if source_dir.exists():
                shutil.copytree(source_dir, target / dirname)
        return target

    @staticmethod
    def _replace(path: Path, old: str, new: str) -> None:
        content = path.read_text(encoding="utf-8")
        if old not in content:
            raise RuntimeError(f"test patch anchor missing in {path}: {old}")
        path.write_text(content.replace(old, new, 1), encoding="utf-8")

    @staticmethod
    def _read_app_version(path: Path) -> str:
        match = APP_VERSION_PATTERN.search(path.read_text(encoding="utf-8"))
        if not match:
            raise RuntimeError(f"APP_VERSION not found in {path}")
        return match.group(2)

    @staticmethod
    def _set_app_version(path: Path, version: str) -> None:
        content = path.read_text(encoding="utf-8")
        updated, count = APP_VERSION_PATTERN.subn(
            lambda match: f'{match.group(1)}"{version}"', content, count=1
        )
        if count != 1:
            raise RuntimeError(f"APP_VERSION not found in {path}")
        path.write_text(updated, encoding="utf-8")

    def build(self, name: str, version: str, *, fail_boot: bool = False,
              build_filesystem: bool = False) -> BuildArtifacts:
        project = self._copy_project(name)
        self._set_app_version(project / "src/config/app_config.h", version)
        self._replace(
            project / "src/config/cfg_ota.h",
            '#define CFG_OTA_MANIFEST_URL "https://example.com/clawd-mochi/stable/manifest.json"',
            f'#define CFG_OTA_MANIFEST_URL "{self.manifest_url}"',
        )
        if fail_boot:
            self._replace(
                project / "src/states/app_state_machine.cpp",
                "    _ota.init();",
                "    _ota.init();\n    ESP.restart();",
            )
        self._run(["pio", "run"], project)
        if build_filesystem:
            self._run(["pio", "run", "--target", "buildfs"], project)
        output = project / ".pio/build" / ENVIRONMENT
        return BuildArtifacts(
            project=project,
            firmware=output / "firmware.bin",
            filesystem=output / "littlefs.bin",
        )

    @staticmethod
    def _run(command: list[str], project: Path) -> None:
        print(f"INFO  {' '.join(command)} ({project.name})")
        subprocess.run(command, cwd=project, check=True)


def flash(project: Path, serial_port: str, *, filesystem: bool = False) -> None:
    command = ["pio", "run", "--target", "uploadfs" if filesystem else "upload",
               "--upload-port", serial_port]
    subprocess.run(command, cwd=project, check=True)


def restore_production(source: Path, serial_port: str) -> None:
    print("INFO  restoring production firmware and LittleFS")
    flash(source, serial_port)
    flash(source, serial_port, filesystem=True)

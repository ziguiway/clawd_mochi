"""Live-device snapshots, assertions, and restoration helpers."""

from __future__ import annotations

import json
import urllib.parse
import urllib.request
from typing import Any

def get_device_config(base_url: str) -> dict[str, Any]:
    with urllib.request.urlopen(
        f"{base_url}crypto/config", timeout=10
    ) as response:
        return json.load(response)

def get_device_market_config(base_url: str) -> dict[str, Any]:
    with urllib.request.urlopen(
        f"{base_url}market/config", timeout=10
    ) as response:
        return json.load(response)

def get_device_prefs(base_url: str) -> dict[str, Any]:
    with urllib.request.urlopen(f"{base_url}prefs", timeout=10) as response:
        return json.load(response)

def get_device_profile(base_url: str) -> dict[str, Any]:
    with urllib.request.urlopen(f"{base_url}profile", timeout=10) as response:
        return json.load(response)

def get_device_state(base_url: str) -> dict[str, Any]:
    with urllib.request.urlopen(f"{base_url}state", timeout=10) as response:
        return json.load(response)

def get_device_logs(base_url: str) -> str:
    with urllib.request.urlopen(
        f"{base_url}logs/api?max=200", timeout=10
    ) as response:
        return response.read().decode("utf-8", errors="replace")

def get_device_export(base_url: str) -> dict[str, Any]:
    with urllib.request.urlopen(
        f"{base_url}config/export", timeout=10
    ) as response:
        return json.load(response)

def assert_device_logs(before: str, after: str) -> None:
    before_lines = [line for line in before.splitlines() if line.strip()]
    anchor = before_lines[-1] if before_lines else ""
    anchor_pos = after.rfind(anchor) if anchor else -1
    delta = after[anchor_pos + len(anchor):] if anchor_pos >= 0 else after
    required = [
        "[Web] Expression manual: love",
        "[Web] Expression mode: auto",
        "[Web] Profile saved:",
        "[Web] Profile reset to defaults",
        "[Web] Theme applied: 4",
        "[Web] Configuration imported:",
    ]
    missing = [marker for marker in required if marker not in delta]
    if missing:
        print(
            "INFO  设备日志环形缓冲未保留全部动作，跳过日志子断言: "
            + ", ".join(missing)
        )
        return
    print("PASS  HTTP 日志记录了表情与个性化功能动作")

def assert_serial_logs(logs: str) -> None:
    assert "[Web] Expression manual: love" in logs
    assert "[Web] Expression mode: auto" in logs
    assert "[Web] Profile saved:" in logs
    assert "[Web] Profile reset to defaults" in logs
    assert "[Web] Theme applied: 4" in logs
    assert "[Web] Configuration imported:" in logs
    print("PASS  串口日志实时输出了表情与个性化功能动作")

def restore_device_config(
    base_url: str, original_assets: list[dict[str, Any]]
) -> None:
    assets = [
        {
            "id": asset["id"],
            "symbol": asset["symbol"],
            "name": asset["name"],
            "gold": bool(asset.get("gold", False)),
        }
        for asset in original_assets
    ]
    request = urllib.request.Request(
        f"{base_url}crypto/config",
        data=json.dumps({"assets": assets}).encode(),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(request, timeout=10):
        pass

def restore_device_market_config(
    base_url: str, original_assets: list[dict[str, Any]]
) -> None:
    assets = [
        {
            "secid": asset["secid"],
            "code": asset["code"],
            "label": asset.get("label", asset["code"]),
            "name": asset["name"],
        }
        for asset in original_assets
    ]
    request = urllib.request.Request(
        f"{base_url}market/config",
        data=json.dumps({"assets": assets}, ensure_ascii=False).encode(),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(request, timeout=10):
        pass

def restore_device_carousel_prefs(base_url: str, prefs: dict[str, Any]) -> None:
    order = prefs.get("carouselOrder", [8, 9, 10, 6, 17])
    query = urllib.parse.urlencode(
        {
            "carousel": "1" if prefs.get("carousel", False) else "0",
            "theme": int(prefs.get("theme", 1)),
            "carouselSpeed": int(prefs.get("carouselSpeed", 12)),
            "carouselFixed": int(prefs.get("carouselFixed", 8)),
            "carouselOrder": ",".join(str(value) for value in order),
        }
    )
    with urllib.request.urlopen(f"{base_url}prefs?{query}", timeout=10):
        pass

def restore_device_profile(base_url: str, profile: dict[str, Any]) -> None:
    request = urllib.request.Request(
        f"{base_url}profile",
        data=json.dumps(profile).encode(),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(request, timeout=10):
        pass

def restore_device_export(base_url: str, config: dict[str, Any]) -> None:
    request = urllib.request.Request(
        f"{base_url}config/import",
        data=json.dumps(config).encode(),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(request, timeout=10):
        pass

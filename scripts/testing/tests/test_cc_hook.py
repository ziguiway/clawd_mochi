from __future__ import annotations

import importlib.util
from pathlib import Path


def load_hook():
    path = Path(__file__).parents[2] / "hooks" / "cc_hook.py"
    spec = importlib.util.spec_from_file_location("cc_hook_under_test", path)
    module = importlib.util.module_from_spec(spec)
    assert spec and spec.loader
    spec.loader.exec_module(module)
    return module


def test_parse_pong_accepts_modes_and_rejects_other_packets():
    hook = load_hook()
    assert hook.parse_pong(b"CC:pong:LAN") == "lan"
    assert hook.parse_pong(b"CC:pong") == "unknown"
    assert hook.parse_pong(b"garbage") is None


def test_event_resolution_and_tool_summary_are_bounded():
    hook = load_hook()
    assert hook.resolve_event("Notification", {"notification_type": "permission_prompt"}) == "permission"
    assert hook.resolve_event("PreToolUse", {"tool_name": "Bash"}) == "working"
    assert hook.extract_display_tool("PreToolUse", {"tool_name": "Bash", "tool_input": {"command": "echo hi"}}) == "Bash"
    assert len(hook.clean_field("x" * 100)) <= 48


def test_packet_fields_and_input_summaries_remove_protocol_delimiters():
    hook = load_hook()
    assert hook.clean_field("a,b\nsecond") == "a b second"
    assert hook.summarize_tool_input({"file_path": "/tmp/a.txt"}) == "/tmp/a.txt"
    assert hook.summarize_tool_input({"unexpected": "value"}) == '{"unexpected": "value"}'
    assert hook.resolve_event("PreToolUse", {"tool_name": "AskUserQuestion"}) == "permission"
    assert hook.resolve_event("Notification", {"notification_type": "unmapped"}) == "working"


def test_device_cache_upsert_and_drop(tmp_path, monkeypatch):
    hook = load_hook()
    monkeypatch.setenv("CLAWD_MOCHI_CACHE_DIR", str(tmp_path))
    devices = hook.upsert_device("192.168.1.20", "lan")
    assert devices[0]["ip"] == "192.168.1.20"
    assert devices[0]["mode"] == "lan"
    assert isinstance(devices[0]["updated_at"], float)
    assert hook.upsert_device("192.168.1.20", "serial")[0]["mode"] == "serial"
    hook.drop_device("192.168.1.20")
    assert hook.load_devices() == []


def test_cache_ignores_malformed_and_expired_entries(tmp_path, monkeypatch):
    hook = load_hook()
    monkeypatch.setenv("CLAWD_MOCHI_CACHE_DIR", str(tmp_path))
    now = 10_000.0
    monkeypatch.setattr(hook.time, "time", lambda: now)
    hook.save_cache({"devices": [
        {"ip": "192.168.1.2", "mode": "lan", "updated_at": now - hook.CACHE_TTL - 1},
        {"ip": "192.168.1.3", "mode": "lan", "updated_at": "bad"},
        {"ip": "192.168.1.4", "mode": "lan", "updated_at": now},
        "invalid",
    ]})
    assert hook.load_devices() == [{"ip": "192.168.1.4", "mode": "lan", "updated_at": now}]

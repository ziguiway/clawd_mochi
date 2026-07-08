#!/usr/bin/env python3
"""Install the Clawd Mochi Claude Code hook on Windows, macOS, and Linux."""

# /// script
# requires-python = ">=3.10"
# ///

from __future__ import annotations

import argparse
import json
import os
import shutil
import stat
import sys
import time
from pathlib import Path


HOOK_EVENTS = [
    "SessionStart",
    "Setup",
    "SessionEnd",
    "UserPromptSubmit",
    "PermissionRequest",
    "PreToolUse",
    "PostToolUse",
    "PostToolUseFailure",
    "PostToolBatch",
    "SubagentStart",
    "SubagentStop",
    "TaskCreated",
    "TaskCompleted",
    "PreCompact",
    "PostCompact",
    "WorktreeCreate",
    "WorktreeRemove",
    "Stop",
    "StopFailure",
]

HOOK_STATUS_MESSAGE = "Clawd Mochi status hook"
HOOK_TIMEOUT_SECONDS = 10
TOOL_MATCHER_EVENTS = {
    "PermissionRequest",
    "PreToolUse",
    "PostToolUse",
    "PostToolUseFailure",
}


def user_data_dir() -> Path:
    override = os.environ.get("CLAWD_MOCHI_DATA_DIR")
    if override:
        return Path(override).expanduser()

    if os.name == "nt":
        base = os.environ.get("LOCALAPPDATA") or os.environ.get("APPDATA")
        if base:
            return Path(base) / "ClawdMochi"
        return Path.home() / "AppData" / "Local" / "ClawdMochi"

    if sys.platform == "darwin":
        return Path.home() / "Library" / "Application Support" / "ClawdMochi"

    base = os.environ.get("XDG_DATA_HOME")
    if base:
        return Path(base) / "clawd-mochi"
    return Path.home() / ".local" / "share" / "clawd-mochi"


def user_cache_dir() -> Path:
    override = os.environ.get("CLAWD_MOCHI_CACHE_DIR")
    if override:
        return Path(override).expanduser()

    if os.name == "nt":
        base = os.environ.get("LOCALAPPDATA") or os.environ.get("APPDATA")
        if base:
            return Path(base) / "ClawdMochi"
        return Path.home() / "AppData" / "Local" / "ClawdMochi"

    if sys.platform == "darwin":
        return Path.home() / "Library" / "Caches" / "ClawdMochi"

    base = os.environ.get("XDG_CACHE_HOME")
    if base:
        return Path(base) / "clawd-mochi"
    return Path.home() / ".cache" / "clawd-mochi"


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def default_hook_source() -> Path:
    return Path(__file__).resolve().with_name("cc_hook.py")


def default_settings_path(scope: str, project_dir: Path) -> Path:
    if scope == "user":
        return Path.home() / ".claude" / "settings.json"
    if scope == "project":
        return project_dir / ".claude" / "settings.json"
    return project_dir / ".claude" / "settings.local.json"


def load_settings(path: Path) -> dict:
    if not path.exists():
        return {}

    try:
        with path.open("r", encoding="utf-8") as f:
            data = json.load(f)
    except json.JSONDecodeError as exc:
        raise SystemExit(f"Invalid JSON in {path}: {exc}") from exc

    if not isinstance(data, dict):
        raise SystemExit(f"{path} must contain a JSON object")
    return data


def save_settings(path: Path, data: dict, dry_run: bool, backup: bool) -> None:
    rendered = json.dumps(data, ensure_ascii=False, indent=2, sort_keys=False) + "\n"
    if dry_run:
        print(rendered)
        return

    path.parent.mkdir(parents=True, exist_ok=True)
    if backup and path.exists():
        stamp = time.strftime("%Y%m%d-%H%M%S")
        shutil.copy2(path, path.with_name(f"{path.name}.bak-{stamp}"))

    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(rendered, encoding="utf-8")
    os.replace(tmp, path)


def install_hook_script(source: Path, no_copy: bool, dry_run: bool) -> Path:
    source = source.expanduser().resolve()
    if not source.exists():
        raise SystemExit(f"Hook script not found: {source}")

    if no_copy:
        return source

    destination = user_data_dir() / "hooks" / "cc_hook.py"
    if not dry_run:
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
        if os.name != "nt":
            mode = destination.stat().st_mode
            destination.chmod(mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
    return destination


def resolve_runner(runner: str, python_path: Path | str, uv_path: Path | str | None) -> tuple[str, list[str]]:
    if runner == "auto":
        found_uv = shutil.which(str(uv_path or "uv"))
        if found_uv:
            return found_uv, ["run", "--script"]
        return str(python_path), []

    if runner == "uv":
        selected_uv = str(uv_path or shutil.which("uv") or "uv")
        return selected_uv, ["run", "--script"]

    return str(python_path), []


def hook_handler(command: str, runner_args: list[str], hook_script: Path, event: str) -> dict:
    return {
        "type": "command",
        "command": command,
        "args": [*runner_args, str(hook_script), event],
        "async": True,
        "timeout": HOOK_TIMEOUT_SECONDS,
        "statusMessage": HOOK_STATUS_MESSAGE,
    }


def hook_group(command: str, runner_args: list[str], hook_script: Path,
               event: str, matcher: str | None = None) -> dict:
    group: dict = {"hooks": [hook_handler(command, runner_args, hook_script, event)]}
    if matcher:
        group["matcher"] = matcher
    return group


def is_clawd_handler(handler: object, hook_script: Path | None = None) -> bool:
    if not isinstance(handler, dict):
        return False
    if handler.get("statusMessage") == HOOK_STATUS_MESSAGE:
        return True

    args = handler.get("args")
    if not isinstance(args, list) or not args:
        return False

    arg_values = [str(arg) for arg in args]
    if hook_script and str(hook_script) in arg_values:
        return True

    for value in arg_values:
        normalized = value.replace("\\", "/").lower()
        if normalized.endswith("/clawdmochi/hooks/cc_hook.py") or normalized.endswith("/clawd-mochi/hooks/cc_hook.py"):
            return True
    return False


def prune_clawd_hooks(settings: dict, hook_script: Path | None = None) -> int:
    hooks_root = settings.get("hooks")
    if not isinstance(hooks_root, dict):
        return 0

    removed = 0
    empty_events: list[str] = []
    for event, groups in hooks_root.items():
        if not isinstance(groups, list):
            continue

        kept_groups = []
        for group in groups:
            if not isinstance(group, dict):
                kept_groups.append(group)
                continue

            handlers = group.get("hooks")
            if not isinstance(handlers, list):
                kept_groups.append(group)
                continue

            kept_handlers = []
            for handler in handlers:
                if is_clawd_handler(handler, hook_script):
                    removed += 1
                else:
                    kept_handlers.append(handler)

            if kept_handlers:
                group = dict(group)
                group["hooks"] = kept_handlers
                kept_groups.append(group)

        if kept_groups:
            hooks_root[event] = kept_groups
        else:
            empty_events.append(event)

    for event in empty_events:
        hooks_root.pop(event, None)
    if not hooks_root:
        settings.pop("hooks", None)
    return removed


def install_settings(settings: dict, command: str, runner_args: list[str],
                     hook_script: Path, events: list[str],
                     tool_matchers: list[str] | None = None) -> int:
    prune_clawd_hooks(settings, hook_script)

    hooks_root = settings.setdefault("hooks", {})
    if not isinstance(hooks_root, dict):
        raise SystemExit('settings["hooks"] must be a JSON object')

    installed = 0
    for event in events:
        hooks_root.setdefault(event, [])
        if not isinstance(hooks_root[event], list):
            raise SystemExit(f'settings["hooks"]["{event}"] must be a JSON array')
        matchers = tool_matchers if event in TOOL_MATCHER_EVENTS else [None]
        for matcher in matchers:
            hooks_root[event].append(hook_group(command, runner_args, hook_script, event, matcher))
            installed += 1
    return installed


def parse_events(raw: str | None) -> list[str]:
    if not raw:
        return HOOK_EVENTS

    events = [part.strip() for part in raw.split(",") if part.strip()]
    unknown = sorted(set(events) - set(HOOK_EVENTS))
    if unknown:
        raise SystemExit(f"Unknown hook event(s): {', '.join(unknown)}")
    return events


def normalize_tool_matcher(raw: str) -> str:
    # Claude Code treats "Edit|Write" as an exact-name list. Accept commas as
    # installer input sugar, but always write the explicit pipe form.
    parts = [part.strip() for part in raw.replace("|", ",").split(",") if part.strip()]
    if not parts:
        raise SystemExit("Tool matcher cannot be empty")

    invalid = [
        part for part in parts
        if any(not (char.isalnum() or char in "_- ") for char in part)
    ]
    if invalid:
        raise SystemExit(
            "Tool matchers must be exact tool names here, e.g. Bash or Edit|Write. "
            f"Unsupported matcher(s): {', '.join(invalid)}"
        )
    return "|".join(dict.fromkeys(parts))


def parse_tool_matchers(raw: str | None) -> list[str] | None:
    if not raw:
        return None
    return [normalize_tool_matcher(part) for part in raw.split(";") if part.strip()]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", nargs="?", choices=("install", "uninstall"), default="install")
    parser.add_argument("--scope", choices=("user", "project", "local"), default="user",
                        help="Settings file to update. Defaults to user-wide ~/.claude/settings.json.")
    parser.add_argument("--project-dir", type=Path, default=repo_root(),
                        help="Project root used with --scope project/local.")
    parser.add_argument("--settings-file", type=Path,
                        help="Explicit settings file path. Overrides --scope.")
    parser.add_argument("--hook-script", type=Path, default=default_hook_source(),
                        help="Source cc_hook.py to install.")
    parser.add_argument("--python", default=sys.executable,
                        help="Python executable Claude Code should run.")
    parser.add_argument("--runner", choices=("auto", "uv", "python"), default="auto",
                        help="Runtime for cc_hook.py. Defaults to auto: uv when available, otherwise Python.")
    parser.add_argument("--uv",
                        help="uv executable path or name. Used with --runner auto/uv.")
    parser.add_argument("--no-copy", action="store_true",
                        help="Reference --hook-script in place instead of copying it to the user data directory.")
    parser.add_argument("--remove-script", action="store_true",
                        help="On uninstall, delete the copied hook script if it is in the Clawd Mochi data dir.")
    parser.add_argument("--events",
                        help="Comma-separated subset of events to install. Defaults to all supported events.")
    parser.add_argument("--tool-matchers",
                        help=("Semicolon-separated exact tool matcher groups for tool hooks. "
                              "Examples: Bash or Bash;Edit,Write. Commas are written as Edit|Write."))
    parser.add_argument("--no-backup", action="store_true",
                        help="Do not create a timestamped backup of an existing settings file.")
    parser.add_argument("--dry-run", action="store_true",
                        help="Print the resulting settings JSON without writing files.")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    project_dir = args.project_dir.expanduser().resolve()
    settings_path = (args.settings_file.expanduser().resolve()
                     if args.settings_file
                     else default_settings_path(args.scope, project_dir))

    if args.action == "install":
        hook_script = install_hook_script(args.hook_script, args.no_copy, args.dry_run)
    elif args.no_copy:
        hook_script = args.hook_script.expanduser().resolve()
    else:
        hook_script = user_data_dir() / "hooks" / "cc_hook.py"

    settings = load_settings(settings_path)

    if args.action == "install":
        events = parse_events(args.events)
        tool_matchers = parse_tool_matchers(args.tool_matchers)
        command, runner_args = resolve_runner(args.runner, args.python, args.uv)
        installed = install_settings(settings, command, runner_args, hook_script, events, tool_matchers)
        save_settings(settings_path, settings, args.dry_run, not args.no_backup)
        print(f"Installed {installed} Clawd Mochi hook entries")
        if tool_matchers:
            print(f"Tool matchers: {'; '.join(tool_matchers)}")
        print(f"Runner: {command} {' '.join(runner_args)}".rstrip())
    else:
        removed = prune_clawd_hooks(settings, hook_script)
        save_settings(settings_path, settings, args.dry_run, not args.no_backup)
        if args.remove_script and not args.dry_run and not args.no_copy:
            data_root = user_data_dir().resolve()
            script_path = hook_script.resolve()
            if data_root in script_path.parents and script_path.exists():
                script_path.unlink()
        print(f"Removed {removed} Clawd Mochi hook entries")

    print(f"Settings: {settings_path}")
    print(f"Hook script: {hook_script}")
    print(f"Cache file: {user_cache_dir() / 'cc_hook_cache.json'}")


if __name__ == "__main__":
    main()

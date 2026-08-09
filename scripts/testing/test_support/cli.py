from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Clawd Mochi automation test suite")
    subparsers = parser.add_subparsers(dest="suite", required=True)

    unit = subparsers.add_parser("unit", help="快速离线回归测试")
    unit.add_argument("pytest_args", nargs=argparse.REMAINDER)

    web = subparsers.add_parser("web", help="浏览器 Web UI 回归测试")
    web.add_argument("runner_args", nargs=argparse.REMAINDER)

    ota = subparsers.add_parser("ota", help="真实设备 OTA 端到端测试")
    ota.add_argument("runner_args", nargs=argparse.REMAINDER)

    all_suites = subparsers.add_parser("all", help="依次运行离线、Web 和 OTA 套件")
    all_suites.add_argument("runner_args", nargs=argparse.REMAINDER)
    return parser


def run(command: list[str]) -> int:
    return subprocess.run(command, cwd=ROOT, check=False).returncode


def main(argv: list[str] | None = None) -> int:
    raw = list(sys.argv[1:] if argv is None else argv)
    if not raw or raw[0] in {"-h", "--help"}:
        build_parser().print_help()
        return 0 if raw else 2
    if raw[0] not in {"unit", "web", "ota", "all"}:
        build_parser().error(f"unknown suite: {raw[0]}")
    args = argparse.Namespace(
        suite=raw[0], pytest_args=raw[1:], runner_args=raw[1:]
    )
    if args.suite == "unit":
        return run([sys.executable, "-m", "pytest", "scripts/testing/tests", *args.pytest_args])
    if args.suite == "web":
        return run(["uv", "run", "scripts/testing/test_web_ui.py", *args.runner_args])
    if args.suite == "ota":
        return run([sys.executable, "scripts/testing/ota_test.py", *args.runner_args])

    result = run([sys.executable, "-m", "pytest", "scripts/testing/tests"])
    if result:
        return result
    result = run(["uv", "run", "scripts/testing/test_web_ui.py", *args.runner_args])
    if result:
        return result
    print("Offline and Web UI suites passed; OTA requires explicit device arguments.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = [
#   "playwright>=1.45",
# ]
# ///
"""Clawd Mochi Web 控制台正向 UI 回归测试。

运行:
    uv run scripts/test_web_ui.py

测试范围只覆盖 Crypto 配置的主流程：
打开页面 -> 联想搜索 -> 添加币种 -> 自动保存到设备。
"""

from __future__ import annotations

import argparse
import json
import threading
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any

from playwright.sync_api import Page, Route, sync_playwright


ROOT = Path(__file__).resolve().parents[1]
WEB_SOURCE = ROOT / "src" / "service" / "web_service.cpp"
EDGE_PATH = Path(
    "/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge"
)

COINLORE_ASSETS = [
    {"id": "90", "symbol": "BTC", "name": "Bitcoin", "rank": 1},
    {"id": "80", "symbol": "ETH", "name": "Ethereum", "rank": 2},
    {"id": "48543", "symbol": "SOL", "name": "Solana", "rank": 5},
    {"id": "58", "symbol": "XRP", "name": "XRP", "rank": 6},
    {"id": "2", "symbol": "DOGE", "name": "Dogecoin", "rank": 8},
    {"id": "33536", "symbol": "OKB", "name": "OKB", "rank": 31},
    {"id": "42855", "symbol": "XAUT", "name": "Tether Gold", "rank": 37},
    {"id": "70001", "symbol": "SPK", "name": "Spark", "rank": 145},
]

INITIAL_ASSETS = [
    {
        "id": "90",
        "symbol": "BTC",
        "name": "Bitcoin",
        "gold": False,
        "price": 118400.0,
        "change": 2.6,
    },
    {
        "id": "80",
        "symbol": "ETH",
        "name": "Ethereum",
        "gold": False,
        "price": 3860.0,
        "change": -0.8,
    },
    {
        "id": "42855",
        "symbol": "XAUT",
        "name": "Tether Gold",
        "gold": False,
        "price": 4073.5,
        "change": 0.8,
    },
]


def extract_index_html() -> str:
    source = WEB_SOURCE.read_text(encoding="utf-8")
    start_marker = 'R"rawhtml('
    end_marker = ')rawhtml";'
    start = source.index(start_marker) + len(start_marker)
    end = source.index(end_marker, start)
    return source[start:end]


class FirmwareStubHandler(BaseHTTPRequestHandler):
    html = extract_index_html()
    assets: list[dict[str, Any]] = [dict(item) for item in INITIAL_ASSETS]
    config_post_count = 0

    def log_message(self, _format: str, *_args: object) -> None:
        return

    def send_json(self, payload: Any, status: int = 200) -> None:
        body = json.dumps(payload).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:
        path = self.path.split("?", 1)[0]
        if path == "/":
            body = self.html.encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        elif path == "/crypto/config":
            self.send_json(
                {
                    "loading": False,
                    "updatedAgeSec": 5,
                    "assets": self.assets,
                }
            )
        elif path == "/prefs":
            self.send_json(
                {"bg": "#aa4818", "speed": 1, "claudeStatus": True}
            )
        elif path == "/state":
            self.send_json({"busy": False, "brightness": 100})
        elif path == "/wifi/status":
            self.send_json(
                {
                    "connected": True,
                    "ssid": "UI-TEST",
                    "lanIp": "127.0.0.1",
                }
            )
        elif path == "/timer/status":
            self.send_json(
                {
                    "phase": "focus",
                    "running": False,
                    "paused": False,
                    "remaining": 1500,
                    "focus": 25,
                    "break": 5,
                }
            )
        else:
            self.send_json({"ok": True})

    def do_POST(self) -> None:
        path = self.path.split("?", 1)[0]
        if path != "/crypto/config":
            self.send_json({"ok": True})
            return

        length = int(self.headers.get("Content-Length", "0"))
        payload = json.loads(self.rfile.read(length) or b"{}")
        self.__class__.assets = payload.get("assets", [])
        self.__class__.config_post_count += 1
        self.send_json(
            {
                "loading": False,
                "updatedAgeSec": 0,
                "assets": self.assets,
            }
        )


def mock_coinlore(route: Route) -> None:
    route.fulfill(
        status=200,
        content_type="application/json",
        body=json.dumps({"data": COINLORE_ASSETS}),
        headers={"Access-Control-Allow-Origin": "*"},
    )


def result_symbols(page: Page) -> list[str]:
    return page.locator("#mResults .mresname strong").all_text_contents()


def assert_suggestion(page: Page, query: str, expected: set[str]) -> None:
    search = page.locator("#mSearch")
    search.fill(query)
    page.locator("#mResults .mresult").first.wait_for(state="visible")
    actual = set(result_symbols(page))
    missing = expected - actual
    assert not missing, (
        f"搜索 {query!r} 缺少联想结果 {sorted(missing)}，"
        f"实际结果为 {sorted(actual)}"
    )
    print(f"PASS  联想搜索 {query!r}: {', '.join(sorted(expected))}")


def run_positive_flow(page: Page, *, stub_mode: bool) -> None:
    page.goto("/", wait_until="domcontentloaded")

    crypto_button = page.locator('button[data-v="9"]')
    crypto_button.click()
    page.locator("#mwrap.open").wait_for(state="visible")
    page.locator("#mSelected .mselrow").first.wait_for(state="visible")
    print("PASS  打开 Crypto 配置")

    assert_suggestion(page, "S", {"SOL"})
    assert_suggestion(page, "SOL", {"SOL"})
    assert_suggestion(page, "SPK", {"SPK"})
    assert_suggestion(page, "OKB", {"OKB"})
    assert_suggestion(page, "OKB SOL SPK", {"OKB", "SOL", "SPK"})

    page.locator("#mSearch").fill("OKB")
    symbols = result_symbols(page)
    assert symbols.count("OKB") == 1, "OKB 搜索结果不唯一"
    okb_index = symbols.index("OKB")
    okb_row = page.locator("#mResults .mresult").nth(okb_index)
    okb_row.get_by_role("button", name="ADD").click()

    page.locator("#mSelected .mselrow").filter(has_text="OKB").wait_for(
        state="visible"
    )
    page.get_by_text("SAVED TO DEVICE", exact=False).wait_for(state="visible")
    if stub_mode:
        assert FirmwareStubHandler.config_post_count == 1, (
            "添加 OKB 后没有且仅有一次自动保存"
        )
    print("PASS  添加 OKB 并自动保存")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="运行 Clawd Mochi Crypto Web UI 正向自动化测试"
    )
    parser.add_argument(
        "--live-directory",
        action="store_true",
        help="使用真实 CoinLore 资产目录；默认使用稳定的本地 Mock",
    )
    parser.add_argument(
        "--device-url",
        help="直接测试实际设备，例如 http://clawd-mochi.local/",
    )
    return parser.parse_args()


def get_device_config(base_url: str) -> dict[str, Any]:
    with urllib.request.urlopen(
        f"{base_url}crypto/config", timeout=10
    ) as response:
        return json.load(response)


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


def main() -> int:
    args = parse_args()
    if not EDGE_PATH.exists():
        raise SystemExit(
            "未找到 Microsoft Edge。请安装 Edge，或修改脚本中的 EDGE_PATH。"
        )

    server: ThreadingHTTPServer | None = None
    original_device_assets: list[dict[str, Any]] | None = None
    if args.device_url:
        base_url = args.device_url.rstrip("/") + "/"
        original_device_assets = get_device_config(base_url)["assets"]
    else:
        FirmwareStubHandler.assets = [dict(item) for item in INITIAL_ASSETS]
        FirmwareStubHandler.config_post_count = 0
        server = ThreadingHTTPServer(("127.0.0.1", 0), FirmwareStubHandler)
        server_thread = threading.Thread(
            target=server.serve_forever, daemon=True
        )
        server_thread.start()
        base_url = f"http://127.0.0.1:{server.server_port}/"

    try:
        with sync_playwright() as playwright:
            browser = playwright.chromium.launch(
                executable_path=str(EDGE_PATH),
                headless=True,
            )
            context = browser.new_context(
                base_url=base_url,
                viewport={"width": 430, "height": 932},
            )
            page = context.new_page()
            page.on(
                "requestfailed",
                lambda request: print(
                    f"NET   {request.method} {request.url}: "
                    f"{request.failure}"
                ),
            )
            page.on(
                "console",
                lambda message: print(
                    f"CONSOLE {message.type}: {message.text}"
                )
                if message.type == "error"
                else None,
            )
            if args.device_url:
                print(f"INFO  直接测试设备 {base_url}")
            elif not args.live_directory:
                page.route(
                    "https://api.coinlore.net/api/assets/",
                    mock_coinlore,
                )
                print("INFO  使用本地 CoinLore Mock 目录")
            else:
                print("INFO  使用真实 CoinLore 资产目录")
            try:
                run_positive_flow(page, stub_mode=not bool(args.device_url))
            except Exception:
                screenshot = Path("/tmp/clawd_mochi_ui_failure.png")
                page.screenshot(path=str(screenshot), full_page=True)
                print(f"FAIL  截图已保存到 {screenshot}")
                raise
            finally:
                context.close()
                browser.close()
    finally:
        if original_device_assets is not None:
            restore_device_config(base_url, original_device_assets)
            print("INFO  已恢复设备原有币种配置")
        if server is not None:
            server.shutdown()
            server.server_close()

    print("PASS  Crypto Web UI 正向流程全部通过")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

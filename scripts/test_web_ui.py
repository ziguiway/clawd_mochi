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

测试范围只覆盖 Crypto / Market 配置的正向主流程：
打开页面 -> 联想搜索 -> 添加行情项目 -> 自动保存到设备。
"""

from __future__ import annotations

import argparse
import json
import threading
import urllib.request
import urllib.parse
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

INITIAL_MARKET_ASSETS = [
    {
        "secid": "1.000001",
        "code": "000001",
        "label": "SSE",
        "name": "上证指数",
        "price": 3858.25,
        "change": 1.15,
    },
    {
        "secid": "0.399001",
        "code": "399001",
        "label": "SZSE",
        "name": "深证成指",
        "price": 14148.73,
        "change": 2.72,
    },
    {
        "secid": "0.399006",
        "code": "399006",
        "label": "CYB",
        "name": "创业板指",
        "price": 3590.79,
        "change": 3.16,
    },
]

MARKET_SEARCH_RESULTS = [
    {
        "secid": "1.600519",
        "code": "600519",
        "label": "600519",
        "name": "贵州茅台",
    },
    {
        "secid": "0.000858",
        "code": "000858",
        "label": "000858",
        "name": "五粮液",
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
    market_assets: list[dict[str, Any]] = [
        dict(item) for item in INITIAL_MARKET_ASSETS
    ]
    config_post_count = 0
    market_post_count = 0
    prefs: dict[str, Any] = {
        "bg": "#aa4818",
        "speed": 1,
        "claudeStatus": True,
        "carousel": False,
        "carouselSpeed": 12,
        "carouselOrder": [8, 9, 10],
        "carouselFixed": 8,
    }
    prefs_update_count = 0

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
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path
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
        elif path == "/market/config":
            self.send_json(
                {
                    "loading": False,
                    "updatedAgeSec": 5,
                    "assets": self.market_assets,
                }
            )
        elif path == "/market/search":
            self.send_json({"results": MARKET_SEARCH_RESULTS})
        elif path == "/prefs":
            args = urllib.parse.parse_qs(parsed.query)
            if args:
                if "carousel" in args:
                    self.__class__.prefs["carousel"] = args["carousel"][0] in {
                        "1",
                        "true",
                    }
                if "carouselSpeed" in args:
                    self.__class__.prefs["carouselSpeed"] = int(
                        args["carouselSpeed"][0]
                    )
                if "carouselFixed" in args:
                    self.__class__.prefs["carouselFixed"] = int(
                        args["carouselFixed"][0]
                    )
                if "carouselOrder" in args:
                    self.__class__.prefs["carouselOrder"] = [
                        int(value)
                        for value in args["carouselOrder"][0].split(",")
                    ]
                self.__class__.prefs_update_count += 1
            self.send_json(self.prefs)
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
        if path not in {"/crypto/config", "/market/config"}:
            self.send_json({"ok": True})
            return

        length = int(self.headers.get("Content-Length", "0"))
        payload = json.loads(self.rfile.read(length) or b"{}")
        if path == "/crypto/config":
            self.__class__.assets = payload.get("assets", [])
            self.__class__.config_post_count += 1
            assets = self.assets
        else:
            self.__class__.market_assets = payload.get("assets", [])
            self.__class__.market_post_count += 1
            assets = self.market_assets
        self.send_json(
            {
                "loading": False,
                "updatedAgeSec": 0,
                "assets": assets,
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
    assert not page.locator("#mwrap").is_visible(), (
        "未选择 Crypto 时不应显示加密货币设置"
    )

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
    page.locator("#mAuto").get_by_text(
        "SAVED TO DEVICE", exact=False
    ).wait_for(state="visible")
    if stub_mode:
        assert FirmwareStubHandler.config_post_count == 1, (
            "添加 OKB 后没有且仅有一次自动保存"
        )
    print("PASS  添加 OKB 并自动保存")


def run_carousel_flow(page: Page, *, stub_mode: bool) -> None:
    page.goto("/", wait_until="domcontentloaded")
    panel_button = page.locator("#carouselPanelBtn")
    panel_button.click()
    page.locator("#carouselWrap.open").wait_for(state="visible")
    assert page.locator("#carouselPanelState").text_content() == "close"
    print("PASS  展开信息轮播设置")
    toggle = page.locator("#carouselToggle")
    toggle.click()
    page.get_by_text("carousel on", exact=False).wait_for(state="visible")
    assert page.locator("#carouselFixed").is_disabled()
    print("PASS  开启信息轮播")

    speed = page.locator("#carouselSpeed")
    assert not speed.is_disabled(), "开启轮播后速度滑块仍被禁用"
    box = speed.bounding_box()
    assert box is not None, "未找到轮播速度滑块"
    speed.click(
        position={"x": box["width"] * (18 - 5) / (60 - 5), "y": box["height"] / 2}
    )
    page.wait_for_timeout(400)
    actual_speed = page.locator("#carouselSpeedV").text_content()
    selected_speed = int(speed.input_value())
    assert selected_speed != 12 and actual_speed == f"{selected_speed}s", (
        f"轮播滑块未正确更新，显示为 {actual_speed!r}，"
        f"输入值为 {selected_speed!r}"
    )
    print(f"PASS  设置轮播间隔为 {selected_speed} 秒")

    weather_handle = page.get_by_role("button", name="Drag Weather")
    weather_box = weather_handle.bounding_box()
    crypto_row = page.locator("#carouselOrder .ritem").nth(1).bounding_box()
    assert weather_box is not None and crypto_row is not None, "未找到轮播拖拽项目"
    weather_handle.hover()
    page.mouse.move(weather_box["x"] + weather_box["width"] / 2, weather_box["y"] + weather_box["height"] / 2)
    page.mouse.down()
    page.mouse.move(weather_box["x"] + weather_box["width"] / 2, crypto_row["y"] + crypto_row["height"] * 0.8, steps=8)
    page.mouse.up()
    page.wait_for_timeout(150)
    assert page.locator("#carouselOrder .rname").first.text_content() == "Crypto"
    print("PASS  拖拽调整轮播顺序为 Crypto 优先")

    toggle.click()
    page.get_by_text("carousel off", exact=False).wait_for(state="visible")
    fixed = page.locator("#carouselFixed")
    assert not fixed.is_disabled()
    fixed.select_option("10")
    page.wait_for_timeout(150)
    if stub_mode:
        assert FirmwareStubHandler.prefs_update_count >= 4, (
            "轮播设置没有完整保存到设备"
        )
        assert FirmwareStubHandler.prefs["carousel"] is False
        assert FirmwareStubHandler.prefs["carouselFixed"] == 10
    print("PASS  关闭轮播并固定显示 Market")


def assert_stock_suggestion(
    page: Page, query: str, expected_code: str, expected_name: str
) -> None:
    search = page.locator("#sSearch")
    search.fill(query)
    result = page.locator("#sResults .mresult").filter(
        has_text=expected_code
    ).filter(has_text=expected_name)
    result.wait_for(state="visible")
    print(f"PASS  股票搜索 {query!r}: {expected_code} {expected_name}")


def run_market_flow(page: Page, *, stub_mode: bool) -> None:
    market_button = page.locator('button[data-v="10"]')
    market_button.click()
    page.locator("#swrap.open").wait_for(state="visible")
    page.locator("#sSelected .mselrow").first.wait_for(state="visible")
    assert page.locator("#sSelected .mselrow").count() == 3
    print("PASS  打开 Market 并显示三大指数")

    assert_stock_suggestion(page, "600519", "600519", "贵州茅台")
    assert_stock_suggestion(page, "茅台", "600519", "贵州茅台")

    page.locator("#sSearch").fill("600519")
    result = page.locator("#sResults .mresult").filter(has_text="600519")
    result.get_by_role("button", name="ADD").click()
    page.locator("#sSelected .mselrow").filter(
        has_text="600519"
    ).wait_for(state="visible")
    page.locator("#sAuto").get_by_text(
        "SAVED TO DEVICE", exact=False
    ).wait_for(state="visible")
    if stub_mode:
        assert FirmwareStubHandler.market_post_count == 1, (
            "添加贵州茅台后没有且仅有一次自动保存"
        )
    print("PASS  添加贵州茅台并自动保存")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="运行 Clawd Mochi Crypto / Market Web UI 正向自动化测试"
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


def get_device_market_config(base_url: str) -> dict[str, Any]:
    with urllib.request.urlopen(
        f"{base_url}market/config", timeout=10
    ) as response:
        return json.load(response)


def get_device_prefs(base_url: str) -> dict[str, Any]:
    with urllib.request.urlopen(f"{base_url}prefs", timeout=10) as response:
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
    order = prefs.get("carouselOrder", [8, 9, 10])
    query = urllib.parse.urlencode(
        {
            "carousel": "1" if prefs.get("carousel", False) else "0",
            "carouselSpeed": int(prefs.get("carouselSpeed", 12)),
            "carouselFixed": int(prefs.get("carouselFixed", 8)),
            "carouselOrder": ",".join(str(value) for value in order),
        }
    )
    with urllib.request.urlopen(f"{base_url}prefs?{query}", timeout=10):
        pass


def main() -> int:
    args = parse_args()
    if not EDGE_PATH.exists():
        raise SystemExit(
            "未找到 Microsoft Edge。请安装 Edge，或修改脚本中的 EDGE_PATH。"
        )

    server: ThreadingHTTPServer | None = None
    original_device_assets: list[dict[str, Any]] | None = None
    original_device_market_assets: list[dict[str, Any]] | None = None
    original_device_prefs: dict[str, Any] | None = None
    if args.device_url:
        base_url = args.device_url.rstrip("/") + "/"
        original_device_assets = get_device_config(base_url)["assets"]
        original_device_market_assets = get_device_market_config(base_url)[
            "assets"
        ]
        original_device_prefs = get_device_prefs(base_url)
    else:
        FirmwareStubHandler.assets = [dict(item) for item in INITIAL_ASSETS]
        FirmwareStubHandler.market_assets = [
            dict(item) for item in INITIAL_MARKET_ASSETS
        ]
        FirmwareStubHandler.config_post_count = 0
        FirmwareStubHandler.market_post_count = 0
        FirmwareStubHandler.prefs = {
            "bg": "#aa4818",
            "speed": 1,
            "claudeStatus": True,
            "carousel": False,
            "carouselSpeed": 12,
            "carouselOrder": [8, 9, 10],
            "carouselFixed": 8,
        }
        FirmwareStubHandler.prefs_update_count = 0
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
                run_carousel_flow(page, stub_mode=not bool(args.device_url))
                run_market_flow(page, stub_mode=not bool(args.device_url))
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
        if original_device_market_assets is not None:
            restore_device_market_config(
                base_url, original_device_market_assets
            )
            print("INFO  已恢复设备原有股票配置")
        if original_device_prefs is not None:
            restore_device_carousel_prefs(base_url, original_device_prefs)
            print("INFO  已恢复设备原有轮播设置")
        if server is not None:
            server.shutdown()
            server.server_close()

    print("PASS  Crypto / Market Web UI 正向流程全部通过")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

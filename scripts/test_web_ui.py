#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = [
#   "playwright>=1.45",
#   "pyserial>=3.5",
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
import time
import urllib.request
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any

from playwright.sync_api import Page, Route, sync_playwright
import serial


ROOT = Path(__file__).resolve().parents[1]
CONTROLLER_HTML = ROOT / "data" / "controller.html"
EDGE_PATH = Path(
    "/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge"
)


class SerialLogCapture:
    def __init__(self, port: str) -> None:
        self._serial = serial.Serial(port, 115200, timeout=0.2)
        self._lines: list[str] = []
        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._read, daemon=True)

    def start(self) -> None:
        self._serial.reset_input_buffer()
        self._thread.start()

    def _read(self) -> None:
        while not self._stop.is_set():
            try:
                raw = self._serial.readline()
            except serial.SerialException:
                return
            if raw:
                self._lines.append(
                    raw.decode("utf-8", errors="replace").strip()
                )

    def text(self) -> str:
        return "\n".join(self._lines)

    def write(self, command: str) -> None:
        self._serial.write(command.encode())
        self._serial.flush()

    def close(self) -> None:
        self._stop.set()
        self._thread.join(timeout=1)
        self._serial.close()


class DeviceRequestThrottle:
    def __init__(self, interval_seconds: float) -> None:
        self._interval = max(0.0, interval_seconds)
        self._last_request = 0.0
        self._lock = threading.Lock()

    def __call__(self, route: Route) -> None:
        with self._lock:
            wait_seconds = self._interval - (time.monotonic() - self._last_request)
            if wait_seconds > 0:
                time.sleep(wait_seconds)
            self._last_request = time.monotonic()
        route.continue_()

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
    return CONTROLLER_HTML.read_text(encoding="utf-8")


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
        "startup": 0,
        "brightness": 100,
        "claudeStatus": True,
        "theme": 1,
        "carousel": False,
        "carouselSpeed": 12,
        "carouselOrder": [8, 9, 10, 6],
        "carouselFixed": 8,
        "nightDim": False,
        "nightStart": 22,
        "nightEnd": 7,
        "nightBrightness": 25,
    }
    prefs_update_count = 0
    expression_state: dict[str, str] = {
        "mode": "manual",
        "selected": "normal",
        "rendered": "normal",
    }
    profile: dict[str, str] = {
        "deviceName": "MOCHI",
        "bootLine1": "HELLO",
        "bootLine2": "MOCHI",
        "defaultExpression": "normal",
        "expressionMode": "manual",
    }

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
                if "theme" in args:
                    theme = int(args["theme"][0])
                    presets = {
                        1: ("#e22400", 100, 25, "normal"),
                        2: ("#050505", 80, 15, "grumpy"),
                        3: ("#10251f", 85, 20, "curious"),
                        4: ("#2a111d", 80, 18, "love"),
                    }
                    bg, brightness, night_brightness, expression = presets[theme]
                    self.__class__.prefs.update({
                        "theme": theme,
                        "bg": bg,
                        "brightness": brightness,
                        "nightBrightness": night_brightness,
                    })
                    self.__class__.profile.update({
                        "defaultExpression": expression,
                        "expressionMode": "manual",
                    })
                    self.__class__.expression_state = {
                        "mode": "manual",
                        "selected": expression,
                        "rendered": expression,
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
            self.send_json(
                {"version": "1.0.0-rc1", "busy": False, "brightness": 100}
            )
        elif path == "/expressions":
            self.send_json(
                {
                    **self.expression_state,
                    "expressions": [
                        {"id": name, "label": name.title()}
                        for name in (
                            "normal",
                            "happy",
                            "sleepy",
                            "sleeping",
                            "curious",
                            "surprised",
                            "grumpy",
                            "love",
                        )
                    ],
                }
            )
        elif path == "/profile":
            self.send_json(self.profile)
        elif path == "/config/export":
            self.send_json({
                "version": 1,
                "profile": self.profile,
                "preferences": self.prefs,
            })
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
        if path == "/expression":
            length = int(self.headers.get("Content-Length", "0"))
            payload = json.loads(self.rfile.read(length) or b"{}")
            if "id" in payload:
                self.__class__.expression_state = {
                    "mode": "manual",
                    "selected": payload["id"],
                    "rendered": payload["id"],
                }
            elif payload.get("mode") == "auto":
                self.__class__.expression_state = {
                    **self.expression_state,
                    "mode": "auto",
                    "rendered": "normal",
                }
            else:
                self.__class__.expression_state = {
                    **self.expression_state,
                    "mode": "manual",
                    "rendered": self.expression_state["selected"],
                }
            self.send_json(self.expression_state)
            return
        if path == "/profile":
            length = int(self.headers.get("Content-Length", "0"))
            self.__class__.profile = json.loads(
                self.rfile.read(length) or b"{}"
            )
            self.__class__.expression_state = {
                "mode": self.profile["expressionMode"],
                "selected": self.profile["defaultExpression"],
                "rendered": (
                    "normal"
                    if self.profile["expressionMode"] == "auto"
                    else self.profile["defaultExpression"]
                ),
            }
            self.send_json(self.profile)
            return
        if path == "/profile/reset":
            self.__class__.profile = {
                "deviceName": "MOCHI",
                "bootLine1": "HELLO",
                "bootLine2": "MOCHI",
                "defaultExpression": "normal",
                "expressionMode": "manual",
            }
            self.__class__.expression_state = {
                "mode": "manual",
                "selected": "normal",
                "rendered": "normal",
            }
            self.send_json(self.profile)
            return
        if path == "/config/import":
            length = int(self.headers.get("Content-Length", "0"))
            try:
                payload = json.loads(self.rfile.read(length) or b"{}")
            except json.JSONDecodeError:
                self.send_json({"error": "invalid json"}, 400)
                return
            if payload.get("version") != 1:
                self.send_json({"error": "unsupported configuration"}, 400)
                return
            self.__class__.profile = dict(payload["profile"])
            self.__class__.prefs = dict(payload["preferences"])
            self.__class__.expression_state = {
                "mode": self.profile["expressionMode"],
                "selected": self.profile["defaultExpression"],
                "rendered": self.profile["defaultExpression"],
            }
            self.send_json(payload)
            return
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


def open_controller(page: Page) -> None:
    page.goto("/?test=1", wait_until="domcontentloaded")
    page.wait_for_function("() => initialLoadComplete === true")


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
    open_controller(page)
    assert not page.locator("#mwrap").is_visible(), (
        "未选择 Crypto 时不应显示加密货币设置"
    )

    crypto_button = page.locator('button[data-v="9"]')
    crypto_button.click()
    page.locator("#mwrap.open").wait_for(state="visible")
    page.locator("#mSelected .mselrow").first.wait_for(state="visible")
    print("PASS  打开 Crypto 配置")

    existing_okb = page.locator("#mSelected .mselrow").filter(has_text="OKB")
    if existing_okb.count():
        existing_okb.get_by_role("button", name="Remove OKB").click()
        page.wait_for_timeout(250)

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


def run_wifi_status_flow(page: Page) -> None:
    rendered = page.evaluate(
        """() => wifiStatusHtml({
            connected: false,
            configured: true,
            savedSsid: 'UI-TEST',
            apIp: '192.168.4.1',
            lastError: 'Wrong password',
            retryCount: 2,
            retryExhausted: false,
            phase: 'Connecting'
        })"""
    )
    assert "Wrong password" in rendered and "retry 2" in rendered, (
        "WiFi 重试状态没有显示设备返回的具体失败原因和次数"
    )
    print("PASS  WiFi 显示具体连接失败原因")


def run_expression_flow(page: Page, *, stub_mode: bool) -> None:
    open_controller(page)
    buttons = page.locator("#expressionGrid .ebtn")
    assert buttons.count() == 8, "表情首屏没有显示完整的 8 种表情"
    buttons.filter(has_text="Love").click()
    page.locator("#exprCurrent").get_by_text("Love", exact=True).wait_for(
        state="visible"
    )
    assert page.locator(
        '.ebtn[data-expression="love"]'
    ).get_attribute("class") == "ebtn active"
    if stub_mode:
        assert FirmwareStubHandler.expression_state == {
            "mode": "manual",
            "selected": "love",
            "rendered": "love",
        }
    print("PASS  手动选择 Love 表情并保持")

    page.locator("#exprAuto").click()
    page.locator("#exprAuto").get_by_text("auto on", exact=False).wait_for(
        state="visible"
    )
    assert page.locator("#expressionGrid .ebtn.active").count() == 0
    if stub_mode:
        assert FirmwareStubHandler.expression_state["mode"] == "auto"
    print("PASS  主动开启可选 AUTO 模式")


def run_profile_flow(page: Page, *, stub_mode: bool) -> None:
    open_controller(page)
    page.locator("#profileName").fill("NOVA")
    page.locator("#profileBoot1").fill("WELCOME")
    page.locator("#profileBoot2").fill("NOVA")
    page.locator("#profileExpression").select_option("love")
    page.locator("#profileMode").select_option("manual")
    page.locator("#profileSave").click()
    page.get_by_text("profile saved", exact=True).wait_for(state="visible")
    assert page.locator(".sitename").text_content() == "NOVA · CONTROLLER"
    if stub_mode:
        assert FirmwareStubHandler.profile["defaultExpression"] == "love"
        assert FirmwareStubHandler.profile["bootLine1"] == "WELCOME"
    print("PASS  保存设备昵称、开机短句和默认表情")

    page.get_by_role("button", name="restore defaults").click()
    page.get_by_text("profile defaults restored", exact=True).wait_for(
        state="visible"
    )
    assert page.locator("#profileName").input_value() == "MOCHI"
    assert page.locator("#profileExpression").input_value() == "normal"
    print("PASS  恢复个性化默认值")


def run_theme_flow(page: Page, *, stub_mode: bool) -> None:
    open_controller(page)
    page.wait_for_function("() => document.querySelectorAll('.theme-btn').length === 4")
    current = int(page.evaluate("displayTheme"))
    expected = {
        1: ("#e22400", "normal"),
        2: ("#050505", "grumpy"),
        3: ("#10251f", "curious"),
        4: ("#2a111d", "love"),
    }
    for target in (1, 2, 3, 4):
        target_button = page.locator(f'.theme-btn[data-theme="{target}"]')
        target_button.click()
        page.wait_for_function(
            "(theme) => displayTheme === theme",
            arg=target,
        )
        page.wait_for_function(
            "(bg) => document.getElementById('bgCol').value.toLowerCase() === bg",
            arg=expected[target][0],
        )
        page.wait_for_function(
            "(expression) => document.getElementById('profileExpression').value === expression",
            arg=expected[target][1],
        )
        assert target_button.get_attribute("aria-pressed") == "true"
        assert page.locator("#bgCol").input_value().lower() == expected[target][0]
        assert page.locator("#profileExpression").input_value() == expected[target][1]
        if stub_mode:
            assert FirmwareStubHandler.prefs["theme"] == target

    original_button = page.locator(f'.theme-btn[data-theme="{current}"]')
    if current not in (1, 2, 3, 4):
        original_button = page.locator('.theme-btn[data-theme="1"]')
        current = 1
    original_button.click()
    page.wait_for_function(
        "(theme) => displayTheme === theme",
        arg=current,
    )
    assert original_button.get_attribute("aria-pressed") == "true"
    print("PASS  切换并持久化 Classic / Dark / Mint / Pink 四主题")


def run_config_flow(page: Page, *, stub_mode: bool) -> None:
    open_controller(page)
    exported = page.evaluate(
        """async () => {
            const r = await fetch('/config/export', {cache: 'no-store'});
            return await r.json();
        }"""
    )
    assert exported["version"] == 1
    assert "wifi" not in json.dumps(exported).lower()
    assert "password" not in json.dumps(exported).lower()
    assert "logs" not in exported
    print("PASS  导出配置不包含 WiFi、密码和日志")

    imported = json.loads(json.dumps(exported))
    imported["profile"]["deviceName"] = "IMPORT"
    imported["profile"]["defaultExpression"] = "curious"
    imported["profile"]["expressionMode"] = "manual"
    imported["preferences"]["theme"] = 3
    imported["preferences"]["bg"] = "#10251f"
    status = page.evaluate(
        """async config => {
            const r = await fetch('/config/import', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify(config)
            });
            return r.status;
        }""",
        imported,
    )
    assert status == 200
    refreshed = page.evaluate(
        "async () => await (await fetch('/profile')).json()"
    )
    assert refreshed["deviceName"] == "IMPORT"
    print("PASS  导入合法配置并应用")

    before = page.evaluate(
        "async () => await (await fetch('/profile')).json()"
    )
    invalid_status = page.evaluate(
        """async () => (await fetch('/config/import', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: '{"version":1,"profile":'
        })).status"""
    )
    after = page.evaluate(
        "async () => await (await fetch('/profile')).json()"
    )
    assert invalid_status == 400 and after == before
    print("PASS  非法配置被拒绝且现有配置不变")


def run_carousel_flow(page: Page, *, stub_mode: bool) -> None:
    open_controller(page)
    panel_button = page.locator("#carouselPanelBtn")
    panel_button.click()
    page.locator("#carouselWrap.open").wait_for(state="visible")
    assert page.locator("#carouselPanelState").text_content() == "close"
    print("PASS  展开信息轮播设置")
    toggle = page.locator("#carouselToggle")
    if "off" in (toggle.text_content() or ""):
        toggle.click()
    page.get_by_text("carousel on", exact=False).wait_for(state="visible")
    page.wait_for_timeout(350)
    assert page.locator("#carouselFixed").is_disabled()
    order_names = page.locator("#carouselOrder .rname").all_text_contents()
    assert set(order_names) == {"Weather", "Crypto", "Market", "Clock"}
    print("PASS  时间页面已加入轮播顺序")
    print("PASS  开启信息轮播")

    speed = page.locator("#carouselSpeed")
    assert not speed.is_disabled(), "开启轮播后速度滑块仍被禁用"
    target_speed = 18 if int(speed.input_value()) != 18 else 19
    speed.evaluate(
        """(el, value) => {
            el.value = String(value);
            el.dispatchEvent(new Event('input', {bubbles: true}));
        }""",
        target_speed,
    )
    page.wait_for_timeout(400)
    actual_speed = page.locator("#carouselSpeedV").text_content()
    selected_speed = int(speed.input_value())
    assert selected_speed == target_speed and actual_speed == f"{selected_speed}s", (
        f"轮播滑块未正确更新，显示为 {actual_speed!r}，"
        f"输入值为 {selected_speed!r}"
    )
    print(f"PASS  设置轮播间隔为 {selected_speed} 秒")

    first_name, second_name = order_names[:2]
    first_handle = page.get_by_role("button", name=f"Drag {first_name}")
    first_box = first_handle.bounding_box()
    second_row = page.locator("#carouselOrder .ritem").nth(1).bounding_box()
    assert first_box is not None and second_row is not None, "未找到轮播拖拽项目"
    first_handle.hover()
    page.mouse.move(first_box["x"] + first_box["width"] / 2, first_box["y"] + first_box["height"] / 2)
    page.mouse.down()
    page.mouse.move(first_box["x"] + first_box["width"] / 2, second_row["y"] + second_row["height"] * 0.8, steps=8)
    page.mouse.up()
    page.wait_for_timeout(150)
    assert page.locator("#carouselOrder .rname").first.text_content() == second_name
    print(f"PASS  拖拽调整轮播顺序为 {second_name} 优先")

    if "on" in (toggle.text_content() or ""):
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
    assert page.locator("#sSelected .mselrow").count() >= 1
    print("PASS  打开 Market 并显示已配置行情")

    existing_maotai = page.locator("#sSelected .mselrow").filter(
        has_text="600519"
    )
    if existing_maotai.count():
        existing_maotai.get_by_role(
            "button", name="Remove 600519"
        ).click()
        page.wait_for_timeout(250)

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
    parser.add_argument(
        "--serial-port",
        help="实机测试时同步断言串口日志，例如 /dev/cu.usbmodem1101",
    )
    parser.add_argument(
        "--serial-log-only",
        action="store_true",
        help="仅通过串口读取持久日志并断言关键功能动作",
    )
    parser.add_argument(
        "--request-interval",
        type=float,
        default=3.0,
        help="实机模式下相邻设备 HTTP 请求的最小间隔秒数，默认 3",
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
    assert "[Web] Expression manual: love" in delta
    assert "[Web] Expression mode: auto" in delta
    assert "[Web] Profile saved:" in delta
    assert "[Web] Profile reset to defaults" in delta
    assert "[Web] Theme applied: 4" in delta
    assert "[Web] Configuration imported:" in delta
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
    order = prefs.get("carouselOrder", [8, 9, 10, 6])
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


def main() -> int:
    args = parse_args()
    if args.serial_log_only:
        if not args.serial_port:
            raise SystemExit("--serial-log-only 需要同时提供 --serial-port")
        capture = SerialLogCapture(args.serial_port)
        capture.start()
        time.sleep(0.5)
        marker = f"SERIAL_{int(time.time())}"
        capture.write(f"log mark {marker}\n")
        time.sleep(0.3)
        capture.write("log 100\n")
        time.sleep(1)
        assert marker in capture.text()
        assert_serial_logs(capture.text())
        capture.close()
        return 0
    if not EDGE_PATH.exists():
        raise SystemExit(
            "未找到 Microsoft Edge。请安装 Edge，或修改脚本中的 EDGE_PATH。"
        )

    server: ThreadingHTTPServer | None = None
    original_device_assets: list[dict[str, Any]] | None = None
    original_device_market_assets: list[dict[str, Any]] | None = None
    original_device_prefs: dict[str, Any] | None = None
    original_device_profile: dict[str, Any] | None = None
    device_logs_before: str | None = None
    original_device_export: dict[str, Any] | None = None
    serial_capture: SerialLogCapture | None = None
    if args.device_url:
        base_url = args.device_url.rstrip("/") + "/"
        state = get_device_state(base_url)
        assert state["version"] == "1.0.0-rc1", state
        print("PASS  实机状态接口返回 RC1 版本号")
        original_device_assets = get_device_config(base_url)["assets"]
        original_device_market_assets = get_device_market_config(base_url)[
            "assets"
        ]
        original_device_prefs = get_device_prefs(base_url)
        original_device_profile = get_device_profile(base_url)
        original_device_export = get_device_export(base_url)
        device_logs_before = get_device_logs(base_url)
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
            "startup": 0,
            "brightness": 100,
            "claudeStatus": True,
            "theme": 1,
            "carousel": False,
            "carouselSpeed": 12,
            "carouselOrder": [8, 9, 10, 6],
            "carouselFixed": 8,
            "nightDim": False,
            "nightStart": 22,
            "nightEnd": 7,
            "nightBrightness": 25,
        }
        FirmwareStubHandler.prefs_update_count = 0
        FirmwareStubHandler.profile = {
            "deviceName": "MOCHI",
            "bootLine1": "HELLO",
            "bootLine2": "MOCHI",
            "defaultExpression": "normal",
            "expressionMode": "manual",
        }
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
            if args.device_url:
                page.route(
                    f"{base_url}**",
                    DeviceRequestThrottle(args.request_interval),
                )
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
                run_wifi_status_flow(page)
                run_expression_flow(page, stub_mode=not bool(args.device_url))
                run_profile_flow(page, stub_mode=not bool(args.device_url))
                run_theme_flow(page, stub_mode=not bool(args.device_url))
                run_config_flow(page, stub_mode=not bool(args.device_url))
                run_carousel_flow(page, stub_mode=not bool(args.device_url))
                run_market_flow(page, stub_mode=not bool(args.device_url))
                if device_logs_before is not None:
                    assert_device_logs(
                        device_logs_before, get_device_logs(base_url)
                    )
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
        if original_device_export is not None:
            restore_device_export(base_url, original_device_export)
            print("INFO  已恢复设备原有显示与个性化配置")
        elif original_device_prefs is not None:
            restore_device_carousel_prefs(base_url, original_device_prefs)
            print("INFO  已恢复设备原有轮播设置")
        if original_device_export is None and original_device_profile is not None:
            restore_device_profile(base_url, original_device_profile)
            print("INFO  已恢复设备原有个性化配置")
        if server is not None:
            server.shutdown()
            server.server_close()

    print("PASS  Crypto / Market Web UI 正向流程全部通过")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

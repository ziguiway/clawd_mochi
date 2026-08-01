"""CLI orchestration for the modular Web UI regression suite."""

from __future__ import annotations

import argparse
import threading
import time
from http.server import ThreadingHTTPServer
from pathlib import Path
from typing import Any

from playwright.sync_api import sync_playwright

from .cases.common import mock_coinlore
from .cases.games import run_game_arcade_flow
from .cases.market import run_market_flow
from .cases.media import run_media_flow
from .cases.productivity import run_salary_flow, run_timetable_import_flow
from .cases.settings import (
    run_carousel_flow,
    run_config_flow,
    run_expression_flow,
    run_font_flow,
    run_positive_flow,
    run_profile_flow,
    run_theme_flow,
    run_wifi_status_flow,
)
from .device import (
    assert_device_logs,
    assert_serial_logs,
    get_device_config,
    get_device_export,
    get_device_logs,
    get_device_market_config,
    get_device_prefs,
    get_device_profile,
    get_device_state,
    restore_device_carousel_prefs,
    restore_device_config,
    restore_device_export,
    restore_device_market_config,
    restore_device_profile,
)
from .fixtures import INITIAL_ASSETS, INITIAL_MARKET_ASSETS
from .infrastructure import (
    DeviceRequestThrottle,
    FirmwareStubHandler,
    SerialLogCapture,
)

EDGE_PATH = Path(
    "/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge"
)

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
            "fontStyle": "pixel",
            "carousel": False,
            "carouselSpeed": 12,
            "carouselOrder": [8, 9, 10, 6, 17],
            "carouselFixed": 8,
            "nightDim": False,
            "nightStart": 22,
            "nightEnd": 7,
            "nightBrightness": 25,
        }
        FirmwareStubHandler.prefs_update_count = 0
        FirmwareStubHandler.dino_state = {
            "active": False,
            "state": "ready",
            "score": 0,
            "highScore": 0,
            "speed": 92,
        }
        FirmwareStubHandler.dino_jump_count = 0
        FirmwareStubHandler.sokoban_state = {
            "active": False,
            "state": "playing",
            "level": 1,
            "levelCount": 8,
            "moves": 0,
            "pushes": 0,
            "boxesOnGoals": 0,
            "boxes": 6,
            "completedLevels": 0,
            "canUndo": False,
        }
        FirmwareStubHandler.sokoban_move_count = 0
        FirmwareStubHandler.arcade_states = {
            "tetris": {
                "id": "tetris", "active": False, "state": "playing",
                "score": 0, "highScore": 0, "lines": 0, "level": 1,
            },
            "snake": {
                "id": "snake", "active": False, "state": "ready",
                "score": 0, "highScore": 0, "length": 5,
                "speedMs": 175,
            },
            "2048": {
                "id": "2048", "active": False, "state": "playing",
                "score": 0, "bestScore": 0, "maxTile": 2,
                "canUndo": False,
            },
            "breakout": {
                "id": "breakout", "active": False, "state": "ready",
                "score": 0, "highScore": 0, "lives": 3,
                "level": 1, "bricks": 48,
            },
        }
        FirmwareStubHandler.active_arcade_game = ""
        FirmwareStubHandler.arcade_actions = []
        FirmwareStubHandler.salary_config = {
            "monthlyCents": 1_500_000,
            "workDaysX100": 2_175,
            "workMinutesPerDay": 480,
            "autoEnabled": True,
            "startMinutes": 570,
            "endMinutes": 1140,
        }
        FirmwareStubHandler.salary_status = {
            "state": "ready",
            "configured": True,
            "activeSeconds": 0,
            "earnedTenThousandths": 0,
            "dailyTargetTenThousandths": 6_896_551,
            "rateTenThousandths": 239,
            "progressPermille": 0,
        }
        FirmwareStubHandler.salary_actions = []
        FirmwareStubHandler.media_frame_count = 0
        FirmwareStubHandler.media_animation_count = 0
        FirmwareStubHandler.media_stop_count = 0
        FirmwareStubHandler.media_upload_bytes = 0
        FirmwareStubHandler.media_animation_bytes = 0
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
                if not args.device_url:
                    run_salary_flow(page)
                else:
                    print(
                        "INFO  实机模式跳过 Live Ledger 状态变更，"
                        "避免覆盖真实工资记录"
                    )
                run_expression_flow(page, stub_mode=not bool(args.device_url))
                run_profile_flow(page, stub_mode=not bool(args.device_url))
                run_theme_flow(page, stub_mode=not bool(args.device_url))
                run_font_flow(page, stub_mode=not bool(args.device_url))
                run_config_flow(page, stub_mode=not bool(args.device_url))
                run_carousel_flow(page, stub_mode=not bool(args.device_url))
                run_timetable_import_flow(
                    page, stub_mode=not bool(args.device_url)
                )
                run_media_flow(page, stub_mode=not bool(args.device_url))
                run_game_arcade_flow(page, stub_mode=not bool(args.device_url))
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


"""Timetable and live-ledger cases."""

import json
from pathlib import Path

from playwright.sync_api import Page

from ..infrastructure import FirmwareStubHandler
from .common import open_console_section, open_controller

def run_timetable_import_flow(page: Page, *, stub_mode: bool) -> None:
    open_controller(page)
    open_console_section(page, "modules")
    page.locator('button[data-v="18"]').click()
    page.locator("#uwrap.open").wait_for(state="visible")
    preview = page.locator("#utPreview")
    box = preview.bounding_box()
    assert box is not None
    assert round(box["width"]) == 240 and round(box["height"]) == 240, (
        f"Timetable preview should be 240x240, got "
        f"{box['width']:.0f}x{box['height']:.0f}"
    )
    assert preview.locator(".utp-head").text_content() == "NEXT CLASS"
    assert preview.locator(".utp-course").text_content() == "MACHINE\nLEARNING"
    assert preview.locator(".utp-count").text_content() == "42 MIN"
    page.wait_for_function(
        "() => !document.querySelector('#toast').classList.contains('show')"
    )
    page.wait_for_timeout(250)
    preview.screenshot(path="/tmp/clawd_mochi_timetable.png")
    print("PASS  Timetable opens with a live 240x240 device preview")
    page.get_by_role("button", name="IMPORT CLASSES").click()
    page.locator("#uImporter.open").wait_for(state="visible")
    print("PASS  打开手机课表 App 导入向导")

    if stub_mode:
        wakeup_data = "\n".join([
            json.dumps({"name": "test"}),
            json.dumps([
                {"node": 1, "startTime": "08:30", "endTime": "09:15"},
                {"node": 2, "startTime": "09:20", "endTime": "10:05"},
            ]),
            json.dumps({"settings": {"start_date": "2026/9/9 00:00:00"}}),
            json.dumps([{"id": 7, "courseName": "机器学习"}]),
            json.dumps([{
                "id": 7, "day": 1, "startNode": 1, "step": 2,
                "startWeek": 1, "endWeek": 16, "type": 0,
                "room": "N301", "teacher": "陈老师",
            }]),
        ])
        page.evaluate(
            "data => { window.WakeUpImport.importCode = async () => data; }",
            wakeup_data,
        )

    page.locator("#uPaste").fill(
        "这是来自「WakeUp课程表」的课表分享，分享口令为「test_share_code_1234」"
    )
    page.get_by_role("button", name="READ TIMETABLE").click()
    page.locator("#uPreview.open").wait_for(state="visible")
    assert page.locator("#uPreviewSource").text_content() == "WAKEUP"
    assert page.locator("#uPreviewCount").text_content() == "1"
    assert page.locator("#uPreviewReview").text_content() == "0"
    assert page.locator("#uReview input").input_value() == "MACHINE LEARNING"
    assert page.locator("#uPreviewTerm").input_value() == "2026-09-07"
    print("PASS  WakeUp 口令自动识别、研究生课程映射和预览")

    page.get_by_role("button", name="IMPORT TO MOCHI").click()
    page.locator("#uImporter").wait_for(state="hidden")
    page.locator("#uList").get_by_text(
        "MACHINE LEARNING", exact=True
    ).wait_for(state="visible")
    if stub_mode:
        assert FirmwareStubHandler.timetable["source"] == "wakeup"
        assert len(FirmwareStubHandler.timetable["courses"]) == 1
    print("PASS  课表确认并同步到设备")

def run_salary_flow(page: Page) -> None:
    open_controller(page)
    open_console_section(page, "modules")
    page.locator('button[data-v="17"]').click()
    page.locator("#ywrap.open").wait_for(state="visible")
    preview = page.locator("#ypreview")
    box = preview.bounding_box()
    assert box is not None
    assert round(box["width"]) == 240 and round(box["height"]) == 240, (
        f"Live Ledger 预览应为 240×240，实际为 "
        f"{box['width']:.0f}×{box['height']:.0f}"
    )
    assert page.locator("#ypAmount").text_content() == "0.000"
    assert page.evaluate("salaryMoney(99999999)") == "9999.999"
    assert page.evaluate("salaryMoney(100000000)") == "10000.000"
    layout = page.evaluate("""
        () => {
          const preview = document.querySelector('#ypreview').getBoundingClientRect();
          const rect = selector => {
            const box = document.querySelector(selector).getBoundingClientRect();
            return [Math.round(box.x-preview.x), Math.round(box.y-preview.y),
                    Math.round(box.width), Math.round(box.height)];
          };
          return {
            title: rect('.yptitle'), headRule: rect('.yprule.head'),
            currency: rect('.ypcurrency'), amount: rect('#ypAmount'),
            earnings: rect('.ypearnings'), middleRule: rect('.yprule.middle'),
            worked: rect('#ypWorked'), schedule: rect('#ypSchedule'),
            progress: rect('.ypprogress'), footerRule: rect('.yprule.footer'),
            footer: rect('.ypfoot')
          };
        }
    """)
    assert layout == {
        "title": [12, 8, 108, 16], "headRule": [12, 30, 216, 2],
        "currency": [36, 56, 9, 14], "amount": [32, 58, 196, 40],
        "earnings": [14, 108, 96, 8], "middleRule": [12, 124, 216, 1],
        "worked": [14, 136, 144, 24], "schedule": [12, 168, 216, 8],
        "progress": [12, 182, 216, 8], "footerRule": [12, 207, 216, 1],
        "footer": [12, 219, 216, 8],
    }
    print("PASS  Live Ledger 打开并显示 240×240 金额优先预览")

    page.locator("#ySettingsToggle").click()
    page.locator("#ySettings.open").wait_for(state="visible")
    page.locator("#yMonthlyInput").fill("15000")
    page.locator("#yDaysInput").fill("21.75")
    page.locator("#yHoursInput").fill("8")
    assert page.locator("#yStartInput").input_value() == "09:30"
    assert page.locator("#yEndInput").input_value() == "19:00"
    assert page.locator("#ypStart").text_content() == "09:30"
    assert page.locator("#ypEnd").text_content() == "19:00"
    page.locator("#yAutoInput").select_option("1")
    page.locator("#ySave").click()
    page.wait_for_function(
        "() => document.querySelector('#yMonthly').textContent === '15000'"
    )
    assert page.locator("#yMonthly").text_content() == "15000"
    assert FirmwareStubHandler.salary_config["startMinutes"] == 570
    assert FirmwareStubHandler.salary_config["endMinutes"] == 1140
    assert FirmwareStubHandler.salary_config["autoEnabled"] is True
    print("PASS  保存工资、09:30–19:00 上下班时间和自动班次")

    primary = page.locator("#yPrimary")
    primary.click()
    page.wait_for_function(
        "() => document.querySelector('#yState').textContent === 'RUNNING'"
    )
    assert page.locator("#ypLive").text_content() == "RUNNING"
    assert page.locator("#yMonthlyInput").is_disabled()
    assert page.locator("#yStartInput").is_disabled()
    assert primary.text_content() == "PAUSE"
    page.wait_for_timeout(180)
    assert page.locator("#ypAmount").text_content() != "0.000"
    print("PASS  RUNNING 金额在秒内连续滚动")
    print("PASS  开始上班后进入 RUNNING 并锁定计薪配置")

    primary.click()
    page.wait_for_function(
        "() => document.querySelector('#ypLive').textContent === 'PAUSED'"
    )
    assert primary.text_content() == "RESUME"
    assert page.locator("#ypAmount").text_content() == "12.345"
    print("PASS  暂停后冻结当前金额")

    primary.click()
    page.wait_for_function(
        "() => document.querySelector('#yState').textContent === 'RUNNING'"
    )
    screenshot = Path("/tmp/clawd_mochi_live_ledger.png")
    preview.screenshot(path=str(screenshot))
    assert screenshot.exists()
    print(f"PASS  Live Ledger 视觉验收截图已保存到 {screenshot}")

    page.once("dialog", lambda dialog: dialog.accept())
    page.locator("#yFinish").click()
    page.wait_for_function(
        "() => document.querySelector('#ypLive').textContent === 'DONE'"
    )
    assert not page.locator("#yMonthlyInput").is_disabled()
    print("PASS  继续计时并完成下班结算")

    page.locator("#ySettingsToggle").click()
    page.locator("#ySettings.open").wait_for(state="visible")
    page.once("dialog", lambda dialog: dialog.accept())
    page.locator("#yReset").click()
    page.wait_for_function(
        "() => document.querySelector('#ypLive').textContent === 'READY'"
    )
    assert FirmwareStubHandler.salary_actions == [
        "start",
        "pause",
        "resume",
        "finish",
        "reset",
    ]
    print("PASS  重置今日记录并完成完整状态流")

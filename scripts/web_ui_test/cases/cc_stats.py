"""Claude Code session/today statistics panel cases."""

from __future__ import annotations

from playwright.sync_api import Page

from ..infrastructure import FirmwareStubHandler
from .common import open_controller


def run_cc_stats_flow(page: Page, *, stub_mode: bool) -> None:
    """Open the Focus panel, verify live stats, and reset to zero."""
    if not stub_mode:
        # 实机模式下统计值随真实会话变化，且 reset 会清掉用户数据，
        # 这里只做轻量冒烟：打开面板并拉取一次 /cc/stats。
        open_controller(page)
        page.locator('button[data-v="20"]').click()
        page.locator("#fwrap.open").wait_for(state="visible")
        stats = page.evaluate(
            "async () => (await (await fetch('/cc/stats',{cache:'no-store'})).json())"
        )
        assert "todayMs" in stats and "done" in stats
        print("PASS  实机 Focus 面板打开并返回统计 JSON")
        return

    open_controller(page)
    page.locator('button[data-v="20"]').click()
    page.locator("#fwrap.open").wait_for(state="visible")

    # stub 默认统计：todayMs=12345000(3:25:45) sessionMs=600000(10:00)
    # longestMs=24000(0:24) done=3 error=1 permission=2
    page.wait_for_function(
        "() => document.querySelector('#fCounts').textContent === '3 / 1 / 2'"
    )
    assert page.locator("#fToday").text_content() == "3:25:45"
    assert page.locator("#fSession").text_content() == "10:00"
    assert page.locator("#fLongest").text_content() == "0:24"
    print("PASS  Focus 面板打开并显示今日/会话/最长/计数统计")

    # 直接 GET /cc/stats 校验 JSON 字段
    stats = page.evaluate(
        "async () => (await (await fetch('/cc/stats',{cache:'no-store'})).json())"
    )
    assert stats["todayMs"] == 12_345_000
    assert stats["sessionMs"] == 600_000
    assert stats["longestMs"] == 24_000
    assert stats["done"] == 3
    assert stats["error"] == 1
    assert stats["permission"] == 2
    assert stats["working"] is False
    print("PASS  /cc/stats 返回累计统计 JSON 且字段完整")

    # RESET STATS：确认对话框 → stub 归零 → 面板刷新
    page.once("dialog", lambda dialog: dialog.accept())
    page.locator("#fReset").click()
    page.wait_for_function(
        "() => document.querySelector('#fCounts').textContent === '0 / 0 / 0'"
    )
    assert page.locator("#fToday").text_content() == "0:00"
    assert page.locator("#fSession").text_content() == "0:00"
    assert page.locator("#fLongest").text_content() == "0:00"
    assert FirmwareStubHandler.cc_stats_reset_count == 1
    assert FirmwareStubHandler.cc_stats["todayMs"] == 0
    print("PASS  重置统计归零、刷新面板并命中设备 /cc/stats/reset")

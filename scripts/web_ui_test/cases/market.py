"""Market search and configuration cases."""

from playwright.sync_api import Page

from ..infrastructure import FirmwareStubHandler
from .common import open_console_section

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
    open_console_section(page, "modules")
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

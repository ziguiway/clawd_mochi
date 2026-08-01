"""Shared browser actions and assertions used by multiple cases."""

import json

from playwright.sync_api import Page, Route

from ..fixtures import COINLORE_ASSETS

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


#!/usr/bin/env python3
"""通过可见浏览器导入广外强智教务课表，并同步到 Clawd Mochi。

账号、密码和 Cookie 只存在于本机 Playwright 浏览器上下文。脚本仅向设备发送
课程、周次、时间、教师和地点。首次使用:

    uv add --dev playwright
    uv run playwright install chromium
    uv run scripts/import_gdufs_timetable.py \
      --device http://clawd-mochi.local --term-start 2026-09-07
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import urllib.error
import urllib.request
from pathlib import Path

LOGIN_URL = "https://jwxt.gdufs.edu.cn/jsxsd/"
ROOT = Path(__file__).resolve().parents[1]
NAME_MAP = ROOT / "docs/product/data/gdufs-graduate-cs-course-name-map.json"

# 仅用于把“第 N 节”转换成实际时间。学校调整作息时请通过 --periods 覆盖。
DEFAULT_PERIODS = {
    "1": ["08:30", "09:15"], "2": ["09:20", "10:05"],
    "3": ["10:25", "11:10"], "4": ["11:15", "12:00"],
    "5": ["14:00", "14:45"], "6": ["14:50", "15:35"],
    "7": ["15:55", "16:40"], "8": ["16:45", "17:30"],
    "9": ["19:00", "19:45"], "10": ["19:50", "20:35"],
    "11": ["20:40", "21:25"], "12": ["21:30", "22:15"],
}


def arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Import a GDUFS timetable")
    parser.add_argument("--device", default="http://clawd-mochi.local")
    parser.add_argument("--term-start", required=True,
                        help="first Monday of the term, YYYY-MM-DD")
    parser.add_argument("--periods", type=Path,
                        help='JSON object: {"1":["08:30","09:15"], ...}')
    parser.add_argument("--output", type=Path, default=Path("gdufs-timetable.json"))
    parser.add_argument("--no-sync", action="store_true")
    return parser.parse_args()


def load_names() -> dict[str, dict[str, str]]:
    data = json.loads(NAME_MAP.read_text(encoding="utf-8"))
    result: dict[str, dict[str, str]] = {}
    for course in data["courses"]:
        for source in course["sourceNames"]:
            result[re.sub(r"\s+", "", source)] = course
    return result


def normalize_lines(text: str) -> list[str]:
    return [line.strip() for line in text.replace("\xa0", " ").splitlines()
            if line.strip() and not set(line.strip()) <= {"-", "—"}]


def parse_block(text: str, day: int, names: dict[str, dict[str, str]],
                periods: dict[str, list[str]]) -> list[dict]:
    courses: list[dict] = []
    for raw in re.split(r"-{4,}|—{4,}", text):
        lines = normalize_lines(raw)
        if not lines:
            continue
        joined = " ".join(lines)
        section = re.search(r"(?:第|\[)?\s*(\d{1,2})\s*[-~至]\s*(\d{1,2})\s*节", joined)
        weeks = re.search(r"((?:\d+(?:-\d+)?)(?:,\d+(?:-\d+)?)*)\s*周", joined)
        if not section:
            continue
        first, last = str(int(section.group(1))), str(int(section.group(2)))
        if first not in periods or last not in periods:
            raise ValueError(f"missing period time for sections {first}-{last}")
        source_name = re.sub(r"\s+", "", lines[0])
        mapping = names.get(source_name)
        english = mapping["canonicalEnglish"] if mapping else ""
        display = mapping["displayName"] if mapping else ""
        short = mapping["shortName"] if mapping else ""
        teacher = ""
        room = ""
        for line in lines[1:]:
            value = re.sub(r"^(教师|老师)[:：]\s*", "", line)
            if value != line:
                teacher = value
            value = re.sub(r"^(教室|地点|上课地点)[:：]\s*", "", line)
            if value != line:
                room = value
        courses.append({
            "sourceName": lines[0],
            "englishName": english,
            "displayName": display,
            "shortName": short,
            "day": day,
            "weeks": weeks.group(1) if weeks else "",
            "start": periods[first][0],
            "end": periods[last][1],
            "room": room,
            "teacher": teacher,
        })
    return courses


def extract_cells(page) -> list[dict]:
    return page.locator("#kbtable .kbcontent, table .kbcontent").evaluate_all(
        """cells => cells.map(cell => ({
          day: cell.closest('td') ? cell.closest('td').cellIndex : 0,
          text: cell.innerText
        })).filter(item => item.day >= 1 && item.day <= 7 && item.text.trim())"""
    )


def open_schedule(page) -> None:
    selectors = [
        "text=我的课表", "text=学生课表查询", "text=个人课表",
        "a[href*='/jsxsd/xskb/']", "a[href*='xskb']",
    ]
    for selector in selectors:
        target = page.locator(selector)
        if target.count():
            target.first.click()
            page.wait_for_load_state("networkidle")
            return
    page.goto("https://jwxt.gdufs.edu.cn/jsxsd/xskb/xskb_list.do")
    page.wait_for_load_state("networkidle")


def fill_missing_names(courses: list[dict]) -> None:
    missing = [course for course in courses if not course["displayName"]]
    if not missing:
        return
    print("\nThese courses are not in the mapping table. Enter an English display name:")
    for course in missing:
        while not course["displayName"]:
            value = input(f"  {course['sourceName']}: ").strip().upper()
            if value:
                course["englishName"] = value.title()
                course["displayName"] = value
                course["shortName"] = value[:22]


def sync(device: str, payload: bytes) -> None:
    request = urllib.request.Request(
        device.rstrip("/") + "/timetable", data=payload,
        headers={"Content-Type": "application/json"}, method="POST")
    try:
        with urllib.request.urlopen(request, timeout=15) as response:
            if response.status != 200:
                raise RuntimeError(f"device returned HTTP {response.status}")
    except urllib.error.URLError as error:
        raise RuntimeError(f"cannot reach device: {error}") from error


def main() -> int:
    args = arguments()
    periods = DEFAULT_PERIODS
    if args.periods:
        periods = json.loads(args.periods.read_text(encoding="utf-8"))
    names = load_names()
    try:
        from playwright.sync_api import sync_playwright
    except ImportError:
        print("Playwright is required. Run: uv add --dev playwright", file=sys.stderr)
        return 2

    with sync_playwright() as playwright:
        browser = playwright.chromium.launch(headless=False)
        context = browser.new_context()
        page = context.new_page()
        page.goto(LOGIN_URL)
        print("Sign in to GDUFS in the opened browser.")
        input("After the student home page is visible, press Enter here...")
        open_schedule(page)
        cells = extract_cells(page)
        if not cells:
            screenshot = args.output.with_suffix(".failed.png")
            page.screenshot(path=str(screenshot), full_page=True)
            print(f"No timetable grid found. Diagnostic screenshot: {screenshot}",
                  file=sys.stderr)
            return 3
        courses = []
        for cell in cells:
            courses.extend(parse_block(cell["text"], cell["day"], names, periods))
        browser.close()

    if not courses:
        print("No course entries could be parsed.", file=sys.stderr)
        return 4
    fill_missing_names(courses)
    timetable = {
        "schemaVersion": 1,
        "school": "GDUFS",
        "termStart": args.term_start,
        "source": "gdufs-visible-browser",
        "courses": courses,
    }
    payload = json.dumps(timetable, ensure_ascii=False, indent=2).encode("utf-8")
    args.output.write_bytes(payload)
    print(f"Parsed {len(courses)} course rules. Preview saved to {args.output}")
    if not args.no_sync:
        answer = input(f"Sync these classes to {args.device}? [Y/n] ").strip().lower()
        if answer in {"", "y", "yes"}:
            sync(args.device, payload)
            print("Timetable synced.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Download every GIF exposed by an Aigei collection page.

The crawler uses a headed Chromium session so the user can complete Aigei's
normal login flow.  It only collects GIF URLs already exposed to that browser;
it does not attempt to bypass login, CAPTCHA, or download permissions.

Install once:
    uv run --with playwright playwright install chromium

Run:
    uv run --with playwright python scripts/data/download_aigei_gifs.py \
      'https://www.aigei.com/set/xiantiaoxiaogoubiaoq_1.html'
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import sys
import time
from pathlib import Path
from urllib.parse import parse_qsl, urlencode, urlsplit, urlunsplit

try:
    from playwright.sync_api import (
        BrowserContext,
        Error as PlaywrightError,
        Page,
        TimeoutError,
        sync_playwright,
    )
except ImportError:  # pragma: no cover - gives a useful error outside Playwright
    print(
        "缺少 Playwright。请先运行：\n"
        "  uv run --with playwright playwright install chromium\n"
        "然后用下面的命令启动：\n"
        "  uv run --with playwright python scripts/data/download_aigei_gifs.py URL",
        file=sys.stderr,
    )
    raise SystemExit(2)


DEFAULT_URL = "https://www.aigei.com/set/xiantiaoxiaogoubiaoq_1.html"
GIF_MAGIC = (b"GIF87a", b"GIF89a")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="从爱给网集合页收集浏览器已展示的 GIF 链接并下载。"
    )
    parser.add_argument("url", nargs="?", default=DEFAULT_URL, help="集合页 URL")
    parser.add_argument("-o", "--output", type=Path, default=Path("aigei_gifs"))
    parser.add_argument(
        "--profile-dir",
        type=Path,
        default=Path.home() / ".cache" / "aigei-gif-crawler-profile",
        help="保存登录状态的独立 Chromium 用户目录",
    )
    parser.add_argument("--max-pages", type=int, default=200, help="分页安全上限")
    parser.add_argument("--page-delay", type=float, default=1.2, help="翻页间隔（秒）")
    parser.add_argument("--download-delay", type=float, default=0.12, help="下载间隔（秒）")
    parser.add_argument(
        "--no-login-wait",
        action="store_true",
        help="已有有效登录状态时，不等待终端确认",
    )
    parser.add_argument("--headless", action="store_true", help="无头运行（首次运行不要使用）")
    return parser.parse_args()


def page_url(collection_url: str, number: int) -> str:
    parts = urlsplit(collection_url)
    query = dict(parse_qsl(parts.query, keep_blank_values=True))
    query["page"] = str(number)
    return urlunsplit((parts.scheme, parts.netloc, parts.path, urlencode(query), "resContainer"))


def canonical_asset(url: str) -> str | None:
    """Return a stable identity while ignoring the CDN signature/query string."""
    if not url or url.startswith(("data:", "blob:")):
        return None
    parts = urlsplit(url)
    path = parts.path
    if "/src/img/gif/" not in path.lower() and not path.lower().endswith(".gif"):
        return None
    return f"{parts.netloc.lower()}{path}"


def collect_visible_urls(page: Page) -> list[str]:
    """Collect media URLs from the DOM and the browser resource timeline."""
    urls = page.evaluate(
        """
        () => {
          const attrs = [
            'src', 'href', 'data-src', 'data-original', 'data-url',
            'data-lazy-src', 'data-actualsrc', 'data-gif', 'srcset'
          ];
          const found = new Set();
          for (const el of document.querySelectorAll('img, source, a, [data-src], [data-original]')) {
            for (const name of attrs) {
              const value = el.getAttribute(name);
              if (!value) continue;
              for (const part of value.split(',')) {
                const candidate = part.trim().split(/\\s+/)[0];
                if (!candidate) continue;
                try { found.add(new URL(candidate, location.href).href); } catch (_) {}
              }
            }
          }
          for (const entry of performance.getEntriesByType('resource')) {
            if (entry && entry.name) found.add(entry.name);
          }
          return [...found];
        }
        """
    )
    return [url for url in urls if canonical_asset(url)]


def fully_scroll(page: Page) -> None:
    """Trigger lazy-loaded thumbnails without hammering the page indefinitely."""
    last_height = -1
    stable_rounds = 0
    for _ in range(30):
        height = page.evaluate("() => document.documentElement.scrollHeight")
        page.evaluate("() => window.scrollTo(0, document.documentElement.scrollHeight)")
        page.wait_for_timeout(350)
        new_height = page.evaluate("() => document.documentElement.scrollHeight")
        if new_height == height == last_height:
            stable_rounds += 1
            if stable_rounds >= 2:
                break
        else:
            stable_rounds = 0
        last_height = new_height


def page_hints(page: Page) -> tuple[int | None, int | None]:
    """Read the largest visible page number and collection total, if present."""
    result = page.evaluate(
        """
        () => {
          const pages = [];
          for (const a of document.querySelectorAll('a[href*="page="]')) {
            try {
              const n = Number(new URL(a.href, location.href).searchParams.get('page'));
              if (Number.isInteger(n) && n > 0) pages.push(n);
            } catch (_) {}
          }
          const totalNode = document.querySelector('.tab-mount-cnt-search-all, .tab-mount-cnt');
          const match = totalNode && totalNode.textContent.match(/\\d+/);
          return {
            maxPage: pages.length ? Math.max(...pages) : null,
            total: match ? Number(match[0]) : null,
          };
        }
        """
    )
    return result["maxPage"], result["total"]


def logged_out(page: Page) -> bool:
    text = page.locator("body").inner_text(timeout=5_000)
    return "需要先登录后才能继续浏览" in text or "请登录后进行访问" in text


def collect_all(page: Page, args: argparse.Namespace) -> dict[str, str]:
    assets: dict[str, str] = {}
    target_pages: int | None = None
    empty_pages = 0

    for number in range(1, args.max_pages + 1):
        if target_pages is not None and number > target_pages:
            break

        url = page_url(args.url, number)
        print(f"[页面 {number}] {url}")
        try:
            page.goto(url, wait_until="domcontentloaded", timeout=45_000)
        except TimeoutError:
            print("  页面加载超时，继续检查已经加载的内容。")

        page.wait_for_timeout(int(args.page_delay * 1_000))
        if logged_out(page):
            raise RuntimeError("登录状态无效，页面要求登录。请重新运行并在浏览器中完成登录。")

        fully_scroll(page)
        found = collect_visible_urls(page)
        before = len(assets)
        for asset_url in found:
            key = canonical_asset(asset_url)
            if key:
                assets[key] = asset_url
        added = len(assets) - before
        print(f"  找到 {len(found)} 个 GIF 链接，新增 {added} 个，累计 {len(assets)} 个。")

        max_linked_page, total = page_hints(page)
        if max_linked_page:
            target_pages = min(args.max_pages, max(target_pages or 0, max_linked_page))
        if number == 1 and total and len(assets):
            inferred = math.ceil(total / len(assets))
            target_pages = min(args.max_pages, max(target_pages or 0, inferred))
            print(f"  集合标示共 {total} 项，预计检查 {target_pages} 页。")

        empty_pages = empty_pages + 1 if not found else 0
        if target_pages is None and empty_pages >= 2:
            print("连续两页没有 GIF，停止翻页。")
            break

    if not assets:
        raise RuntimeError("没有发现 GIF 链接；请确认集合能正常显示，且账号有权查看这些资源。")
    return assets


def safe_filename(key: str) -> str:
    name = Path(urlsplit("https://" + key).path).name
    name = re.sub(r"[^0-9A-Za-z._-]+", "_", name)
    if not name.lower().endswith(".gif"):
        name += ".gif"
    return name


def is_gif(data: bytes) -> bool:
    return data.startswith(GIF_MAGIC)


def download_all(
    context: BrowserContext, assets: dict[str, str], output: Path, referer: str, delay: float
) -> None:
    output.mkdir(parents=True, exist_ok=True)
    manifest_path = output / "manifest.jsonl"
    ok = skipped = failed = 0

    with manifest_path.open("a", encoding="utf-8") as manifest:
        for index, (key, url) in enumerate(sorted(assets.items()), 1):
            path = output / safe_filename(key)
            if path.exists() and is_gif(path.read_bytes()[:6]):
                skipped += 1
                print(f"[下载 {index}/{len(assets)}] 已存在：{path.name}")
                continue

            try:
                response = context.request.get(
                    url,
                    headers={"Referer": referer, "Accept": "image/gif,image/*;q=0.9,*/*;q=0.8"},
                    timeout=45_000,
                )
                data = response.body()
                if not response.ok or not is_gif(data):
                    raise RuntimeError(
                        f"HTTP {response.status}，返回内容不是 GIF（{len(data)} bytes）"
                    )
                path.write_bytes(data)
                record = {
                    "file": path.name,
                    "bytes": len(data),
                    "sha256": hashlib.sha256(data).hexdigest(),
                    "source_url": url,
                }
                manifest.write(json.dumps(record, ensure_ascii=False) + "\n")
                manifest.flush()
                ok += 1
                print(f"[下载 {index}/{len(assets)}] 完成：{path.name} ({len(data)} bytes)")
            except Exception as exc:  # continue so a later rerun can resume failures
                failed += 1
                print(f"[下载 {index}/{len(assets)}] 失败：{path.name}：{exc}", file=sys.stderr)
            time.sleep(max(0.0, delay))

    print(f"\n完成：新下载 {ok}，已存在 {skipped}，失败 {failed}。")
    print(f"文件目录：{output.resolve()}")
    print(f"清单文件：{manifest_path.resolve()}")


def main() -> int:
    args = parse_args()
    args.profile_dir.expanduser().mkdir(parents=True, exist_ok=True)

    with sync_playwright() as playwright:
        try:
            context = playwright.chromium.launch_persistent_context(
                str(args.profile_dir.expanduser()),
                headless=args.headless,
                viewport={"width": 1440, "height": 1000},
            )
        except PlaywrightError as exc:
            if "Executable doesn't exist" in str(exc):
                print(
                    "找不到 Playwright 浏览器，请先运行：\n"
                    "  uv run --with playwright playwright install chromium",
                    file=sys.stderr,
                )
                return 2
            raise
        page = context.pages[0] if context.pages else context.new_page()
        try:
            page.goto(page_url(args.url, 1), wait_until="domcontentloaded", timeout=45_000)
            if not args.no_login_wait:
                print("\n请在打开的浏览器中登录爱给网，并确认集合图片已显示。")
                input("完成后回到这里按 Enter 开始抓取……")
                page.reload(wait_until="domcontentloaded", timeout=45_000)
            assets = collect_all(page, args)
            download_all(context, assets, args.output, args.url, args.download_delay)
        except RuntimeError as exc:
            print(f"抓取停止：{exc}", file=sys.stderr)
            return 1
        finally:
            context.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

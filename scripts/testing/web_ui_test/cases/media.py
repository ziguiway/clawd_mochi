"""Static image and GIF media-cast cases."""

import struct

from playwright.sync_api import Page

from ..infrastructure import FirmwareStubHandler
from .common import open_console_section, open_controller

def make_test_bmp() -> bytes:
    """Create a dependency-free 2x2 24-bit BMP for browser upload tests."""
    rows = (
        b"\xff\x00\x00" + b"\xff\xff\xff" + b"\x00\x00",
        b"\x00\x00\xff" + b"\x00\xff\x00" + b"\x00\x00",
    )
    pixels = b"".join(rows)
    file_size = 14 + 40 + len(pixels)
    return (
        struct.pack("<2sIHHI", b"BM", file_size, 0, 0, 54)
        + struct.pack(
            "<IIIHHIIIIII", 40, 2, 2, 1, 24, 0, len(pixels),
            2835, 2835, 0, 0,
        )
        + pixels
    )

def make_test_gif() -> bytes:
    """Create a looping 1x1 black/white two-frame GIF."""
    return (
        b"GIF89a\x01\x00\x01\x00\x80\x00\x00"
        b"\x00\x00\x00\xff\xff\xff"
        b"\x21\xff\x0bNETSCAPE2.0\x03\x01\x00\x00\x00"
        b"\x21\xf9\x04\x00\x0a\x00\x00\x00"
        b"\x2c\x00\x00\x00\x00\x01\x00\x01\x00\x00"
        b"\x02\x02\x44\x01\x00"
        b"\x21\xf9\x04\x00\x0a\x00\x00\x00"
        b"\x2c\x00\x00\x00\x00\x01\x00\x01\x00\x00"
        b"\x02\x02\x4c\x01\x00\x3b"
    )

def run_media_flow(page: Page, *, stub_mode: bool) -> None:
    open_controller(page)
    open_console_section(page, "modules")
    page.locator('button[data-v="19"]').click()
    page.locator("#mediaWrap.open").wait_for(state="visible")
    preview = page.locator("#mediaPreview")
    box = preview.bounding_box()
    assert box is not None
    assert round(box["width"]) == 240 and round(box["height"]) == 240, (
        f"Media preview should be 240x240, got "
        f"{box['width']:.0f}x{box['height']:.0f}"
    )
    page.locator("#mediaFile").set_input_files({
        "name": "four-colors.bmp",
        "mimeType": "image/bmp",
        "buffer": make_test_bmp(),
    })
    page.wait_for_function(
        "() => document.querySelector('#mediaState').textContent === 'IMAGE READY'"
    )
    assert not page.locator("#mediaPlay").is_disabled()
    print("PASS  媒体面板以 240×240 像素画布预览 BMP")

    page.locator("#mediaPlay").click()
    page.wait_for_function(
        "() => document.querySelector('#mediaState').textContent === 'DISPLAYED'"
    )
    if stub_mode:
        assert FirmwareStubHandler.media_frame_count == 1
        assert FirmwareStubHandler.media_upload_bytes > 240 * 240 * 2
    assert not page.locator("#mediaStop").is_disabled()
    print("PASS  静态图转换并上传 240×240 RGB565 帧")

    page.locator("#mediaStop").click()
    page.wait_for_function(
        "() => document.querySelector('#mediaState').textContent === 'IMAGE READY'"
    )
    if stub_mode:
        assert FirmwareStubHandler.media_stop_count == 1
    print("PASS  媒体投屏可停止并释放设备侧会话")

    gif_start_animations = FirmwareStubHandler.media_animation_count
    page.locator("#mediaFile").set_input_files({
        "name": "loop.gif",
        "mimeType": "image/gif",
        "buffer": make_test_gif(),
    })
    page.wait_for_function(
        "() => document.querySelector('#mediaState').textContent === 'GIF READY'"
    )
    page.locator("#mediaPlay").click()
    page.wait_for_function(
        "() => document.querySelector('#mediaState').textContent === 'GIF PLAYING'"
    )
    if stub_mode:
        assert FirmwareStubHandler.media_animation_count == gif_start_animations + 1
        assert FirmwareStubHandler.media_animation_bytes > 16
    page.locator("#mediaStop").click()
    page.wait_for_function(
        "() => document.querySelector('#mediaState').textContent === 'GIF READY'"
    )
    print("PASS  GIF 经浏览器优化后上传，并在设备端本地解码")

"""Crypto, connectivity, appearance, profile, and carousel cases."""

import json

from playwright.sync_api import Page

from ..infrastructure import FirmwareStubHandler
from .common import assert_suggestion, open_console_section, open_controller, result_symbols

def run_positive_flow(page: Page, *, stub_mode: bool) -> None:
    open_controller(page)
    assert not page.locator("#mwrap").is_visible(), (
        "未选择 Crypto 时不应显示加密货币设置"
    )

    open_console_section(page, "modules")
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
    open_controller(page)
    open_console_section(page, "setup")
    page.evaluate(
        """() => renderWifiStatus({
            connected: true,
            configured: true,
            ssid: 'UI-TEST',
            savedSsid: 'UI-TEST',
            lanIp: '127.0.0.1',
            apIp: '192.168.4.1',
            apSsid: 'ClaWD-Mochi',
            mdns: 'http://clawd-mochi.local',
            changingNetwork: false,
            lastError: ''
        })"""
    )
    change_button = page.locator("#wscanBtn")
    assert change_button.is_visible()
    assert change_button.text_content() == "CHANGE NETWORK"
    change_button.click()
    page.locator("#wsetup.open").wait_for(state="visible")
    assert page.locator("#wform").is_visible()
    print("PASS  已联网时仍可进入更换网络流程")

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

    page.evaluate(
        """() => renderWifiStatus({
            connected: false,
            configured: false,
            ssid: 'ClaWD-Mochi',
            savedSsid: '',
            apIp: '192.168.4.1',
            apSsid: 'ClaWD-Mochi',
            changingNetwork: false,
            lastError: ''
        })"""
    )
    assert page.locator("#wscanBtn").text_content() == "CONNECT WIFI"
    assert page.locator("#wsetup").is_visible()
    assert "192.168.4.1" in (
        page.locator(".wrescue").text_content() or ""
    )
    print("PASS  无凭据时自动展开配网并持续显示 AP 救援入口")

def run_expression_flow(page: Page, *, stub_mode: bool) -> None:
    open_controller(page)
    buttons = page.locator("#expressionGrid .ebtn")
    assert buttons.count() == 8, "表情首屏没有显示完整的 8 种表情"
    assert buttons.filter(has_text="Thinking").count() == 1, (
        "Thinking 表情没有替换 Sleepy"
    )
    assert buttons.filter(has_text="Sleepy").count() == 0, (
        "Sleepy 表情仍然出现在控制器中"
    )
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
    open_console_section(page, "setup")
    assert not page.locator("#profileWrap").is_visible()
    page.locator("#profilePanelBtn").click()
    page.locator("#profileWrap").wait_for(state="visible")
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
    open_console_section(page, "control")
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
        if stub_mode:
            page.wait_for_function(
                "(bg) => document.getElementById('bgCol').value.toLowerCase() === bg",
                arg=expected[target][0],
            )
        else:
            page.wait_for_function(
                "() => /^#[0-9a-f]{6}$/i.test(document.getElementById('bgCol').value)"
            )
        page.wait_for_function(
            "(expression) => document.getElementById('profileExpression').value === expression",
            arg=expected[target][1],
        )
        assert target_button.get_attribute("aria-pressed") == "true"
        actual_bg = page.locator("#bgCol").input_value().lower()
        if stub_mode:
            assert actual_bg == expected[target][0]
        else:
            # 真机返回经过 ST7789 色彩补偿的设备背景色，主题状态仍需一致。
            assert len(actual_bg) == 7 and actual_bg[0] == "#"
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

def run_font_flow(page: Page, *, stub_mode: bool) -> None:
    open_controller(page)
    open_console_section(page, "control")
    selector = page.locator("#fontSelect")
    assert selector.locator("option").count() == 4, (
        "Font selector should expose four profiles"
    )
    page.wait_for_function("() => fontStyle === 'pixel'")

    for style in ("courier", "terminal", "dashboard", "pixel"):
        selector.select_option(style)
        page.wait_for_function("style => fontStyle === style", arg=style)
        assert selector.input_value() == style
        if stub_mode:
            assert FirmwareStubHandler.prefs["fontStyle"] == style

    print("PASS  简洁字体设置支持四套字体并可持久化切换")

def run_config_flow(page: Page, *, stub_mode: bool) -> None:
    open_controller(page)
    open_console_section(page, "setup")
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
    imported["preferences"]["fontStyle"] = "terminal"
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
    if stub_mode:
        assert FirmwareStubHandler.prefs["fontStyle"] == "terminal"
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
    open_console_section(page, "workspace")
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
    assert set(order_names) == {
        "Weather", "Crypto", "Market", "Clock", "Live Ledger",
    }
    print("PASS  时间页面和 Live Ledger 已加入轮播顺序")
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
    second_row = page.locator("#carouselOrder .ritem").nth(1)
    first_handle.drag_to(second_row)
    page.wait_for_timeout(500)
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

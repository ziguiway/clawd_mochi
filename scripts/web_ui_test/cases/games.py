"""Arcade launcher and game interaction cases."""

from playwright.sync_api import Page

from ..infrastructure import FirmwareStubHandler
from .common import open_console_section, open_controller

def run_game_arcade_flow(page: Page, *, stub_mode: bool) -> None:
    open_controller(page)
    open_console_section(page, "modules")
    page.locator("#dinoViewBtn").click()
    page.locator("#arcadeWrap.open").wait_for(state="visible")
    assert page.locator("#arcadeHome .game-card").count() == 6
    assert page.locator("#arcadeDinoCard").is_visible()
    assert page.locator("#arcadeSokobanCard").is_visible()
    assert page.locator("#arcadeTetrisCard").is_visible()
    assert page.locator("#arcadeSnakeCard").is_visible()
    assert page.locator("#arcade2048Card").is_visible()
    assert page.locator("#arcadeBreakoutCard").is_visible()
    print("PASS  独立游戏厅显示六款游戏")

    page.locator("#arcadeDinoCard").click()
    page.locator("#dinoWrap.open").wait_for(state="visible")
    assert page.locator("#dinoState").text_content() == "READY"
    assert page.locator("#dinoScore").text_content() == "0000"
    print("PASS  打开无物理按键的小恐龙控制器")

    jump = page.locator("#dinoJump")
    jump.dispatch_event("pointerdown")
    page.wait_for_function(
        "() => document.querySelector('#dinoState').textContent === 'RUNNING'"
    )
    page.wait_for_timeout(80)
    page.keyboard.press("Space")
    page.wait_for_timeout(100)
    if stub_mode:
        assert FirmwareStubHandler.dino_jump_count == 2, (
            "触摸与空格没有各发送一次跳跃指令"
        )
    print("PASS  触摸和空格键均可触发跳跃")

    page.locator("#dinoRestart").click()
    page.wait_for_function(
        "() => document.querySelector('#dinoState').textContent === 'READY'"
    )
    page.locator("#dinoExit").click()
    page.wait_for_function(
        "() => !document.querySelector('#dinoWrap').classList.contains('open')"
    )
    assert not page.locator("#dinoWrap").is_visible()
    assert page.locator("#arcadeHome").is_visible()
    print("PASS  小恐龙可重开并返回游戏厅")

    page.locator("#arcadeSokobanCard").click()
    page.locator("#sokobanWrap.open").wait_for(state="visible")
    assert page.locator("#sokobanState").text_content() == "PLAYING"
    assert page.locator("#sokobanLevel").text_content() == "01/08"
    assert page.locator("#sokobanMoves").text_content() == "000"
    print("PASS  推箱子控制器显示关卡和步数")

    page.locator(
        '#sokobanPad .dbtn[data-direction="up"]'
    ).dispatch_event("pointerdown")
    page.wait_for_function(
        "() => document.querySelector('#sokobanMoves').textContent === '001'"
    )
    page.wait_for_timeout(80)
    page.keyboard.press("ArrowRight")
    page.wait_for_function(
        "() => document.querySelector('#sokobanMoves').textContent === '002'"
    )
    if stub_mode:
        assert FirmwareStubHandler.sokoban_move_count == 2, (
            "方向键触控与键盘没有各发送一次移动指令"
        )
    print("PASS  推箱子支持触控方向键和电脑方向键")

    page.locator("#sokobanUndo").click()
    page.wait_for_function(
        "() => document.querySelector('#sokobanMoves').textContent === '001'"
    )
    page.locator("#sokobanRestart").click()
    page.wait_for_function(
        "() => document.querySelector('#sokobanMoves').textContent === '000'"
    )
    page.locator("#sokobanNext").click()
    page.wait_for_function(
        "() => document.querySelector('#sokobanLevel').textContent === '02/08'"
    )
    page.locator("#sokobanExit").click()
    page.wait_for_function(
        "() => !document.querySelector('#sokobanWrap').classList.contains('open')"
    )
    assert page.locator("#arcadeHome").is_visible()
    print("PASS  推箱子支持撤销、重开、选关并返回游戏厅")

    page.locator("#arcadeTetrisCard").click()
    page.locator("#tetrisWrap.open").wait_for(state="visible")
    page.locator(
        '#tetrisWrap [data-arcade-action="rotate"]'
    ).dispatch_event("pointerdown")
    page.wait_for_timeout(80)
    page.keyboard.press("Space")
    page.wait_for_function(
        "() => document.querySelector('#tetrisScore').textContent === '000020'"
    )
    assert page.locator("#tetrisLines").text_content() == "001"
    print("PASS  俄罗斯方块支持旋转、硬降和状态同步")
    page.locator("#tetrisWrap .gactions .cbtn").last.click()
    page.locator("#arcadeHome").wait_for(state="visible")

    page.locator("#arcadeSnakeCard").click()
    page.locator("#snakeWrap.open").wait_for(state="visible")
    page.locator(
        '#snakeWrap [data-arcade-action="up"]'
    ).dispatch_event("pointerdown")
    page.wait_for_function(
        "() => document.querySelector('#snakeState').textContent === 'PLAYING'"
    )
    assert page.locator("#snakeLength").text_content() == "006"
    print("PASS  贪吃蛇支持触控方向和实时长度")
    page.locator("#snakeWrap .gactions .cbtn").last.click()
    page.locator("#arcadeHome").wait_for(state="visible")

    page.locator("#arcade2048Card").click()
    page.locator("#game2048Wrap.open").wait_for(state="visible")
    page.keyboard.press("ArrowLeft")
    page.wait_for_function(
        "() => document.querySelector('#game2048Score').textContent === '00004'"
    )
    assert page.locator("#game2048Undo").is_enabled()
    page.locator("#game2048Undo").click()
    page.wait_for_function(
        "() => document.querySelector('#game2048Score').textContent === '00000'"
    )
    print("PASS  2048 支持方向操作和单步撤销")
    page.locator("#game2048Wrap .gactions .cbtn").last.click()
    page.locator("#arcadeHome").wait_for(state="visible")

    page.locator("#arcadeBreakoutCard").click()
    page.locator("#breakoutWrap.open").wait_for(state="visible")
    page.locator("#breakoutLaunch").click()
    page.wait_for_function(
        "() => document.querySelector('#breakoutState').textContent === 'PLAYING'"
    )
    page.locator("#breakoutPaddle").fill("700")
    page.wait_for_timeout(120)
    if stub_mode:
        assert ("breakout", "launch", 0) in FirmwareStubHandler.arcade_actions
        assert any(
            game == "breakout" and action == "position" and value == 700
            for game, action, value in FirmwareStubHandler.arcade_actions
        )
    print("PASS  打砖块支持发球和连续滑杆控制")
    page.locator("#breakoutWrap .gactions .cbtn").last.click()
    page.locator("#arcadeHome").wait_for(state="visible")

    page.locator("#arcadeClose").click()
    page.wait_for_function(
        "() => !document.querySelector('#arcadeWrap').classList.contains('open')"
    )
    print("PASS  游戏厅可关闭并返回主控制面板")

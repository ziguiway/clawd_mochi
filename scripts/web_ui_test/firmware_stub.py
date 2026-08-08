"""Stateful HTTP stub that mirrors the embedded firmware API."""

from __future__ import annotations

import json
import urllib.parse
from http.server import BaseHTTPRequestHandler
from pathlib import Path
from typing import Any

from .fixtures import INITIAL_ASSETS, INITIAL_MARKET_ASSETS, MARKET_SEARCH_RESULTS

ROOT = Path(__file__).resolve().parents[2]
CONTROLLER_HTML = ROOT / "data" / "controller.html"


def extract_index_html() -> str:
    return CONTROLLER_HTML.read_text(encoding="utf-8")


class FirmwareStubHandler(BaseHTTPRequestHandler):
    html = extract_index_html()
    wakeup_js = (ROOT / "data" / "wakeup_import.js").read_text()
    gif_reader_js = (ROOT / "data" / "gif_reader.js").read_text()
    gif_encoder_js = (ROOT / "data" / "gif_encoder.js").read_text()
    media_js = (ROOT / "data" / "media.js").read_text()
    media_frame_count = 0
    media_animation_count = 0
    media_stop_count = 0
    media_upload_bytes = 0
    media_animation_bytes = 0
    stream_status: dict[str, Any] = {
        "active": False, "connected": False, "fps": 0.0, "frames": 0,
    }
    stream_enter_count = 0
    stream_exit_count = 0
    assets: list[dict[str, Any]] = [dict(item) for item in INITIAL_ASSETS]
    market_assets: list[dict[str, Any]] = [
        dict(item) for item in INITIAL_MARKET_ASSETS
    ]
    config_post_count = 0
    market_post_count = 0
    prefs: dict[str, Any] = {
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
    prefs_update_count = 0
    expression_state: dict[str, str] = {
        "mode": "manual",
        "selected": "normal",
        "rendered": "normal",
    }
    profile: dict[str, str] = {
        "deviceName": "MOCHI",
        "bootLine1": "HELLO",
        "bootLine2": "MOCHI",
        "defaultExpression": "normal",
        "expressionMode": "manual",
    }
    dino_state: dict[str, Any] = {
        "active": False,
        "state": "ready",
        "score": 0,
        "highScore": 0,
        "speed": 92,
    }
    dino_jump_count = 0
    sokoban_state: dict[str, Any] = {
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
    sokoban_move_count = 0
    arcade_states: dict[str, dict[str, Any]] = {
        "tetris": {
            "id": "tetris", "active": False, "state": "playing",
            "score": 0, "highScore": 0, "lines": 0, "level": 1,
        },
        "snake": {
            "id": "snake", "active": False, "state": "ready",
            "score": 0, "highScore": 0, "length": 5, "speedMs": 175,
        },
        "2048": {
            "id": "2048", "active": False, "state": "playing",
            "score": 0, "bestScore": 0, "maxTile": 2, "canUndo": False,
        },
        "breakout": {
            "id": "breakout", "active": False, "state": "ready",
            "score": 0, "highScore": 0, "lives": 3, "level": 1,
            "bricks": 48,
        },
    }
    active_arcade_game = ""
    arcade_actions: list[tuple[str, str, int]] = []
    salary_config: dict[str, Any] = {
        "monthlyCents": 1_500_000,
        "workDaysX100": 2_175,
        "workMinutesPerDay": 480,
        "autoEnabled": True,
        "startMinutes": 570,
        "endMinutes": 1140,
    }
    salary_status: dict[str, Any] = {
        "state": "ready",
        "configured": True,
        "activeSeconds": 0,
        "earnedTenThousandths": 0,
        "dailyTargetTenThousandths": 6_896_551,
        "rateTenThousandths": 239,
        "progressPermille": 0,
    }
    salary_actions: list[str] = []
    timetable: dict[str, Any] = {
        "schemaVersion": 1,
        "school": "GDUFS",
        "termStart": "2026-09-07",
        "courses": [
            {
                "sourceName": "机器学习",
                "englishName": "Machine Learning",
                "displayName": "MACHINE LEARNING",
                "shortName": "ML",
                "day": 1,
                "weeks": "1-16",
                "start": "08:30",
                "end": "10:05",
                "room": "N301",
                "teacher": "PROF. CHEN",
            }
        ],
    }

    def log_message(self, _format: str, *_args: object) -> None:
        return

    def send_json(self, payload: Any, status: int = 200) -> None:
        body = json.dumps(payload).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path
        if path == "/":
            body = self.html.encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        elif path == "/wakeup_import.js":
            body = self.wakeup_js.encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/javascript")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        elif path == "/media.js":
            body = self.media_js.encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/javascript")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        elif path == "/gif_reader.js":
            body = self.gif_reader_js.encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/javascript")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        elif path == "/gif_encoder.js":
            body = self.gif_encoder_js.encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/javascript")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        elif path == "/stream/status":
            self.send_json(self.stream_status)
        elif path == "/crypto/config":
            self.send_json(
                {
                    "loading": False,
                    "updatedAgeSec": 5,
                    "assets": self.assets,
                }
            )
        elif path == "/market/config":
            self.send_json(
                {
                    "loading": False,
                    "updatedAgeSec": 5,
                    "assets": self.market_assets,
                }
            )
        elif path == "/market/search":
            self.send_json({"results": MARKET_SEARCH_RESULTS})
        elif path == "/prefs":
            args = urllib.parse.parse_qs(parsed.query)
            if args:
                if "carousel" in args:
                    self.__class__.prefs["carousel"] = args["carousel"][0] in {
                        "1",
                        "true",
                    }
                if "theme" in args:
                    theme = int(args["theme"][0])
                    presets = {
                        1: ("#e22400", 100, 25, "normal"),
                        2: ("#050505", 80, 15, "grumpy"),
                        3: ("#10251f", 85, 20, "curious"),
                        4: ("#2a111d", 80, 18, "love"),
                    }
                    bg, brightness, night_brightness, expression = presets[theme]
                    self.__class__.prefs.update({
                        "theme": theme,
                        "bg": bg,
                        "brightness": brightness,
                        "nightBrightness": night_brightness,
                    })
                    self.__class__.profile.update({
                        "defaultExpression": expression,
                        "expressionMode": "manual",
                    })
                    self.__class__.expression_state = {
                        "mode": "manual",
                        "selected": expression,
                        "rendered": expression,
                    }
                if "fontStyle" in args:
                    style = args["fontStyle"][0]
                    if style not in {"pixel", "courier", "terminal", "dashboard"}:
                        self.send_json({"error": "unknown font style"}, 400)
                        return
                    self.__class__.prefs["fontStyle"] = style
                if "carouselSpeed" in args:
                    self.__class__.prefs["carouselSpeed"] = int(
                        args["carouselSpeed"][0]
                    )
                if "carouselFixed" in args:
                    self.__class__.prefs["carouselFixed"] = int(
                        args["carouselFixed"][0]
                    )
                if "carouselOrder" in args:
                    self.__class__.prefs["carouselOrder"] = [
                        int(value)
                        for value in args["carouselOrder"][0].split(",")
                    ]
                self.__class__.prefs_update_count += 1
            self.send_json(self.prefs)
        elif path == "/state":
            self.send_json(
                {"version": "1.0.0-rc1", "busy": False, "brightness": 100}
            )
        elif path == "/game/dino/state":
            self.send_json(self.dino_state)
        elif path == "/game/sokoban/state":
            self.send_json(self.sokoban_state)
        elif path == "/game/state":
            args = urllib.parse.parse_qs(parsed.query)
            game_id = args.get(
                "id", [self.active_arcade_game or "tetris"]
            )[0]
            self.send_json(self.arcade_states.get(game_id, {"active": False}))
        elif path == "/game/catalog":
            self.send_json({
                "games": [
                    {"id": game_id}
                    for game_id in (
                        "dino", "sokoban", "tetris", "snake",
                        "2048", "breakout",
                    )
                ]
            })
        elif path == "/expressions":
            self.send_json(
                {
                    **self.expression_state,
                    "expressions": [
                        {"id": name, "label": name.title()}
                        for name in (
                            "normal",
                            "happy",
                            "thinking",
                            "sleeping",
                            "curious",
                            "surprised",
                            "grumpy",
                            "love",
                        )
                    ],
                }
            )
        elif path == "/profile":
            self.send_json(self.profile)
        elif path == "/config/export":
            self.send_json({
                "version": 1,
                "profile": self.profile,
                "preferences": self.prefs,
            })
        elif path == "/wifi/status":
            self.send_json(
                {
                    "connected": True,
                    "ssid": "UI-TEST",
                    "lanIp": "127.0.0.1",
                    "ip": "127.0.0.1",
                    "apIp": "192.168.4.1",
                    "apSsid": "ClaWD-Mochi",
                    "mdns": "http://clawd-mochi.local",
                    "configured": True,
                    "savedSsid": "UI-TEST",
                    "changingNetwork": False,
                    "lastError": "",
                }
            )
        elif path == "/timer/status":
            self.send_json(
                {
                    "phase": "focus",
                    "running": False,
                    "paused": False,
                    "remaining": 1500,
                    "focus": 25,
                    "break": 5,
                }
            )
        elif path == "/salary/config":
            self.send_json(
                {
                    **self.salary_config,
                    "locked": self.salary_status["state"] in {
                        "running",
                        "paused",
                    },
                }
            )
        elif path == "/salary/status":
            self.send_json(self.salary_status)
        elif path == "/timetable":
            self.send_json(self.timetable)
        elif path == "/timetable/status":
            self.send_json({
                "configured": True, "ready": True, "state": "next_class",
                "week": 1, "weekday": 1, "todayTotal": 1,
                "todayCompleted": 0, "remainingToday": 1,
                "minutesRemaining": 42, "hasNextCourse": False,
                "course": {
                    "name": "MACHINE LEARNING", "shortName": "ML",
                    "room": "N301", "teacher": "PROF. CHEN",
                    "start": "08:30", "end": "10:05", "day": 1,
                },
            })
        elif path == "/media/status":
            self.send_json({
                "active": (
                    self.media_frame_count + self.media_animation_count
                    > self.media_stop_count
                ),
                "width": 240,
                "height": 240,
                "pixelFormat": "rgb565be",
                "fsFree": 700 * 1024,
            })
        else:
            self.send_json({"ok": True})

    def do_POST(self) -> None:
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path
        args = urllib.parse.parse_qs(parsed.query)
        if path == "/stream/enter":
            self.__class__.stream_enter_count += 1
            self.__class__.stream_status = {
                "active": True, "connected": False, "fps": 0.0, "frames": 0,
            }
            self.send_json(self.stream_status)
            return
        if path == "/stream/exit":
            self.__class__.stream_exit_count += 1
            self.__class__.stream_status = {
                "active": False, "connected": False, "fps": 0.0, "frames": 0,
            }
            self.send_json(self.stream_status)
            return
        if path == "/media/frame":
            length = int(self.headers.get("Content-Length", "0"))
            self.rfile.read(length)
            self.__class__.media_frame_count += 1
            self.__class__.media_upload_bytes = length
            self.send_json({"ok": True, "width": 240, "height": 240})
            return
        if path == "/media/animation":
            length = int(self.headers.get("Content-Length", "0"))
            self.rfile.read(length)
            self.__class__.media_animation_count += 1
            self.__class__.media_animation_bytes = length
            self.send_json({"ok": True, "bytes": length, "uploadMs": 1})
            return
        if path == "/media/stop":
            length = int(self.headers.get("Content-Length", "0"))
            if length:
                self.rfile.read(length)
            self.__class__.media_stop_count += 1
            self.send_json({"ok": True})
            return
        if path == "/timetable/import/wakeup":
            length = int(self.headers.get("Content-Length", "0"))
            payload = json.loads(self.rfile.read(length) or b"{}")
            if not payload.get("code"):
                self.send_json({"error": "share code required"}, 400)
                return
            wakeup_data = "\n".join([
                json.dumps({"name": "test"}),
                json.dumps([
                    {"node": 1, "startTime": "08:30", "endTime": "09:15"},
                    {"node": 2, "startTime": "09:20", "endTime": "10:05"},
                ]),
                json.dumps({"startDate": "2026-09-07"}),
                json.dumps([{"id": 7, "courseName": "机器学习"}]),
                json.dumps([{
                    "id": 7, "day": 1, "startNode": 1, "step": 2,
                    "startWeek": 1, "endWeek": 16, "type": 0,
                    "room": "N301", "teacher": "陈老师",
                }]),
            ])
            self.send_json({"status": 1, "data": wakeup_data})
            return
        if path == "/timetable":
            length = int(self.headers.get("Content-Length", "0"))
            payload = json.loads(self.rfile.read(length) or b"{}")
            if not payload.get("termStart") or not isinstance(payload.get("courses"), list):
                self.send_json({"error": "invalid timetable"}, 400)
                return
            self.__class__.timetable = payload
            self.send_json({"ok": True})
            return
        if path == "/salary/config":
            length = int(self.headers.get("Content-Length", "0"))
            payload = json.loads(self.rfile.read(length) or b"{}")
            monthly_cents = int(payload["monthlyCents"])
            work_days_x100 = int(payload["workDaysX100"])
            work_minutes = int(payload["workMinutesPerDay"])
            day_seconds = work_minutes * 60
            self.__class__.salary_config = {
                "monthlyCents": monthly_cents,
                "workDaysX100": work_days_x100,
                "workMinutesPerDay": work_minutes,
                "autoEnabled": bool(payload.get("autoEnabled", True)),
                "startMinutes": int(payload.get("startMinutes", 570)),
                "endMinutes": int(payload.get("endMinutes", 1140)),
            }
            self.__class__.salary_status = {
                **self.salary_status,
                "configured": True,
                "dailyTargetTenThousandths": (
                    monthly_cents * 10_000 // work_days_x100
                ),
                "rateTenThousandths": (
                    monthly_cents * 10_000
                    // (work_days_x100 * day_seconds)
                ),
            }
            self.send_json(self.salary_status)
            return
        if path.startswith("/salary/"):
            action = path.rsplit("/", 1)[-1]
            self.__class__.salary_actions.append(action)
            state = {
                "start": "running",
                "pause": "paused",
                "resume": "running",
                "finish": "finished",
                "reset": "ready",
            }.get(action)
            if state is None:
                self.send_json({"error": "unknown salary action"}, 404)
                return
            reset_values = action in {"start", "reset"}
            self.__class__.salary_status = {
                **self.salary_status,
                "state": state,
                "activeSeconds": 0
                if reset_values
                else self.salary_status["activeSeconds"],
                "earnedTenThousandths": 0
                if reset_values
                else self.salary_status["earnedTenThousandths"],
                "progressPermille": 0
                if reset_values
                else self.salary_status["progressPermille"],
            }
            if action in {"pause", "resume", "finish"}:
                self.__class__.salary_status.update(
                    activeSeconds=12_068,
                    earnedTenThousandths=123_456,
                    progressPermille=340,
                )
            self.send_json(self.salary_status)
            return
        if path == "/game/dino/start":
            self.__class__.dino_state = {
                **self.dino_state,
                "active": True,
                "state": "ready",
                "score": 0,
            }
            self.send_json(self.dino_state)
            return
        if path == "/game/dino/action":
            self.__class__.dino_jump_count += 1
            self.__class__.dino_state = {
                **self.dino_state,
                "state": "running",
            }
            self.send_json(self.dino_state)
            return
        if path == "/game/dino/restart":
            self.__class__.dino_state = {
                **self.dino_state,
                "state": "ready",
                "score": 0,
            }
            self.send_json(self.dino_state)
            return
        if path == "/game/dino/exit":
            self.__class__.dino_state = {
                **self.dino_state,
                "active": False,
            }
            self.send_json(self.dino_state)
            return
        if path == "/game/sokoban/start":
            self.__class__.sokoban_state = {
                **self.sokoban_state,
                "active": True,
                "state": "playing",
                "moves": 0,
                "pushes": 0,
                "canUndo": False,
            }
            self.send_json(self.sokoban_state)
            return
        if path == "/game/sokoban/move":
            self.__class__.sokoban_move_count += 1
            self.__class__.sokoban_state = {
                **self.sokoban_state,
                "moves": self.sokoban_state["moves"] + 1,
                "canUndo": True,
            }
            self.send_json(self.sokoban_state)
            return
        if path == "/game/sokoban/undo":
            self.__class__.sokoban_state = {
                **self.sokoban_state,
                "moves": max(0, self.sokoban_state["moves"] - 1),
                "canUndo": self.sokoban_state["moves"] > 1,
            }
            self.send_json(self.sokoban_state)
            return
        if path == "/game/sokoban/restart":
            self.__class__.sokoban_state = {
                **self.sokoban_state,
                "state": "playing",
                "moves": 0,
                "pushes": 0,
                "canUndo": False,
            }
            self.send_json(self.sokoban_state)
            return
        if path == "/game/sokoban/level":
            level = int(args.get("index", ["1"])[0])
            self.__class__.sokoban_state = {
                **self.sokoban_state,
                "state": "playing",
                "level": level,
                "moves": 0,
                "pushes": 0,
                "canUndo": False,
            }
            self.send_json(self.sokoban_state)
            return
        if path == "/game/sokoban/exit":
            self.__class__.sokoban_state = {
                **self.sokoban_state,
                "active": False,
            }
            self.send_json(self.sokoban_state)
            return
        if path == "/game/start":
            game_id = args.get("id", [""])[0]
            if game_id not in self.arcade_states:
                self.send_json({"error": "unknown game"}, 400)
                return
            self.__class__.active_arcade_game = game_id
            state = {
                **self.arcade_states[game_id],
                "active": True,
                "state": "ready" if game_id in {"snake", "breakout"}
                else "playing",
            }
            self.__class__.arcade_states[game_id] = state
            self.send_json(state)
            return
        if path == "/game/action":
            game_id = self.active_arcade_game
            action = args.get("action", [""])[0]
            value = int(args.get("value", ["0"])[0])
            if not game_id:
                self.send_json({"error": "game is not active"}, 409)
                return
            self.__class__.arcade_actions.append((game_id, action, value))
            state = {**self.arcade_states[game_id]}
            if game_id == "tetris" and action == "drop":
                state.update(score=20, lines=1)
            elif game_id == "snake" and action in {
                "up", "down", "left", "right"
            }:
                state.update(state="playing", score=1, length=6)
            elif game_id == "2048":
                if action == "undo":
                    state.update(score=0, maxTile=2, canUndo=False)
                elif action in {"up", "down", "left", "right"}:
                    state.update(score=4, bestScore=4, maxTile=4, canUndo=True)
            elif game_id == "breakout" and action == "launch":
                state.update(state="playing", score=10)
            self.__class__.arcade_states[game_id] = state
            self.send_json(state)
            return
        if path == "/game/exit":
            game_id = self.active_arcade_game
            if game_id:
                self.__class__.arcade_states[game_id] = {
                    **self.arcade_states[game_id],
                    "active": False,
                }
            self.__class__.active_arcade_game = ""
            self.send_json({"active": False, "state": "closed"})
            return
        if path == "/expression":
            length = int(self.headers.get("Content-Length", "0"))
            payload = json.loads(self.rfile.read(length) or b"{}")
            if "id" in payload:
                self.__class__.expression_state = {
                    "mode": "manual",
                    "selected": payload["id"],
                    "rendered": payload["id"],
                }
            elif payload.get("mode") == "auto":
                self.__class__.expression_state = {
                    **self.expression_state,
                    "mode": "auto",
                    "rendered": "normal",
                }
            else:
                self.__class__.expression_state = {
                    **self.expression_state,
                    "mode": "manual",
                    "rendered": self.expression_state["selected"],
                }
            self.send_json(self.expression_state)
            return
        if path == "/profile":
            length = int(self.headers.get("Content-Length", "0"))
            self.__class__.profile = json.loads(
                self.rfile.read(length) or b"{}"
            )
            self.__class__.expression_state = {
                "mode": self.profile["expressionMode"],
                "selected": self.profile["defaultExpression"],
                "rendered": (
                    "normal"
                    if self.profile["expressionMode"] == "auto"
                    else self.profile["defaultExpression"]
                ),
            }
            self.send_json(self.profile)
            return
        if path == "/profile/reset":
            self.__class__.profile = {
                "deviceName": "MOCHI",
                "bootLine1": "HELLO",
                "bootLine2": "MOCHI",
                "defaultExpression": "normal",
                "expressionMode": "manual",
            }
            self.__class__.expression_state = {
                "mode": "manual",
                "selected": "normal",
                "rendered": "normal",
            }
            self.send_json(self.profile)
            return
        if path == "/config/import":
            length = int(self.headers.get("Content-Length", "0"))
            try:
                payload = json.loads(self.rfile.read(length) or b"{}")
            except json.JSONDecodeError:
                self.send_json({"error": "invalid json"}, 400)
                return
            if payload.get("version") != 1:
                self.send_json({"error": "unsupported configuration"}, 400)
                return
            self.__class__.profile = dict(payload["profile"])
            self.__class__.prefs = dict(payload["preferences"])
            self.__class__.expression_state = {
                "mode": self.profile["expressionMode"],
                "selected": self.profile["defaultExpression"],
                "rendered": self.profile["defaultExpression"],
            }
            self.send_json(payload)
            return
        if path not in {"/crypto/config", "/market/config"}:
            self.send_json({"ok": True})
            return

        length = int(self.headers.get("Content-Length", "0"))
        payload = json.loads(self.rfile.read(length) or b"{}")
        if path == "/crypto/config":
            self.__class__.assets = payload.get("assets", [])
            self.__class__.config_post_count += 1
            assets = self.assets
        else:
            self.__class__.market_assets = payload.get("assets", [])
            self.__class__.market_post_count += 1
            assets = self.market_assets
        self.send_json(
            {
                "loading": False,
                "updatedAgeSec": 0,
                "assets": assets,
            }
        )

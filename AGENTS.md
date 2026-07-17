# AGENTS.md

This file provides guidance to Codex (Codex.ai/code) when working with code in this repository.

## Project Overview

Clawd Mochi is an ESP32-C3 desk companion that displays animated expressions and Codex status on a 1.54" ST7789 TFT (240x240). It hosts a WiFi AP + web controller — no cloud required. Built with PlatformIO (Arduino framework).

## Build & Flash Commands

```bash
# Build firmware
pio run

# Build and upload to ESP32-C3
pio run --target upload

# Upload LittleFS filesystem (web assets)
pio run --target uploadfs

# Serial monitor
pio device monitor

# Combined: build, upload, and monitor
pio run --target upload && pio device monitor
```

## Architecture

### State Machine (core pattern)

`AppStateMachine` owns all service and state instances. It implements `IAppContext`, which states use to access services without circular dependencies. State transitions happen via `static_cast<AppStateMachine*>(_ctx)->transitionTo(StateId)`.

**State flow:**
```
BOOT → MODE_SELECT ─┬─→ SERIAL_IDLE ⇄ SERIAL_WORKING
                     └─→ PROVISIONING → LAN_IDLE ⇄ LAN_WORKING
                                              ↑         │
                                              └── RESET ← (5s boot button hold)
```

- **MODE_SELECT**: 3-second window — if USB serial is active, enters SERIAL mode; otherwise LAN mode
- **PROVISIONING**: WiFi AP + web setup, transitions to LAN_IDLE on connection
- **LAN_IDLE / SERIAL_IDLE**: Expression mode (eyes), transitions to WORKING when Codex status becomes active
- **LAN_WORKING / SERIAL_WORKING**: Info mode (Codex status panel), transitions back to IDLE after 10s settled (DONE/ERROR) or immediately on IDLE/SLEEPING

### Service Layer

All services are owned by `AppStateMachine` and accessed via `IAppContext`:

| Service | Role |
|---------|------|
| `TftDisplay` | Hardware abstraction over Adafruit_ST7789 (SPI on GPIO 8/10) |
| `DisplayService` | View rendering — eyes, Codex panel, clock, pomodoro, canvas, terminal |
| `ClaudeCodeService` | Receives CC status via UDP (port 4210) or serial; manages Status enum state machine |
| `WebService` | HTTP server (port 80) — serves LittleFS web assets, REST API for control |
| `WifiConfigService` | AP/STA management, credential storage in LittleFS (`/wifi.json`) |
| `TimeService` | NTP sync, time/date queries |
| `SerialCommandService` | CLI over USB serial (`cc`, `help`, `status`, `time`, `logs`, `reset`) |
| `OperationModeService` | Global singleton — LAN vs SERIAL mode detection |
| `BootButtonService` | 5-second hold on GPIO9 (BOOT button) triggers factory reset |
| `PreferenceService` | NVS-backed settings: bg color, anim speed, startup view, night dimming |

### View Layer

| View | Purpose |
|------|---------|
| `BootAnimation` | Static `run()` method — logo reveal on startup |
| `EyesView` | Animated pixel-art eyes with blink/look cycles |
| `ClaudeCodeView` | Status panel with shell-like layout, dirty-checking to avoid redraws |

### Display Modes & Interactive Views

`DisplayService` has two top-level modes: `EXPRESSION` (eyes) and `INFO` (Codex panel). When in `INTERACTIVE` mode (triggered by web controller), it can show: normal eyes, squish eyes, code view, canvas draw, thinking, working, clock, or pomodoro.

### Codex Hook

`scripts/cc_hook.py` is a Python script installed into Codex's settings as a hook. It sends session events to Clawd Mochi over serial (preferred) or UDP (fallback). Install with `scripts/install_claude_hook.sh`.

## Key Conventions

- **Language**: Code comments and log messages are in Chinese; UI strings are in English
- **Two StateMachine classes**: `StateMachine` (in `utils/`) is a generic callback-based framework used by `ClaudeCodeService`. `AppStateMachine` (in `states/`) is the app-level state machine with typed State classes
- **Service init timing**: In LAN mode, services init in `ProvisioningState::onEnter()`. In SERIAL mode, they init in `SerialIdleState::onEnter()`. Both use `s_*Initialized` static flags to prevent double init
- **Display anti-flicker**: `DisplayService` and `ClaudeCodeView` use dirty-checking (compare previous values) to skip redundant redraws. Time views use `_timeViewDirty` / `_timeViewLayoutDrawn` flags
- **RGB565 colors**: All display colors are RGB565 uint16_t. Use `hexToRgb565()` for web hex conversion
- **LittleFS**: Web assets in `data/` are uploaded separately from firmware via `pio run --target uploadfs`
- **Global singletons**: `OperationModeService` and `WifiConfigService` use `bind()/current()` pattern for cross-service access without passing through IAppContext

## Config Headers

All hardware pins, timeouts, buffer sizes, and UI constants are in `src/config/`:
- `cfg_display.h` — TFT pins, SPI freq, screen size, color constants, font sizes, terminal layout
- `cfg_wifi.h` — AP credentials, timeouts, web port
- `cfg_claude_code.h` — UDP ports, field lengths, timeouts
- `app_config.h` — loop intervals, debug flag

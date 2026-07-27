# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Clawd Mochi is an ESP32-C3 desk companion that displays animated expressions and Claude Code status on a 1.54" ST7789 TFT (240x240). It hosts a WiFi AP + web controller — no cloud required. Built with PlatformIO (Arduino framework).

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

# Positive Web UI regression (stable mocked market directory)
uv run scripts/test_web_ui.py

# Positive Web UI regression against the live CoinLore directory
uv run scripts/test_web_ui.py --live-directory
```

After changing the embedded Web controller, run the mocked positive UI
regression. When changing Crypto search or its directory integration, run both
the mocked and live-directory variants. These tests intentionally cover the
happy path only.

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
- **LAN_IDLE / SERIAL_IDLE**: Expression mode (eyes), transitions to WORKING when Claude Code status becomes active
- **LAN_WORKING / SERIAL_WORKING**: Info mode (Claude Code status panel), transitions back to IDLE after 10s settled (DONE/ERROR) or immediately on IDLE/SLEEPING

### Service Layer

All services are owned by `AppStateMachine` and accessed via `IAppContext`:

| Service | Role |
|---------|------|
| `TftDisplay` | Hardware abstraction over Adafruit_ST7789 (SPI on GPIO 8/10), backlight/brightness control. Lives in `src/hardware/` |
| `DisplayService` | View rendering — eyes, Claude Code panel, clock, pomodoro, canvas, terminal, weather/crypto/market carousel |
| `ClaudeCodeService` | Receives CC status via UDP (port 4210) or serial; manages `Status` enum state machine (IDLE/THINKING/WORKING/ERROR/DONE/PERMISSION/SWEEPING/SLEEPING) using the generic `utils/StateMachine` |
| `WebService` | HTTP server (port 80) — serves LittleFS web assets, REST API for control (prefs, state, backlight, canvas, crypto/market config+refresh+search, ccStatus) |
| `WifiConfigService` | AP/STA management, credential storage in LittleFS (`/wifi.json`). Global singleton (`bind()/current()`); mDNS `clawd-mochi`; AP SSID `ClaWD-Mochi` / pw `clawd1234` |
| `TimeService` | NTP sync, time/date/timestamp queries, `timestampCallback` for Logger |
| `SerialCommandService` | CLI over USB serial (`cc`, `help`, `status`, `IP`, `time`, `sync`, `logs`, `log-level`, `reset`, `restart`) |
| `OperationModeService` | Global singleton (`bind()/current()`) — LAN vs SERIAL mode detection (3s startup window) |
| `BootButtonService` | 5-second hold on GPIO9 (BOOT button) triggers factory reset |
| `PreferenceService` | NVS-backed settings: bg color, anim speed, startup view, brightness, claude status enabled, carousel config (enabled/speed/order/fixedView), night dimming |
| `WeatherService` | IP-located weather (IPWhois + Open-Meteo). 30-min refresh, 24h location refresh. Uses `NetworkRequestGate` |
| `CryptoService` | Cryptocurrency assets (max 5, CoinLore source). 10-min refresh, 2-min retry. Persists via `Preferences`. Uses `NetworkRequestGate` |
| `MarketService` | Stock market assets (max 5, Tencent quote source). 5-min refresh, 2-min retry. Uses `NetworkRequestGate` |

Note: `WeatherService`, `CryptoService`, and `MarketService` are owned by `AppStateMachine` as private members and wired directly into `DisplayService` and `WebService` — they are **not** exposed through `IAppContext`.

### NetworkRequestGate (`src/utils/network_request_gate`)

A static concurrency gate (FreeRTOS binary semaphore, lazily created) with `tryAcquire()`/`release()`. `WeatherService`, `CryptoService`, and `MarketService` all call `tryAcquire()` at the start of their background refresh task and `release()` when done — guaranteeing only one background TLS/HTTPS request runs at a time so the ESP32-C3 main loop is never starved. When adding another network-fetching service, gate it the same way.

### View Layer

| View | Purpose |
|------|---------|
| `BootAnimation` | Namespace with static `run()` — logo reveal on startup |
| `EyesView` | Animated pixel-art eyes with blink/look cycles, dirty-erasure of prior frame |
| `ClaudeCodeView` | Status panel with shell-like layout, dirty-checking to avoid redraws |

### Display Modes & Interactive Views

`DisplayService` has top-level `DisplayMode` values: `SETUP`, `EXPRESSION` (eyes), `INFO` (Claude Code panel), `PROVISIONING`, `INTERACTIVE`. In INTERACTIVE mode (triggered by web controller) it can show any `InteractiveView`: `EYES_NORMAL`, `EYES_SQUISH`, `CODE_VIEW`, `DRAW`, `THINKING`, `WORKING`, `CLOCK`, `POMODORO`, `WEATHER`, `CRYPTO`, `MARKET`. View indices are `VIEW_*` constants in `cfg_display.h`.

### Claude Code Hook

`scripts/cc_hook.py` is a Python script installed into Claude Code's settings as a hook. It broadcasts session events to all discovered Clawd Mochi devices over UDP port 4210 (LAN-only — the serial path was removed to avoid the ESP32-C3 USB-CDC reset on Windows). `scripts/cc_serial_daemon.py` is an optional Windows daemon that forwards local UDP to serial for users who still want the serial path. Install the hook with `scripts/install_claude_hook.sh` (or `.bat`/`.py`).

## Key Conventions

- **Language**: Code comments and log messages are in Chinese; UI strings are in English
- **Two StateMachine classes**: `StateMachine` (in `utils/`) is a generic callback-based framework used by `ClaudeCodeService`. `AppStateMachine` (in `states/`) is the app-level state machine with typed State classes
- **Service init timing**: In LAN mode, services init in `ProvisioningState::onEnter()`. In SERIAL mode, they init in `SerialIdleState::onEnter()`. Both use `s_*Initialized` static flags to prevent double init
- **Display anti-flicker**: `DisplayService` and `ClaudeCodeView` use dirty-checking (compare previous values) to skip redundant redraws. Time views use `_timeViewDirty` / `_timeViewLayoutDrawn` flags
- **RGB565 colors**: All display colors are RGB565 uint16_t. Use `hexToRgb565()` for web hex conversion
- **LittleFS**: Web assets in `data/` are uploaded separately from firmware via `pio run --target uploadfs`
- **Global singletons**: `OperationModeService` and `WifiConfigService` use `bind()/current()` pattern for cross-service access without passing through `IAppContext`
- **Background network requests**: Any HTTPS fetch from a service must go through `NetworkRequestGate` (see above) to keep the main loop responsive

## Config Headers

All hardware pins, timeouts, buffer sizes, and UI constants are in `src/config/`:
- `cfg_display.h` — TFT pins, SPI freq, screen size, RGB565 color constants, font sizes, terminal layout, `VIEW_*` interactive view indices
- `cfg_wifi.h` — AP credentials, timeouts, web port, mDNS hostname
- `cfg_claude_code.h` — UDP/discovery ports, field lengths, timeouts, `Status` enum
- `app_config.h` — app name/version, loop intervals, debug flag

## Scripts

| Script | Purpose |
|---|---|
| `scripts/cc_hook.py` | Claude Code hook — broadcasts status events to discovered devices over UDP 4210; caches devices in `cc_hook_cache.json` (24h TTL) |
| `scripts/cc_serial_daemon.py` | Optional Windows daemon forwarding local UDP to serial (avoids COM-close hardware reset) |
| `scripts/install_claude_hook.{py,sh,bat}` | Cross-platform hook installer writing `~/.claude/settings.json` |
| `scripts/preview_expressions_udp.py` | Sends a status sequence over UDP 4210 to preview expressions |
| `scripts/test_web_ui.py` | Playwright positive UI regression test for Crypto/Market config flows (mocked + `--live-directory` variants) |

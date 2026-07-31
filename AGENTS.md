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
- **Display anti-flicker is mandatory**: dynamic content must never be updated
  by clearing and redrawing an entire screen, panel, row, or text block on
  every tick. Keep the static layout drawn once, cache the last rendered
  value, and redraw only changed characters or the smallest dirty rectangle.
  Use opaque text backgrounds for in-place glyph replacement, fixed character
  positions for counters/timers, and a stable bounded refresh cadence. A full
  region clear is allowed only when the layout, font size, or text width
  actually changes. Fast counters should use sub-second value interpolation
  plus per-character dirty updates; timers should update only the digits that
  changed. `DisplayService` and `ClaudeCodeView` already use dirty-checking,
  and time views use `_timeViewDirty` / `_timeViewLayoutDrawn` flags. Any new
  animated or periodically updated screen must be checked on the physical
  ST7789 for visible flashing before it is considered complete.
- **RGB565 colors**: All display colors are RGB565 uint16_t. Use `hexToRgb565()` for web hex conversion
- **LittleFS**: Web assets in `data/` are uploaded separately from firmware via `pio run --target uploadfs`
- **Global singletons**: `OperationModeService` and `WifiConfigService` use `bind()/current()` pattern for cross-service access without passing through IAppContext

## RAM Budget (ESP32-C3)

RAM is a hard product constraint. Flash partition changes do not increase RAM.

- **Lazy loading is mandatory**: module runtime state, game objects, render
  buffers, scratch buffers, and other large resources must be allocated only
  when the user enters that module. Do not allocate them at boot or keep them
  as value members of global services. Immutable code/assets may remain in
  Flash/PROGMEM because they do not consume runtime heap.
- **Immediate release is mandatory**: when the user exits or switches away
  from a module, stop it and immediately `delete`/free all module-owned
  objects, render buffers, scratch buffers, and temporary state. Clear owning
  pointers after release. Do not retain memory “for faster reopening.”
- Never allocate a 240x240 RGB565 framebuffer: it costs 115,200 bytes and is
  unsafe even when allocated lazily because WiFi heap fragmentation may prevent
  obtaining one contiguous block.
- Full-color games must use the 16-row `ArcadeCanvas` strip buffer
  (7,680 bytes), created on game entry and destroyed on game exit.
- Mutually exclusive views must reuse one scratch allocation. Dino and Sokoban
  each request the same 7,200-byte 1-bit buffer shape, so only the active game
  receives one lazily allocated buffer.
- Keep `AppStateMachine` at or below the 32 KB compile-time budget and each
  render buffer at or below 12 KB. Do not relax these assertions to make a
  build pass; redesign ownership or rendering instead.
- Account for dynamic headroom, not only PlatformIO's static RAM percentage.
  WiFi, WebServer, mDNS, task stacks, JSON, and TLS all allocate from the heap.
- HTTPS work requires at least 80 KB free heap and a 64 KB largest contiguous
  block. Use `MemoryMonitor` for new network services.
- Pause nonessential HTTPS refreshes while a game is active. Resume through
  normal retry/refresh scheduling after the game exits.
- After memory-affecting changes, run `pio run` and report the before/after
  static RAM bytes and percentage.

### 2026-07-30 memory incident (do not repeat)

The first multi-game implementation placed a `uint16_t[240 * 240]` RGB565
canvas inside `DisplayService`. Because `DisplayService` is owned by the global
`AppStateMachine`, that canvas permanently consumed 115,200 bytes from boot,
even outside the game module. Dino and Sokoban also held separate 7,200-byte
1-bit buffers.

Static RAM changed from 45,372 bytes (13.8%) to 177,788 bytes (54.3%). Once
WiFi, WebServer, mDNS, and the weather task stack were active, mbedTLS could
not obtain a large contiguous allocation. The first request to
`https://ipwho.is/` failed with `MBEDTLS_ERR_SSL_ALLOC_FAILED`
(`-0x7F00`, logged as `-32512`).

The corrective architecture is:

- keep game code and immutable sprites/levels in Flash;
- allocate only the selected game object on entry;
- allocate either one 7,680-byte RGB565 strip buffer or one 7,200-byte 1-bit
  buffer for that active game;
- render full resolution/full color through strips rather than lowering game
  fidelity;
- destroy the game first, then its canvas/scratch buffer, immediately on exit
  or game switch;
- pause Weather/Crypto/Market HTTPS refresh while a game is active, then let
  normal scheduling resume after exit;
- log heap snapshots on boot, game load, and game release;
- reject/delay TLS when free 8-bit heap is below 80 KB or the largest
  contiguous block is below 64 KB.

After the strip-rendering change, static RAM fell to 63,084 bytes (19.3%).
After game objects and buffers were made lazy, boot-time static RAM returned to
45,452 bytes (13.9%). A successful link alone is not sufficient validation;
network/TLS behavior and allocation/release cycles must also be tested.

Interpret network errors before changing memory architecture:

- `-32512` / `-0x7F00` is `MBEDTLS_ERR_SSL_ALLOC_FAILED` and indicates an
  allocation failure.
- `start_ssl_client: -1` together with `HTTP=-1` is the generic TCP/TLS
  connection failure/timeout path; it is not evidence of low memory. Check the
  logged free heap and largest block before drawing conclusions.
- External TLS handshakes can fail transiently even with ample heap. Preserve
  HTTPS, use bounded connection/handshake timeouts, and retry with backoff.
  Never downgrade authenticated or sensitive traffic to HTTP to hide a
  transient network failure.

## Config Headers

All hardware pins, timeouts, buffer sizes, and UI constants are in `src/config/`:
- `cfg_display.h` — TFT pins, SPI freq, screen size, color constants, font sizes, terminal layout
- `cfg_wifi.h` — AP credentials, timeouts, web port
- `cfg_claude_code.h` — UDP ports, field lengths, timeouts
- `app_config.h` — loop intervals, debug flag

# AGENTS.md

This file provides guidance to Codex (Codex.ai/code) when working with code in this repository.

## Project Overview

Clawd Mochi is an ESP32-C3 desk companion that displays animated expressions and Claude Code status on a 1.54" ST7789 TFT (240x240). It hosts a WiFi AP + web controller — no cloud required. Built with PlatformIO (Arduino framework).

## Implemented Features (as of 2026-08-03)

A snapshot of what the firmware already does, so a new session does not have to re-scan the tree. Keep this in sync when a feature ships; see the relevant section below for architecture details.

**Services** (`src/service/`, all have real `.cpp` implementations — no stubs): `DisplayService` (master renderer), `WebService` (HTTP + ~60 REST routes), `WifiConfigService`, `MarketService` (stocks, Tencent), `PreferenceService` (NVS), `OtaService`, `ClaudeCodeService` (UDP 4210 status, 4211 discovery; also accumulates session/today WORKING stats + DONE/ERROR/PERMISSION counts in `mochi-ccstats` NVS, exposed via `/cc/stats`), `CryptoService` (CoinLore), `SalaryCounterService` (earn-while-working), `HolidayService`, `TimetableService` (lazy, LittleFS JSON), `SerialCommandService`, `WeatherService` (IP-located), `TimeService` (NTP), `OperationModeService`, `BootButtonService`, `DesktopStreamService` (PC desktop casting — lazy WiFiServer:3333, `ESPF`+len+JPEG frames decoded via TJpg_Decoder to 240x240; buffer/server allocated on view entry, fully released on exit).

**Display modes / views**: `DisplayMode` = SETUP / EXPRESSION (eyes) / INFO (Claude Code panel) / PROVISIONING / INTERACTIVE (web-controlled). `InteractiveView` (22 screens, `VIEW_*` in `cfg_display.h`): EYES_NORMAL, EYES_SQUISH, CODE_VIEW, DRAW (canvas), THINKING, WORKING, CLOCK, POMODORO, WEATHER, CRYPTO, MARKET, SALARY_COUNTER, TIMETABLE, MEDIA (image/GIF casting), DESKTOP_STREAM (PC screen casting, `VIEW_DESKTOP_STREAM` 21), STATS (Claude Code session/today focus panel), plus 6 arcade games (DINO, SOKOBAN, TETRIS, SNAKE, GAME_2048, BREAKOUT). Idle carousel cycles 5 info views.

**Expressions** (`expression_id.h`): 8 faces (NORMAL, HAPPY, THINKING, SLEEPING, CURIOUS, SURPRISED, GRUMPY, LOVE) with MANUAL/AUTO modes.

**Themes**: 5 (orange-black, orange-white, dark-orange, mint, pink) + brightness + night dimming.

**Web controller** (`data/`): `index.html` (Chinese status page), `controller.html` (full English touch controller — display/brightness/expression/theme/font/profile/OTA/WiFi/arcade/media/timetable panels), `wifi_setup.html`, `logs.html`. JS: `app.js`, `claude_code.js`, `media.js`, `gif_encoder.js`, `gif_reader.js`, `wakeup_import.js`, `wifi.js`.

**Games**: 6 under `src/view/`, all `IArcadeGame`, full-color via 16-row `ArcadeCanvas` strip buffer; Dino/Sokoban 1-bit via shared `GameRenderBuffer`. Dino + Sokoban have dedicated endpoints; the other four use the generic arcade API.

**Desktop app** (`desktop_app/`): Electron + React 18 + Vite + TypeScript cross-platform (macOS/Windows) PC companion console — UDP 4211 discovery, screen capture (mouse-follow crop / full-screen / fixed region), sharp JPEG encoding, TCP 3333 `ESPF` framing, tray, reconnect. i18n via JSON locale bundles + React context (`i18n/`, zh/en; add a bundle and register it in `I18nContext.tsx` to add a language). Theming via CSS custom properties driven by `<html data-theme>` (`theme/ThemeContext.tsx`): `system` (follows `prefers-color-scheme`) plus 5 device-matching palettes — orange-black(dark), orange-white(light), dark-orange, mint, pink. Both persisted in localStorage. `desktop_app/design/` holds the approved static HTML design prototype. Packaged with electron-builder (`npm run pack:mac` / `pack:win`); not signed. Subject to the **Feature Parity Rule** below.

**OTA**: full pipeline — remote manifest check (daily 03:30), version compare, firmware + LittleFS download with SHA256 verify, manual upload. `cfg_ota.h` notes `CFG_OTA_ROOT_CA` is empty (not pinned) and `CFG_OTA_MANIFEST_URL` is still the `example.com` placeholder.

**Hardware in use**: ST7789 SPI (MOSI=10, SCK=8, CS=4, DC=1, RST=2, BL=3, 40 MHz) with PWM backlight; BOOT button GPIO9 (5s hold = factory reset only); WiFi; UDP. **Unused**: GPIO0/5/6/7/20/21, I2C, ADC, extra PWM — but the project direction is software-only, so new features should not assume extra hardware.

**Forward-looking markers**: `operation_mode_service.h` lists unimplemented future modes BLE / USB_CDC_ONLY / AP_ONLY. No `TODO`/`FIXME` markers exist in `src/`.

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

# OTA end-to-end suite (builds isolated variants, flashes, tests, restores)
uv run scripts/ota_test.py \
  --device-url http://<device-ip>/ \
  --advertise-host <host-ip> \
  --serial-port /dev/cu.usbmodem...
```

The OTA runner creates disposable PlatformIO project copies under the system
temporary directory. It injects the test manifest URL, test versions, and
failed-boot variant only into those copies. It must restore production firmware
and LittleFS, verify the version read from the production configuration, and
delete all temporary projects even when a case fails.

After changing the embedded Web controller, run the mocked positive UI
regression. When changing Crypto search or its directory integration, run both
the mocked and live-directory variants. These tests intentionally cover the
happy path only.

## Test organization

- `scripts/web_ui_test/cases/` contains browser-only feature cases and is
  orchestrated by `scripts/test_web_ui.py`. New feature cases live in their own
  module (e.g. `cases/cc_stats.py` for the Claude Code session stats panel).
- `scripts/ota_test/` contains OTA transport, release-server, upload, interrupt,
  checksum, and rollback cases. It must not be added to `web_ui_test`.
- `scripts/ota_test.py` is the OTA end-to-end entry point. It requires a real
  ESP32 connected over USB and a host IP reachable from the device. The runner
  builds and flashes its own isolated baseline before executing any cases.
- OTA tests build three disposable variants: baseline, valid update, and
  boot-failure injection. Test environments must not be added to the tracked
  `platformio.ini` or production configuration headers.
- A successful OTA test must include remote upgrade, checksum rejection,
  interrupted local upload, failed-boot rollback, and offline upload. Do not
  report OTA as complete from a build-only or mocked HTTP test.
- Restoration and cleanup belong to the runner's `finally` path. A test run is
  incomplete unless the device reports the production version afterward and
  the temporary build root has been removed.

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
| `OtaService` | HTTP OTA firmware update — daily remote-manifest check, remote install, local upload streaming, checksum verify, and boot-rollback. Driven via `WebService` `/ota/*` endpoints |

Note: `WeatherService`, `CryptoService`, `MarketService`, and `OtaService` are owned by `AppStateMachine` as private members and wired directly into `DisplayService` and `WebService` — they are **not** exposed through `IAppContext`.

### NetworkRequestGate (`src/utils/network_request_gate`)

A static concurrency gate (FreeRTOS binary semaphore, lazily created) with `tryAcquire()`/`release()`. `WeatherService`, `CryptoService`, and `MarketService` all call `tryAcquire()` at the start of their background refresh task and `release()` when done — guaranteeing only one background TLS/HTTPS request runs at a time so the ESP32-C3 main loop is never starved. When adding another network-fetching service, gate it the same way.

### View Layer

| View | Purpose |
|------|---------|
| `BootAnimation` | Namespace with static `run()` — logo reveal on startup |
| `EyesView` | Animated pixel-art eyes with blink/look cycles, dirty-erasure of prior frame |
| `ClaudeCodeView` | Status panel with shell-like layout, dirty-checking to avoid redraws |

### Games & ArcadeCanvas (`src/view/`)

Six arcade games (`dino`, `sokoban`, `tetris`, `snake`, `game_2048`, `breakout`) share a common pattern: immutable sprites/levels live in Flash (`*_sprites.h` / `sokoban_levels.h`); only the active game object and its render buffer are allocated on entry. Full-color games render through the 16-row `ArcadeCanvas` strip buffer (`arcade_canvas.{h,cpp}`, 7,680 bytes) rather than a full 240x240 framebuffer. 1-bit games (Dino, Sokoban) share one 7,200-byte `GameRenderBuffer`. See **RAM Budget** below — games are the reason those rules exist; do not introduce a per-game framebuffer or keep game state on a global service.

### Display Modes & Interactive Views

`DisplayService` has top-level `DisplayMode` values: `SETUP`, `EXPRESSION` (eyes), `INFO` (Claude Code panel), `PROVISIONING`, `INTERACTIVE`. In INTERACTIVE mode (triggered by web controller) it can show any `InteractiveView`: `EYES_NORMAL`, `EYES_SQUISH`, `CODE_VIEW`, `DRAW`, `THINKING`, `WORKING`, `CLOCK`, `POMODORO`, `WEATHER`, `CRYPTO`, `MARKET`, plus the games `DINO`, `SOKOBAN`, `TETRIS`, `SNAKE`, `BREAKOUT` (and `GAME_2048`), and the info panels `SALARY`, `TIMETABLE`, `MEDIA`. View indices are `VIEW_*` constants in `cfg_display.h`.

### Claude Code Hook

`scripts/cc_hook.py` is a Python script installed into Claude Code's settings as a hook. It broadcasts session events to all discovered Clawd Mochi devices over UDP port 4210 (LAN-only — the serial path was removed to avoid the ESP32-C3 USB-CDC reset on Windows). `scripts/cc_serial_daemon.py` is an optional Windows daemon that forwards local UDP to serial for users who still want the serial path. Install the hook with `scripts/install_claude_hook.sh` (or `.bat`/`.py`).

## Feature Parity Rule (web controller ⇄ desktop app)

The Electron desktop console (`desktop_app/`) is the PC-side counterpart of the embedded web controller (`data/controller.html`). **Any new device-control feature added to the web controller must also be added to the desktop app, and vice versa** — the two are user-facing surfaces of the same device API.

When adding such a feature, do all of the following in the same change:

- expose/extend the firmware REST endpoint and use it from both clients;
- desktop app: add the page module under `desktop_app/src/renderer/src/pages/`, register it in `lib/nav.ts` (navigation groups mirror the firmware `InteractiveView` enum), and extend `lib/DeviceClient.ts` instead of ad-hoc `fetch` calls;
- add the new UI strings to **all** locale bundles (`i18n/zh.ts`, `i18n/en.ts`) — never hardcode UI text in components; the key set must stay identical across bundles (type-checked via `Messages`);
- keep the desktop UI theme-safe: reference only the CSS custom properties defined in `styles.css` (no hardcoded hex colors), so the feature renders correctly in every `data-theme`;
- run the web UI regression (`uv run scripts/test_web_ui.py`) plus the desktop `npm run typecheck && npm run build`.

The only intentional exceptions are platform-exclusive capabilities (e.g. screen capture / tray in the desktop app, LittleFS-served pages in the web controller).

## Key Conventions

- **Language**: Code comments and log messages are in Chinese; UI strings are in English
- **Two StateMachine classes**: `StateMachine` (in `utils/`) is a generic callback-based framework used by `ClaudeCodeService`. `AppStateMachine` (in `states/`) is the app-level state machine with typed State classes
- **Service init timing**: In LAN mode, services init in `ProvisioningState::onEnter()`. In SERIAL mode, they init in `SerialIdleState::onEnter()`. Both use `s_*Initialized` static flags to prevent double init
- **Display anti-flicker is mandatory**: dynamic content must never be updated by clearing and redrawing an entire screen, panel, row, or text block on every tick. Keep the static layout drawn once, cache the last rendered value, and redraw only changed characters or the smallest dirty rectangle. Use opaque text backgrounds for in-place glyph replacement, fixed character positions for counters/timers, and a stable bounded refresh cadence. A full region clear is allowed only when the layout, font size, or text width actually changes. Fast counters should use sub-second value interpolation plus per-character dirty updates; timers should update only the digits that changed. `DisplayService` and `ClaudeCodeView` already use dirty-checking (compare previous values) to skip redundant redraws, and time views use `_timeViewDirty` / `_timeViewLayoutDrawn` flags. Any new animated or periodically updated screen must be checked on the physical ST7789 for visible flashing before it is considered complete.
- **RGB565 colors**: All display colors are RGB565 uint16_t. Use `hexToRgb565()` for web hex conversion
- **Device color compensation is not a Web color**: the firmware default background `#da1100` is the RGB representation of the boot animation's `color565(218, 17, 0)` and is used on the physical ST7789. Browser previews do not have the panel color cast and must not reuse the firmware background; Crypto, Market, and future Web previews must use the shared design-target `--preview-bg` (`#fb6b10`) so their perceived color matches each other and the design. Hardware color calibration belongs only in the firmware/device preference layer; keep Web preview colors stable.
- **All device UI designs and Web previews must be pixel-accurate**: every UI design mockup, review image, and browser-based device preview must reproduce the intended ESP32 output at the native 240x240 coordinate grid. Use the exact firmware font bitmap data, glyph metrics, baselines, text sizes, wrapping rules, coordinates, spacing, strokes, and assets. Browser/system substitute fonts or visually similar CSS fonts are not acceptable for device previews. Device rendering and Web preview rendering must consume the same source-of-truth layout and typography definitions, or generated artifacts derived from those definitions. A UI change is incomplete until the design reference, Web preview, and headless/device render agree by pixel comparison; dynamic screens must additionally be checked on the physical ST7789. The only permitted non-pixel-identical difference is documented hardware color and brightness compensation as described above.
- **LittleFS**: Web assets in `data/` are uploaded separately from firmware via `pio run --target uploadfs`
- **Global singletons**: `OperationModeService` and `WifiConfigService` use `bind()/current()` pattern for cross-service access without passing through `IAppContext`
- **Background network requests**: Any HTTPS fetch from a service must go through `NetworkRequestGate` (see above) to keep the main loop responsive

## RAM Budget (ESP32-C3)

RAM is a hard product constraint. Flash partition changes do not increase RAM.

- **Lazy loading is mandatory**: module runtime state, game objects, render buffers, scratch buffers, and other large resources must be allocated only when the user enters that module. Do not allocate them at boot or keep them as value members of global services. Immutable code/assets may remain in Flash/PROGMEM because they do not consume runtime heap.
- **Immediate release is mandatory**: when the user exits or switches away from a module, stop it and immediately `delete`/free all module-owned objects, render buffers, scratch buffers, and temporary state. Clear owning pointers after release. Do not retain memory "for faster reopening."
- Never allocate a 240x240 RGB565 framebuffer: it costs 115,200 bytes and is unsafe even when allocated lazily because WiFi heap fragmentation may prevent obtaining one contiguous block.
- Full-color games must use the 16-row `ArcadeCanvas` strip buffer (7,680 bytes), created on game entry and destroyed on game exit.
- Mutually exclusive views must reuse one scratch allocation. Dino and Sokoban each request the same 7,200-byte 1-bit buffer shape, so only the active game receives one lazily allocated buffer.
- Keep `AppStateMachine` at or below the 32 KB compile-time budget and each render buffer at or below 12 KB. Do not relax these assertions to make a build pass; redesign ownership or rendering instead.
- Account for dynamic headroom, not only PlatformIO's static RAM percentage. WiFi, WebServer, mDNS, task stacks, JSON, and TLS all allocate from the heap.
- HTTPS work requires at least 80 KB free heap and a 64 KB largest contiguous block. Use `MemoryMonitor` for new network services.
- Pause nonessential HTTPS refreshes while a game is active. Resume through normal retry/refresh scheduling after the game exits.
- After memory-affecting changes, run `pio run` and report the before/after static RAM bytes and percentage.

### 2026-07-30 memory incident (do not repeat)

The first multi-game implementation placed a `uint16_t[240 * 240]` RGB565 canvas inside `DisplayService`. Because `DisplayService` is owned by the global `AppStateMachine`, that canvas permanently consumed 115,200 bytes from boot, even outside the game module. Dino and Sokoban also held separate 7,200-byte 1-bit buffers.

Static RAM changed from 45,372 bytes (13.8%) to 177,788 bytes (54.3%). Once WiFi, WebServer, mDNS, and the weather task stack were active, mbedTLS could not obtain a large contiguous allocation. The first request to `https://ipwho.is/` failed with `MBEDTLS_ERR_SSL_ALLOC_FAILED` (`-0x7F00`, logged as `-32512`).

The corrective architecture is:

- keep game code and immutable sprites/levels in Flash;
- allocate only the selected game object on entry;
- allocate either one 7,680-byte RGB565 strip buffer or one 7,200-byte 1-bit buffer for that active game;
- render full resolution/full color through strips rather than lowering game fidelity;
- destroy the game first, then its canvas/scratch buffer, immediately on exit or game switch;
- pause Weather/Crypto/Market HTTPS refresh while a game is active, then let normal scheduling resume after exit;
- log heap snapshots on boot, game load, and game release;
- reject/delay TLS when free 8-bit heap is below 80 KB or the largest contiguous block is below 64 KB.

After the strip-rendering change, static RAM fell to 63,084 bytes (19.3%). After game objects and buffers were made lazy, boot-time static RAM returned to 45,452 bytes (13.9%). A successful link alone is not sufficient validation; network/TLS behavior and allocation/release cycles must also be tested.

Interpret network errors before changing memory architecture:

- `-32512` / `-0x7F00` is `MBEDTLS_ERR_SSL_ALLOC_FAILED` and indicates an allocation failure.
- `start_ssl_client: -1` together with `HTTP=-1` is the generic TCP/TLS connection failure/timeout path; it is not evidence of low memory. Check the logged free heap and largest block before drawing conclusions.
- External TLS handshakes can fail transiently even with ample heap. Preserve HTTPS, use bounded connection/handshake timeouts, and retry with backoff. Never downgrade authenticated or sensitive traffic to HTTP to hide a transient network failure.

## Config Headers

All hardware pins, timeouts, buffer sizes, and UI constants are in `src/config/`:
- `cfg_display.h` — TFT pins, SPI freq, screen size, RGB565 color constants, font sizes, terminal layout, `VIEW_*` interactive view indices
- `cfg_wifi.h` — AP credentials, timeouts, web port, mDNS hostname
- `cfg_claude_code.h` — UDP/discovery ports, field lengths, timeouts, `Status` enum
- `cfg_ota.h` — OTA manifest URL, channel, daily-check window, version/size limits, checksum policy
- `app_config.h` — app name/version, loop intervals, debug flag

### Partition table

Firmware uses `ota_4mb.csv` (dual OTA slots: `app0`/`app1` + `otadata`), set via `board_build.partitions` in `platformio.ini`. OTA tests build their own disposable variants and must never add environments to the tracked `platformio.ini`.

## Scripts

| Script | Purpose |
|---|---|
| `scripts/cc_hook.py` | Claude Code hook — broadcasts status events to discovered devices over UDP 4210; caches devices in `cc_hook_cache.json` (24h TTL) |
| `scripts/cc_serial_daemon.py` | Optional Windows daemon forwarding local UDP to serial (avoids COM-close hardware reset) |
| `scripts/install_claude_hook.{py,sh,bat}` | Cross-platform hook installer writing `~/.claude/settings.json` |
| `scripts/preview_expressions_udp.py` | Sends a status sequence over UDP 4210 to preview expressions |
| `scripts/test_web_ui.py` | Playwright positive UI regression test for Crypto/Market config flows (mocked + `--live-directory` variants) |
| `scripts/ota_test.py` | OTA end-to-end runner — builds isolated variants, flashes, runs transport/rollback/upload cases, restores production firmware |
| `scripts/ota_test/` | OTA case modules (transport, release-server, upload, interrupt, checksum, rollback) consumed by `ota_test.py` |

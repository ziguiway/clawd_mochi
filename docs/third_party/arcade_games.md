# Arcade game references

The Clawd Mochi arcade engines are original ports for the project's
240x240 ST7789 display, HTTP controls, state machine, and full-frame RGB565
renderer. The following open-source projects were used as behavioral and
visual references.

## Tetris

- Project: `MhageGH/esp32_ST7735_Tetris`
- URL: https://github.com/MhageGH/esp32_ST7735_Tetris
- License: MIT
- Used as reference for the 10x20 embedded playfield, tetromino presentation,
  next-piece preview, and ESP32 remote-control flow.

## Snake

- Project: `poky/ESP32_eSPI_snake`
- URL: https://github.com/poky/ESP32_eSPI_snake
- License status: no license file was present when reviewed on 2026-07-30.
- Used as a visual and interaction reference for an ST7789-based ESP32 Snake
  game. No source file from this project is included verbatim.

## 2048

- Project: `mevdschee/2048.c`
- URL: https://github.com/mevdschee/2048.c
- License: MIT
- Used as the rules reference for compression, merge-once behavior, spawning
  a new 2/4 tile, win detection, and game-over detection. The upstream project
  includes 13 rule tests.

## Breakout

- Project: `hpsaturn/esp32-atari-game-breakout`
- URL: https://github.com/hpsaturn/esp32-atari-game-breakout
- License: GPL-3.0
- Used as a gameplay and product reference for ESP32 lives, levels, score
  persistence, and paddle control. The Clawd Mochi implementation uses an
  independently written RGB565 renderer and physics loop.


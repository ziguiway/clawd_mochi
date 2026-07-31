# Live Ledger Design QA

## Comparison Target

- Selected target: the original pixel-font Live Ledger implementation restored from the repository baseline.
- Reference capture: `/Users/zhengshuang/.codex/visualizations/2026/07/31/019fb61f-6073-7522-a376-ba48a92563a6/live-ledger-pixel-original-restored.png`
- Rendered implementation: `/tmp/clawd_mochi_live_ledger.png`
- Comparison: `/Users/zhengshuang/.codex/visualizations/2026/07/31/019fb61f-6073-7522-a376-ba48a92563a6/live-ledger-pixel-balanced-comparison.png`
- Viewport: 240 × 240 px.

## Findings

- No actionable P0, P1, or P2 differences remain.
- The custom FreeMono/FreeSans font experiment remains removed.
- The header now follows the existing CRYPTO / MARKET convention exactly:
  `FONT_MEDIUM` title at `(8, 8)`, `FONT_SMALL` status at `y=12`, and the
  full-width divider at `y=30`.
- The amount remains dominant; worked time is reduced to `FONT_SMALL`, the
  progress bar is widened to 220 px, and the schedule uses a quiet `~`
  separator.

## Runtime Checks

- The classic GFX font supports opaque foreground/background replacement, so changing digits overwrite in place without a clear-then-redraw blank frame.
- Only changed amount and timer characters are redrawn. Progress updates change only the added or removed strip.
- Adaptive precision remains as a non-visual safety fix: `9999.9999` becomes `10000.000`, with further decimal reduction as the integer grows.
- Running sessions remain active when NTP is not yet available after reboot.
- PlatformIO build passed: 45,724 bytes static RAM (14.0%), 1,231,058 bytes Flash (39.1%).
- Full mocked Web UI regression passed, including the precision boundary.
- Firmware and LittleFS uploads completed with hash verification.
- Post-upload live samples increased from 160,347 to 163,099
  ten-thousandths while state remained `running`.

## Checklist

- [x] Keep the original pixel-font UI.
- [x] Match the header typography and divider to CRYPTO / MARKET.
- [x] Rebalance the worked time, progress bar, and schedule footer.
- [x] Remove custom font code and assets.
- [x] Preserve adaptive precision and restart-safe counting.
- [x] Verify the 240 × 240 preview.
- [x] Build, test, upload, and confirm live amount growth.

final result: passed

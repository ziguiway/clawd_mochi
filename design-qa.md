# Live Ledger Design QA

## Comparison Target

- Source visual truth: `/Users/zhengshuang/.codex/generated_images/019fb61f-6073-7522-a376-ba48a92563a6/call_RwJro1kxjeyHXXCzuCR05aIg.png`
- Rendered implementation: `/tmp/clawd_mochi_live_ledger.png`
- Side-by-side evidence: `/Users/zhengshuang/.codex/visualizations/2026/07/31/019fb61f-6073-7522-a376-ba48a92563a6/live-ledger-comparison.png`
- State: running, approximately CNY 12.35 earned, 03:21:08 worked, rate CNY 0.0239/s. The implementation capture is slightly ahead of the static source because the amount now rolls continuously.
- CSS viewport: 240 × 240 px
- Browser capture density: deviceScaleFactor 1
- Source pixels: 1254 × 1254 px
- Implementation pixels: 240 × 240 px
- Normalization: both square content regions were resized to 480 × 480 px for the side-by-side comparison; the implementation used nearest-neighbor scaling to preserve its pixel-display character.

## Findings

- No actionable P0, P1, or P2 differences remain.
- [P3] The source uses a soft orange glow/gradient and a taller display face, while the implementation uses a flat orange field and the product's existing monospace/bitmap typography. This is an accepted hardware-oriented deviation: the ESP32 implementation uses RGB565 fills and the existing TFT bitmap font to preserve legibility and avoid unnecessary redraw cost.
- The removed `EARNED`, rate, and duplicate footer state are intentional changes from the source, made in response to the user's request for a quieter information hierarchy. The configured `09:30 > 19:00` shift replaces the redundant footer metadata.

## Fidelity Surfaces

- Fonts and typography: the amount remains the dominant element; top labels, earned label, worked time, and footer preserve the source hierarchy, optical weight, alignment, and single-line behavior. The exact source display face is treated as art direction rather than a bundled asset.
- Spacing and layout rhythm: the 240 × 240 frame, top divider, amount block, worked-time row, progress bar, and shift-time footer form a simpler vertical rhythm without clipping or overflow.
- Colors and visual tokens: the orange/white high-contrast palette is preserved. The flat RGB565-oriented orange is intentional; RUNNING remains a text state instead of introducing a new semantic color.
- Image quality and asset fidelity: the screen contains no logos, illustrations, icons, or photographic assets. All visible marks are native text, dividers, and the functional progress indicator, so no source asset was substituted.
- Copy and content: `CNY TODAY`, `LIVE`, `EARNED`, `WORKED`, `RATE`, and `RUNNING` match the selected design. The few ten-thousandths difference in the captured amount is expected evidence of the new continuous-motion behavior.

## Full-view Comparison Evidence

The final side-by-side image shows the same state and data at the same square aspect ratio. Amount prominence, horizontal alignment, information order, progress direction, and footer balance all match closely. No content is cropped and no persistent control is obscured.

## Focused-region Comparison

A separate crop was not needed: after normalization to 480 × 480 px, all display text, dividers, and the progress bar are clearly readable in the full-view comparison. There are no dense controls, icons, or image assets requiring a higher-magnification pass.

## Comparison History

1. Initial comparison
   - The implementation evidence showed a finished state with different values, so it was not a valid same-state fidelity comparison.
   - [P2] The web preview also added a two-pixel outer stroke absent from the screen design, changing the edge treatment.
2. Fixes
   - Captured the running state with the same amount, worked time, rate, and progress values as the source.
   - Removed the preview-only outer stroke.
3. Post-fix evidence
   - `/Users/zhengshuang/.codex/visualizations/2026/07/31/019fb61f-6073-7522-a376-ba48a92563a6/live-ledger-comparison.png`
   - No actionable P0/P1/P2 differences remain.
4. Information-simplification pass
   - Removed the redundant `EARNED`, rate, and repeated running-state footer.
   - Added the configured shift window as the only footer metadata.
   - Kept amount, worked time, and progress as the three primary live signals.
   - Changed worked-time updates from whole-row clearing to changed-character redraws.

## Interaction and Runtime Checks

- Tested salary and 09:30–19:00 schedule configuration, automatic-shift setting, start, sub-second continuous amount motion, pause, resume, clock-out, reset, locked settings while active, 240 × 240 preview sizing, and inclusion in the information carousel.
- The full mocked Web UI regression passed.
- Browser console was checked. The only reported 400 response is the existing deliberate invalid-import negative assertion; no unexpected Live Ledger console error occurred.
- PlatformIO firmware build passed.

## Implementation Checklist

- [x] Match the selected amount-first layout.
- [x] Keep the device render framebuffer-free.
- [x] Verify the complete working-day state flow.
- [x] Verify the fixed-size web preview.
- [x] Rebuild firmware and run the Web UI regression.

final result: passed

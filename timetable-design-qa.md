# Timetable Implementation Design QA

## Comparison target

- Device source visuals:
  - `docs/ui-concepts/timetable-next-class.svg`
  - `docs/ui-concepts/timetable-class-in-progress.svg`
  - `docs/ui-concepts/timetable-all-done.svg`
  - `docs/ui-concepts/timetable-no-class.svg`
- Controller implementation: `data/controller.html`
- Browser-rendered controller evidence: Codex in-app browser, local firmware
  stub at `http://127.0.0.1:8765/`.
- Browser viewport: 430 × 900 CSS px, density 1.
- Visible timetable panel: 390 px wide.
- State: imported GDUFS timetable with Machine Learning; then manual addition
  of Deep Learning.

## Full-view and focused evidence

The mobile-width full-page capture showed the timetable entry aligned to the
existing two-column Views grid. The focused viewport capture showed the full
management panel, term date, two course rows, import actions, and the
transition into the following Idle Display section without horizontal
overflow.

The device SVGs are 240 × 240. Firmware uses the same information order,
coordinates, orange/foreground theme, dividers, completion mark, empty
calendar mark, next-course footer, and minute countdown. A physical TFT
capture is not available in this run, so pixel-level font and optical
alignment against the hardware remain a physical acceptance check.

## Fidelity surfaces

- Fonts and typography: controller uses the product's existing Courier stack;
  firmware uses the existing Adafruit GFX bitmap font required by the device.
  Course names select a full or short English name by measured pixel width.
- Spacing and layout rhythm: controller remains within the existing 390 px
  column. The firmware retains the 14 px side margins and major y positions
  from the selected SVGs.
- Colors and visual tokens: controller reuses the existing dark/orange tokens;
  device rendering uses the current RGB565 theme colors.
- Image quality and assets: no raster assets are needed. The TFT completion
  and no-class marks are drawn with native primitives that the device can
  reproduce completely.
- Copy and content: Next Class, In Class, All Classes Complete, and No Classes
  Today match the approved English-only states. No emoji are present.

## Interactions checked

- Opened the Timetable view.
- Loaded an imported Machine Learning course.
- Opened the manual Add Class form.
- Entered `深度学习`; mapping produced `DEEP LEARNING`.
- Saved the course and verified it appeared with time, week, and room.
- Checked the browser console; no errors were reported.

## Findings

- No actionable P0, P1, or P2 issue was found in the controller.
- [P3] Physical ST7789 comparison remains advisable because browser capture
  cannot reproduce the bitmap font's panel-specific optical weight.

## Comparison history

1. Initial browser pass: the new panel rendered at 390 px without overflow.
2. Interaction pass: automatic name mapping and save produced a second course
   row; no console errors appeared.
3. Firmware review: switching back to the Timetable view was corrected to
   force one full layout draw, while minute-only countdown changes use a small
   dirty rectangle.

final result: passed

## Mobile App Import Flow Addendum

- Source truth: `docs/product/2026-07-31-timetable-import-interaction-flow.md`.
- Rendered implementation: `data/controller.html`, `IMPORT CLASSES` overlay.
- Browser evidence: Codex in-app browser at 430 × 900 CSS px, density 1.
- States checked: source selection, intelligent paste, ICS reading, mapped course
  preview, confirmation, and return to the timetable list.
- The overlay measured exactly 430 px wide with a 430 px document scroll width;
  no horizontal overflow or hidden persistent action was found.
- WakeUp is intentionally labeled as share-code/export input, but only exported
  files are enabled until a real desensitized share-code sample is verified.
- XiaoAi accepts only `i.ai.mi.com` links. ICS and normalized JSON are covered
  by automated positive regression.
- No new P0, P1, or P2 finding was found.

final result: passed

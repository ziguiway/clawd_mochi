# Design QA — Claude Code Mochi Expression System

## Evidence

- Source visual truth: `/Users/zhengshuang/Documents/PlatformIO/Projects/clawd_mochi/pics/clawd_mochi_avatar.jpg`
- Product constraints: the user's final state mapping in the current task.
- Implementation: `/Users/zhengshuang/Documents/PlatformIO/Projects/clawd_mochi/docs/ui-concepts/claude-code-mochi-expression-system.html`
- Browser-rendered implementation screenshot: `/Users/zhengshuang/.codex/visualizations/2026/07/15/019f6529-f3a8-79d3-8e93-5f0f109c04a3/claude-code-mochi-expression-system-desktop.jpg`
- Focused source/implementation comparison: `/Users/zhengshuang/.codex/visualizations/2026/07/15/019f6529-f3a8-79d3-8e93-5f0f109c04a3/mochi-expression-qa-comparison.png`
- Captured surface: 1425 × 990, desktop, IDLE selected.
- Primary interactions tested: selecting IDLE, THINKING, WORKING, PERMISSION, COMPACTING, and SLEEPING updated the hero title, expression, primitive list, GFX code, pressed state, and active atlas card.
- Pixel comparison: THINKING, WORKING, PERMISSION, and COMPACTING render identically to IDLE; SLEEPING uses two horizontal 42 × 10 rectangles.
- Console errors checked: none.

## Full-view comparison evidence

The independent design sheet renders as an editorial hardware-spec board with a literal 240 × 240 face canvas. The display region contains only orange and black. The page chrome is visually separate from the simulated display, so labels and implementation notes cannot be mistaken for content intended for the ST7789.

## Focused region comparison evidence

The focused side-by-side comparison confirms that IDLE preserves the source character's defining wide eye spacing, tall black geometric eyes, neutral expression, and absence of mouth, pupils, or eye whites. The source's soft rounded corners are intentionally converted to hard rectangular pixels to honor the Adafruit GFX constraint. The cream face panel from the source is intentionally replaced by the requested orange screen background.

## Findings

- No actionable P0, P1, or P2 fidelity issues found.
- Fonts and typography: the design-sheet typography is clear and does not enter the simulated display.
- Spacing and layout rhythm: the screen, engineering notes, and state controls remain distinctly grouped at the captured desktop viewport.
- Colors and tokens: the simulated screen uses only orange and black; no gradients or opacity are used inside the 240 × 240 canvas.
- Image quality and asset fidelity: the source avatar is used only as visual truth; no source asset is replaced by a fake visible page asset.
- Copy and content: all eight required states are present and named exactly as specified.
- Accessibility: state controls are semantic buttons with pressed state; canvases have labels; reduced-motion behavior is included.

## Comparison history

- Pass 1: no P0/P1/P2 issues identified; no visual fixes were required.
- Pass 2: applied the final simplified state mapping, compared the shared IDLE canvases byte-for-byte, verified the horizontal SLEEPING coordinates, and found no console issues.

## Implementation checklist

- [x] Eight states mapped to four intentional eye poses.
- [x] 240 × 240 integer-coordinate canvases.
- [x] Orange/black display palette only.
- [x] Rectangle and thick-line geometry only.
- [x] Pixel Z limited to SLEEPING.
- [x] Interactive state selector and GFX coordinate reference.

## Follow-up polish

- P3 test gap: the desktop surface and primary interaction were browser-verified; the later mobile screenshot pass was interrupted by the in-app preview connection. Responsive breakpoints are present but were only statically inspected.

final result: passed

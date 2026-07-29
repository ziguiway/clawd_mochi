#include "eyes_view.h"

#define EYE_W   30
#define EYE_H   60
#define EYE_GAP 120
#define EYE_OY  40

namespace {
constexpr unsigned long BLINK_MIN_INTERVAL_MS = 2500;
constexpr unsigned long BLINK_MAX_INTERVAL_MS = 6000;
constexpr unsigned long BLINK_DURATION_MS = 160;
constexpr unsigned long THINKING_FRAME_INTERVAL_MS = 280;

int16_t eyeLX(int16_t screenW) {
    return (screenW - (EYE_W * 2 + EYE_GAP)) / 2;
}

int16_t eyeRX(int16_t screenW) {
    return eyeLX(screenW) + EYE_W + EYE_GAP;
}

int16_t eyeY(int16_t screenH) {
    return (screenH - EYE_H) / 2 - EYE_OY;
}
}

EyesView::EyesView(TftDisplay* tft)
    : _tft(tft)
    , _backgroundColor(COLOR_ORANGE)
    , _expression(ExpressionId::NORMAL)
    , _nextBlinkMs(0)
    , _blinkStartedMs(0)
    , _lastThinkingFrameMs(0)
    , _isBlinking(false)
    , _thinkingFrame(2)
{
}

void EyesView::init() {
    _isBlinking = false;
    _thinkingFrame = 2;
    _lastThinkingFrameMs = millis();
    redraw();
    if (supportsBlink()) scheduleNextBlink(_lastThinkingFrameMs);
}

void EyesView::setExpression(ExpressionId expression) {
    _expression = expression;
    init();
}

void EyesView::redraw() {
    _tft->fillScreen(_backgroundColor);
    drawExpression();
}

void EyesView::update() {
    const unsigned long now = millis();
    if (_expression == ExpressionId::THINKING) {
        if (now - _lastThinkingFrameMs >= THINKING_FRAME_INTERVAL_MS) {
            _lastThinkingFrameMs = now;
            _thinkingFrame = (_thinkingFrame + 1) % 4;
            drawThinkingDots();
        }
        return;
    }

    if (!supportsBlink()) return;

    if (!_isBlinking && static_cast<long>(now - _nextBlinkMs) >= 0) {
        _isBlinking = true;
        _blinkStartedMs = now;
        redraw();
        return;
    }
    if (_isBlinking && now - _blinkStartedMs >= BLINK_DURATION_MS) {
        _isBlinking = false;
        redraw();
        scheduleNextBlink(now);
    }
}

void EyesView::drawExpression() {
    switch (_expression) {
        case ExpressionId::NORMAL:    drawNormal(_isBlinking); break;
        case ExpressionId::HAPPY:     drawHappy(); break;
        case ExpressionId::THINKING:  drawThinking(); break;
        case ExpressionId::SLEEPING:  drawSleeping(); break;
        case ExpressionId::CURIOUS:   drawCurious(); break;
        case ExpressionId::SURPRISED: drawSurprised(); break;
        case ExpressionId::GRUMPY:    drawGrumpy(); break;
        case ExpressionId::LOVE:      drawLove(); break;
    }
}

void EyesView::drawNormal(bool blinking) {
    const int16_t lx = eyeLX(_tft->getWidth());
    const int16_t rx = eyeRX(_tft->getWidth());
    const int16_t ey = eyeY(_tft->getHeight());
    if (blinking) {
        _tft->fillRect(lx, ey + EYE_H / 2 - 3, EYE_W, 6, COLOR_EYES);
        _tft->fillRect(rx, ey + EYE_H / 2 - 3, EYE_W, 6, COLOR_EYES);
        return;
    }
    _tft->fillRect(lx, ey, EYE_W, EYE_H, COLOR_EYES);
    _tft->fillRect(rx, ey, EYE_W, EYE_H, COLOR_EYES);
}

void EyesView::drawHappy() {
    const int16_t lx = eyeLX(_tft->getWidth()) + EYE_W / 2;
    const int16_t rx = eyeRX(_tft->getWidth()) + EYE_W / 2;
    const int16_t cy = eyeY(_tft->getHeight()) + EYE_H / 2;
    drawChevron(lx, cy, 25, 18, 5, true);
    drawChevron(rx, cy, 25, 18, 5, false);
}

void EyesView::drawThinking() {
    drawNormal(false);
    drawThinkingDots();
}

void EyesView::drawThinkingDots() {
    constexpr int16_t DOT_X = 194;
    constexpr int16_t DOT_Y = 14;
    constexpr int16_t DOT_SIZE = 8;
    constexpr int16_t DOT_STEP = 14;
    constexpr uint8_t VISIBLE_DOTS[] = {1, 2, 3, 2};

    // 只刷新右上角符号区域，避免动画导致整屏闪烁。
    _tft->fillRect(DOT_X - 2, DOT_Y - 2, 42, 12, _backgroundColor);
    const uint8_t count = VISIBLE_DOTS[_thinkingFrame];
    for (uint8_t index = 0; index < count; index++) {
        _tft->fillRect(DOT_X + index * DOT_STEP, DOT_Y,
                       DOT_SIZE, DOT_SIZE, COLOR_EYES);
    }
}

void EyesView::drawSleeping() {
    const int16_t lx = eyeLX(_tft->getWidth());
    const int16_t rx = eyeRX(_tft->getWidth());
    const int16_t cy = eyeY(_tft->getHeight()) + EYE_H / 2;
    _tft->fillRect(lx - 4, cy - 4, EYE_W + 8, 8, COLOR_EYES);
    _tft->fillRect(rx - 4, cy - 4, EYE_W + 8, 8, COLOR_EYES);
    _tft->drawText(rx + 28, cy - 50, "Z", COLOR_EYES, _backgroundColor, FONT_MEDIUM);
    _tft->drawText(rx + 8, cy - 30, "z", COLOR_EYES, _backgroundColor, FONT_SMALL);
}

void EyesView::drawCurious() {
    // 眼睛完全复用默认表情，尺寸、位置与间距均不改变。
    drawNormal(false);

    // 原图像素问号的六个独立矩形块。
    _tft->fillRect(209, 13, 12, 5, COLOR_EYES);
    _tft->fillRect(205, 18, 5, 5, COLOR_EYES);
    _tft->fillRect(221, 18, 5, 10, COLOR_EYES);
    _tft->fillRect(217, 28, 5, 5, COLOR_EYES);
    _tft->fillRect(212, 32, 5, 5, COLOR_EYES);
    _tft->fillRect(212, 42, 5, 5, COLOR_EYES);
}

void EyesView::drawSurprised() {
    // 完全复用默认表情的方眼尺寸、间距与纵向位置。
    drawNormal(false);

    // 右上角使用方形感叹号表达惊讶，与 Sleeping 的 Zz 状态符号呼应。
    const int16_t symbolX = _tft->getWidth() - 20;
    _tft->fillRect(symbolX, 14, 8, 24, COLOR_EYES);
    _tft->fillRect(symbolX, 44, 8, 8, COLOR_EYES);
}

void EyesView::drawGrumpy() {
    const int16_t lx = eyeLX(_tft->getWidth());
    const int16_t rx = eyeRX(_tft->getWidth());
    const int16_t ey = eyeY(_tft->getHeight());
    for (int8_t thickness = -5; thickness <= 5; thickness++) {
        _tft->drawLine(lx - 5, ey + 2 + thickness,
                       lx + EYE_W + 5, ey + 22 + thickness, COLOR_EYES);
        _tft->drawLine(rx - 5, ey + 22 + thickness,
                       rx + EYE_W + 5, ey + 2 + thickness, COLOR_EYES);
    }
    _tft->fillRect(lx, ey + 25, EYE_W, EYE_H - 25, COLOR_EYES);
    _tft->fillRect(rx, ey + 25, EYE_W, EYE_H - 25, COLOR_EYES);
}

void EyesView::drawLove() {
    const int16_t lx = eyeLX(_tft->getWidth()) + EYE_W / 2;
    const int16_t rx = eyeRX(_tft->getWidth()) + EYE_W / 2;
    const int16_t cy = eyeY(_tft->getHeight()) + EYE_H / 2;
    drawHeart(lx, cy);
    drawHeart(rx, cy);

    // 短小的上扬弧线，让爱心眼保留辨识度的同时更有表情
    const int16_t mouthX = _tft->getWidth() / 2;
    const int16_t mouthY = cy + 61;
    for (int8_t offset = -3; offset <= 3; offset++) {
        _tft->drawLine(mouthX - 12, mouthY + offset,
                       mouthX - 5, mouthY + 6 + offset, COLOR_EYES);
        _tft->drawLine(mouthX - 5, mouthY + 6 + offset,
                       mouthX + 5, mouthY + 6 + offset, COLOR_EYES);
        _tft->drawLine(mouthX + 5, mouthY + 6 + offset,
                       mouthX + 12, mouthY + offset, COLOR_EYES);
    }
    _tft->fillCircle(mouthX - 12, mouthY, 3, COLOR_EYES);
    _tft->fillCircle(mouthX + 12, mouthY, 3, COLOR_EYES);
}

void EyesView::drawChevron(int16_t cx, int16_t cy, int16_t arm, int16_t reach,
                           uint8_t thickness, bool rightFacing) {
    for (int8_t offset = -static_cast<int8_t>(thickness);
         offset <= static_cast<int8_t>(thickness); offset++) {
        if (rightFacing) {
            _tft->drawLine(cx - reach / 2, cy - arm + offset,
                           cx + reach / 2, cy + offset, COLOR_EYES);
            _tft->drawLine(cx + reach / 2, cy + offset,
                           cx - reach / 2, cy + arm + offset, COLOR_EYES);
        } else {
            _tft->drawLine(cx + reach / 2, cy - arm + offset,
                           cx - reach / 2, cy + offset, COLOR_EYES);
            _tft->drawLine(cx - reach / 2, cy + offset,
                           cx + reach / 2, cy + arm + offset, COLOR_EYES);
        }
    }
}

void EyesView::drawHeart(int16_t cx, int16_t cy) {
    // 圆润双瓣与逐行收窄的下半部，避免圆形和三角形拼接出的硬边
    _tft->fillCircle(cx - 11, cy - 10, 15, COLOR_EYES);
    _tft->fillCircle(cx + 11, cy - 10, 15, COLOR_EYES);

    static constexpr uint8_t HALF_WIDTHS[] = {
        28, 28, 28, 27, 27, 27, 26, 26, 25,
        25, 24, 24, 23, 23, 22, 21, 21, 20,
        19, 19, 18, 17, 16, 16, 15, 14, 13,
        12, 11, 10, 9, 8, 7, 5, 3, 1
    };
    for (uint8_t row = 0; row < sizeof(HALF_WIDTHS); row++) {
        const int16_t halfWidth = HALF_WIDTHS[row];
        _tft->fillRect(cx - halfWidth, cy - 7 + row,
                       halfWidth * 2 + 1, 1, COLOR_EYES);
    }
}

void EyesView::scheduleNextBlink(unsigned long now) {
    _nextBlinkMs = now + random(BLINK_MIN_INTERVAL_MS,
                                BLINK_MAX_INTERVAL_MS + 1);
}

bool EyesView::supportsBlink() const {
    return _expression == ExpressionId::NORMAL;
}

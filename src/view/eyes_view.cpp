#include "eyes_view.h"

#define EYE_W   30
#define EYE_H   60
#define EYE_GAP 120
#define EYE_OY  40

namespace {
constexpr unsigned long BLINK_MIN_INTERVAL_MS = 2500;
constexpr unsigned long BLINK_MAX_INTERVAL_MS = 6000;
constexpr unsigned long SLEEPY_BLINK_MIN_INTERVAL_MS = 4000;
constexpr unsigned long SLEEPY_BLINK_MAX_INTERVAL_MS = 8000;
constexpr unsigned long BLINK_DURATION_MS = 160;

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
    , _isBlinking(false)
{
}

void EyesView::init() {
    _isBlinking = false;
    redraw();
    scheduleNextBlink(millis());
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
    if (!supportsBlink()) return;

    const unsigned long now = millis();
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
        case ExpressionId::SLEEPY:    drawSleepy(_isBlinking); break;
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

void EyesView::drawSleepy(bool blinking) {
    const int16_t lx = eyeLX(_tft->getWidth());
    const int16_t rx = eyeRX(_tft->getWidth());
    const int16_t ey = eyeY(_tft->getHeight());
    if (blinking) {
        _tft->fillRect(lx - 2, ey + 39, EYE_W + 4, 7, COLOR_EYES);
        _tft->fillRect(rx - 2, ey + 39, EYE_W + 4, 7, COLOR_EYES);
        return;
    }
    _tft->fillRect(lx, ey + 24, EYE_W, 36, COLOR_EYES);
    _tft->fillRect(rx, ey + 24, EYE_W, 36, COLOR_EYES);
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
    const int16_t lx = eyeLX(_tft->getWidth());
    const int16_t rx = eyeRX(_tft->getWidth());
    const int16_t ey = eyeY(_tft->getHeight());
    _tft->fillRect(lx - 4, ey + 8, EYE_W + 8, EYE_H - 16, COLOR_EYES);
    _tft->fillRect(rx + 5, ey - 5, EYE_W - 10, EYE_H + 10, COLOR_EYES);
}

void EyesView::drawSurprised() {
    const int16_t lx = eyeLX(_tft->getWidth()) + EYE_W / 2;
    const int16_t rx = eyeRX(_tft->getWidth()) + EYE_W / 2;
    const int16_t cy = eyeY(_tft->getHeight()) + EYE_H / 2;
    _tft->fillCircle(lx, cy, 24, COLOR_EYES);
    _tft->fillCircle(rx, cy, 24, COLOR_EYES);
    _tft->fillCircle(lx, cy, 11, _backgroundColor);
    _tft->fillCircle(rx, cy, 11, _backgroundColor);
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
    _tft->fillCircle(cx - 9, cy - 10, 12, COLOR_EYES);
    _tft->fillCircle(cx + 9, cy - 10, 12, COLOR_EYES);
    _tft->getTft().fillTriangle(cx - 21, cy - 7, cx + 21, cy - 7,
                                cx, cy + 24, COLOR_EYES);
}

void EyesView::scheduleNextBlink(unsigned long now) {
    const bool sleepy = _expression == ExpressionId::SLEEPY;
    const unsigned long minimum = sleepy
        ? SLEEPY_BLINK_MIN_INTERVAL_MS : BLINK_MIN_INTERVAL_MS;
    const unsigned long maximum = sleepy
        ? SLEEPY_BLINK_MAX_INTERVAL_MS : BLINK_MAX_INTERVAL_MS;
    _nextBlinkMs = now + random(minimum, maximum + 1);
}

bool EyesView::supportsBlink() const {
    return _expression == ExpressionId::NORMAL ||
           _expression == ExpressionId::SLEEPY;
}

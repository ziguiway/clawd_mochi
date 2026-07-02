#include "eyes_view.h"

#define EYE_W   30
#define EYE_H   60
#define EYE_GAP 120
#define EYE_OX  0
#define EYE_OY  40

static int16_t eyeLX(int16_t ox, int16_t screenW) {
    return (screenW - (EYE_W * 2 + EYE_GAP)) / 2 + EYE_OX + ox;
}
static int16_t eyeRX(int16_t ox, int16_t screenW) {
    return eyeLX(ox, screenW) + EYE_W + EYE_GAP;
}
static int16_t eyeY(int16_t screenH) {
    return (screenH - EYE_H) / 2 - EYE_OY;
}

EyesView::EyesView(TftDisplay* tft)
    : _tft(tft), _lastFrameMs(0), _blinkCounter(0)
    , _lookOffset(0), _lookDir(1), _isBlinking(false)
    , _prevLookOffset(0), _prevBlinking(false), _firstFrame(true)
{
}

void EyesView::init() {
    _blinkCounter = random(50, 150);
    _lookOffset = 0;
    _lookDir = 1;
    _isBlinking = false;
    _prevLookOffset = 0;
    _prevBlinking = false;
    _firstFrame = true;
    _tft->fillScreen(COLOR_ORANGE);
    drawEyes();
    _firstFrame = false;
}

void EyesView::update() {
    unsigned long now = millis();
    if (now - _lastFrameMs < 80) return;
    _lastFrameMs = now;

    updateBlink();
    updateLook();

    if (_lookOffset != _prevLookOffset || _isBlinking != _prevBlinking) {
        eraseEyes(_prevLookOffset, _prevBlinking);
        drawEyes();
        _prevLookOffset = _lookOffset;
        _prevBlinking = _isBlinking;
    }
}

void EyesView::eraseEyes(int16_t offset, bool blinking) {
    int16_t w = _tft->getWidth();
    int16_t h = _tft->getHeight();
    int16_t lx = eyeLX(offset, w);
    int16_t rx = eyeRX(offset, w);
    int16_t ey = eyeY(h);

    if (blinking) {
        _tft->fillRect(lx, ey + EYE_H / 2 - 3, EYE_W, 6, COLOR_ORANGE);
        _tft->fillRect(rx, ey + EYE_H / 2 - 3, EYE_W, 6, COLOR_ORANGE);
    } else {
        _tft->fillRect(lx, ey, EYE_W, EYE_H, COLOR_ORANGE);
        _tft->fillRect(rx, ey, EYE_W, EYE_H, COLOR_ORANGE);
    }
}

void EyesView::drawEyes() {
    int16_t w = _tft->getWidth();
    int16_t h = _tft->getHeight();
    int16_t lx = eyeLX(_lookOffset, w);
    int16_t rx = eyeRX(_lookOffset, w);
    int16_t ey = eyeY(h);

    if (_isBlinking) {
        _tft->fillRect(lx, ey + EYE_H / 2 - 3, EYE_W, 6, COLOR_BLACK);
        _tft->fillRect(rx, ey + EYE_H / 2 - 3, EYE_W, 6, COLOR_BLACK);
    } else {
        _tft->fillRect(lx, ey, EYE_W, EYE_H, COLOR_BLACK);
        _tft->fillRect(rx, ey, EYE_W, EYE_H, COLOR_BLACK);
    }
}

void EyesView::updateBlink() {
    _blinkCounter--;
    if (_blinkCounter <= 0) {
        if (_isBlinking) {
            _isBlinking = false;
            _blinkCounter = random(50, 150);
        } else {
            _isBlinking = true;
            _blinkCounter = 3;
        }
    }
}

void EyesView::updateLook() {
    _lookOffset += _lookDir;
    if (abs(_lookOffset) > 16) _lookDir = -_lookDir;
    if (random(100) < 2) _lookDir = -_lookDir;
}

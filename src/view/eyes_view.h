#pragma once

#include <Arduino.h>
#include "../hardware/tft_display.h"
#include "../config/cfg_display.h"
#include "expression_id.h"

class EyesView {
public:
    explicit EyesView(TftDisplay* tft);
    void init();
    void update();
    void setExpression(ExpressionId expression);
    ExpressionId getExpression() const { return _expression; }
    void redraw();
    void setBackgroundColor(uint16_t color) { _backgroundColor = color; }

private:
    TftDisplay* _tft;
    uint16_t _backgroundColor;
    ExpressionId _expression;
    unsigned long _nextBlinkMs;
    unsigned long _blinkStartedMs;
    bool _isBlinking;

    void drawExpression();
    void drawNormal(bool blinking);
    void drawHappy();
    void drawSleepy(bool blinking);
    void drawSleeping();
    void drawCurious();
    void drawSurprised();
    void drawGrumpy();
    void drawLove();
    void drawChevron(int16_t cx, int16_t cy, int16_t arm, int16_t reach,
                     uint8_t thickness, bool rightFacing);
    void drawHeart(int16_t cx, int16_t cy);
    void scheduleNextBlink(unsigned long now);
    bool supportsBlink() const;
};

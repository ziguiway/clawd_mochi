#pragma once

#include <Arduino.h>
#include "../hardware/tft_display.h"
#include "../config/cfg_display.h"

class EyesView {
public:
    explicit EyesView(TftDisplay* tft);
    void init();
    void update();

private:
    TftDisplay* _tft;
    unsigned long _lastFrameMs;
    int _blinkCounter;
    int _lookOffset;
    int _lookDir;
    bool _isBlinking;

    void drawEyes();
    void eraseEyes(int16_t offset, bool blinking);
    void updateBlink();
    void updateLook();

    int16_t _prevLookOffset;
    bool _prevBlinking;
    bool _firstFrame;
};

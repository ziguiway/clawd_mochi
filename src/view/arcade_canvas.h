#pragma once

#include <Adafruit_GFX.h>

class TftDisplay;

// 240x240 RGB565 整帧画布。游戏先在内存中完成一帧，再一次性推送，
// 避免分数、方块和小球分别刷新造成闪屏。
class ArcadeCanvas : public Adafruit_GFX {
public:
    explicit ArcadeCanvas(TftDisplay* tft);

    void drawPixel(int16_t x, int16_t y, uint16_t color) override;
    void clear(uint16_t color);
    void flush();
    uint16_t* pixels() { return _pixels; }

private:
    TftDisplay* _tft;
    uint16_t _pixels[240 * 240];
};


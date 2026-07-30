#pragma once

#include <Adafruit_GFX.h>

class TftDisplay;

// 240x240 RGB565 条带画布。
//
// ESP32-C3 没有 PSRAM，不能常驻一张 115,200 字节的 RGB565 整帧画布。
// 游戏会把同一帧按 STRIP_HEIGHT 行重复绘制到这个小缓冲并逐条推送；
// 颜色和像素精度不变，静态 RAM 占用从 115,200 字节降到 7,680 字节。
class ArcadeCanvas : public Adafruit_GFX {
public:
    static constexpr int16_t WIDTH = 240;
    static constexpr int16_t HEIGHT = 240;
    static constexpr int16_t STRIP_HEIGHT = 16;
    static constexpr size_t BUFFER_BYTES =
        WIDTH * STRIP_HEIGHT * sizeof(uint16_t);

    explicit ArcadeCanvas(TftDisplay* tft);

    void drawPixel(int16_t x, int16_t y, uint16_t color) override;
    void writePixel(int16_t x, int16_t y, uint16_t color) override;
    void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                       uint16_t color) override;
    void writeFastVLine(int16_t x, int16_t y, int16_t h,
                        uint16_t color) override;
    void writeFastHLine(int16_t x, int16_t y, int16_t w,
                        uint16_t color) override;
    void drawFastVLine(int16_t x, int16_t y, int16_t h,
                       uint16_t color) override;
    void drawFastHLine(int16_t x, int16_t y, int16_t w,
                       uint16_t color) override;
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                  uint16_t color) override;
    void fillScreen(uint16_t color) override;

    // beginFrame() 后绘制一遍场景，再调用 nextStrip()。
    // nextStrip() 返回 true 时，用同样的游戏状态再次绘制下一条；
    // 返回 false 表示 240 行均已推送完成。
    void beginFrame(uint16_t background);
    bool nextStrip();

private:
    TftDisplay* _tft;
    uint16_t _pixels[WIDTH * STRIP_HEIGHT];
    int16_t _stripY;
    int16_t _stripRows;
    uint16_t _background;

    void clearStrip();
    void flushStrip();
};

#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include "../config/cfg_display.h"

class TftDisplay {
public:
    TftDisplay();

    void init();
    void setBacklight(bool on);
    void setBrightness(uint8_t level);

    void clear(uint16_t color = COLOR_BLACK);
    void fillScreen(uint16_t color);

    void drawText(int x, int y, const char* text, uint16_t color, uint16_t bgColor = COLOR_BLACK, uint8_t size = FONT_SMALL);
    void drawTextCentered(int y, const char* text, uint16_t color, uint16_t bgColor = COLOR_BLACK, uint8_t size = FONT_SMALL);

    void drawRect(int x, int y, int w, int h, uint16_t color);
    void fillRect(int x, int y, int w, int h, uint16_t color);

    void drawCircle(int x, int y, int r, uint16_t color);
    void fillCircle(int x, int y, int r, uint16_t color);

    void drawLine(int x0, int y0, int x1, int y1, uint16_t color);

    void drawRoundRect(int x, int y, int w, int h, int r, uint16_t color);
    void fillRoundRect(int x, int y, int w, int h, int r, uint16_t color);

    int getWidth() const { return CFG_DISPLAY_WIDTH; }
    int getHeight() const { return CFG_DISPLAY_HEIGHT; }

    int getTextWidth(const char* text, uint8_t size = FONT_SMALL);
    int getTextHeight(uint8_t size = FONT_SMALL);

    Adafruit_ST7789& getTft() { return _tft; }

private:
    Adafruit_ST7789 _tft;
    bool _backlightOn;
};

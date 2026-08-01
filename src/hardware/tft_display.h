#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include "../config/cfg_display.h"
#include "../config/font_style.h"
#include <U8g2_for_Adafruit_GFX.h>

class ScaledTextGfx : public Adafruit_GFX {
public:
    explicit ScaledTextGfx(Adafruit_ST7789& target);

    void configure(int16_t originX, int16_t originY, uint16_t scaleXQ8,
                   uint16_t scaleYQ8);
    void beginWrite();
    void endWrite();
    void drawPixel(int16_t x, int16_t y, uint16_t color) override;
    void drawFastHLine(int16_t x, int16_t y, int16_t w,
                       uint16_t color) override;
    void drawFastVLine(int16_t x, int16_t y, int16_t h,
                       uint16_t color) override;

private:
    void drawScaledRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                        uint16_t color);

    Adafruit_ST7789& _target;
    int16_t _originX;
    int16_t _originY;
    uint16_t _scaleXQ8;
    uint16_t _scaleYQ8;
};

class TftDisplay {
public:
    TftDisplay();

    void init();
    void setBacklight(bool on);
    void setBrightness(uint8_t level);

    void clear(uint16_t color = COLOR_BLACK);
    void fillScreen(uint16_t color);

    void setFontStyle(FontStyle style) { _fontStyle = style; }
    FontStyle getFontStyle() const { return _fontStyle; }

    void drawText(int x, int y, const char* text, uint16_t color, uint16_t bgColor = COLOR_BLACK, uint8_t size = FONT_SMALL);
    void drawTextCentered(int y, const char* text, uint16_t color, uint16_t bgColor = COLOR_BLACK, uint8_t size = FONT_SMALL);

    void drawRect(int x, int y, int w, int h, uint16_t color);
    void fillRect(int x, int y, int w, int h, uint16_t color);

    void drawCircle(int x, int y, int r, uint16_t color);
    void fillCircle(int x, int y, int r, uint16_t color);

    void drawLine(int x0, int y0, int x1, int y1, uint16_t color);

    void drawRoundRect(int x, int y, int w, int h, int r, uint16_t color);
    void fillRoundRect(int x, int y, int w, int h, int r, uint16_t color);
    void fillEllipse(int x, int y, int rx, int ry, uint16_t color);

    // 将 RGB565 像素以单次 SPI 事务推送到屏幕。
    void pushRgb565Rect(int16_t x, int16_t y, uint16_t width,
                        uint16_t height, const uint16_t* pixels);
    void pushRgb565Row(int16_t x, int16_t y, const uint16_t* pixels,
                       uint16_t width);

    int getWidth() const { return CFG_DISPLAY_WIDTH; }
    int getHeight() const { return CFG_DISPLAY_HEIGHT; }

    int getTextWidth(const char* text, uint8_t size = FONT_SMALL);
    int getTextHeight(uint8_t size = FONT_SMALL);

    Adafruit_ST7789& getTft() { return _tft; }

private:
    const uint8_t* u8FontForText(const char* text, uint8_t size) const;
    uint16_t u8ScaleXQ8(const char* text, uint8_t size);
    uint16_t u8ScaleYQ8(const char* text, uint8_t size);
    bool isNumericText(const char* text) const;
    void drawU8Text(int x, int y, const char* text, uint16_t color,
                   uint16_t bgColor, uint8_t size);

    Adafruit_ST7789 _tft;
    ScaledTextGfx _scaledTextGfx;
    U8G2_FOR_ADAFRUIT_GFX _u8Text;
    FontStyle _fontStyle;
    bool _backlightOn;
};

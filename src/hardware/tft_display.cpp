#include "tft_display.h"

TftDisplay::TftDisplay()
    : _tft(CFG_DISPLAY_PIN_CS, CFG_DISPLAY_PIN_DC, CFG_DISPLAY_PIN_RST)
    , _backlightOn(true)
{
}

void TftDisplay::init() {
    pinMode(CFG_DISPLAY_PIN_BL, OUTPUT);
    setBacklight(true);

    SPI.begin(CFG_DISPLAY_PIN_SCL, -1, CFG_DISPLAY_PIN_SDA, CFG_DISPLAY_PIN_CS);
    _tft.init(CFG_DISPLAY_WIDTH, CFG_DISPLAY_HEIGHT);
    _tft.setSPISpeed(CFG_DISPLAY_SPI_FREQ);
    _tft.setRotation(1);
    _tft.fillScreen(COLOR_BLACK);
}

void TftDisplay::setBacklight(bool on) {
    _backlightOn = on;
    digitalWrite(CFG_DISPLAY_PIN_BL, on ? HIGH : LOW);
}

void TftDisplay::setBrightness(uint8_t level) {
    analogWrite(CFG_DISPLAY_PIN_BL, level);
}

void TftDisplay::clear(uint16_t color) {
    _tft.fillScreen(color);
}

void TftDisplay::fillScreen(uint16_t color) {
    _tft.fillScreen(color);
}

void TftDisplay::drawText(int x, int y, const char* text, uint16_t color, uint16_t bgColor, uint8_t size) {
    _tft.setTextSize(size);
    _tft.setTextColor(color, bgColor);
    _tft.setCursor(x, y);
    _tft.print(text);
}

void TftDisplay::drawTextCentered(int y, const char* text, uint16_t color, uint16_t bgColor, uint8_t size) {
    _tft.setTextSize(size);
    int16_t x1, y1;
    uint16_t w, h;
    _tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    int x = (CFG_DISPLAY_WIDTH - w) / 2;
    _tft.setTextColor(color, bgColor);
    _tft.setCursor(x, y);
    _tft.print(text);
}

void TftDisplay::drawRect(int x, int y, int w, int h, uint16_t color) {
    _tft.drawRect(x, y, w, h, color);
}

void TftDisplay::fillRect(int x, int y, int w, int h, uint16_t color) {
    _tft.fillRect(x, y, w, h, color);
}

void TftDisplay::drawCircle(int x, int y, int r, uint16_t color) {
    _tft.drawCircle(x, y, r, color);
}

void TftDisplay::fillCircle(int x, int y, int r, uint16_t color) {
    _tft.fillCircle(x, y, r, color);
}

void TftDisplay::drawLine(int x0, int y0, int x1, int y1, uint16_t color) {
    _tft.drawLine(x0, y0, x1, y1, color);
}

void TftDisplay::drawRoundRect(int x, int y, int w, int h, int r, uint16_t color) {
    _tft.drawRoundRect(x, y, w, h, r, color);
}

void TftDisplay::fillRoundRect(int x, int y, int w, int h, int r, uint16_t color) {
    if (w <= 0 || h <= 0) return;

    // Adafruit_GFX 内部会绘制宽度为 w - 2r 的中心矩形。
    // 半径达到短边一半时该宽度可能为 0，部分 ST7789 驱动会下溢并锁住 SPI。
    const int shortestSide = w < h ? w : h;
    const int maxRadius = (shortestSide - 1) / 2;
    if (r < 0) r = 0;
    if (r > maxRadius) r = maxRadius;

    if (r == 0) {
        _tft.fillRect(x, y, w, h, color);
        return;
    }
    _tft.fillRoundRect(x, y, w, h, r, color);
}

void TftDisplay::fillEllipse(int x, int y, int rx, int ry, uint16_t color) {
    if (rx < 0 || ry < 0) return;
    if (rx == 0) {
        _tft.drawFastVLine(x, y - ry, ry * 2 + 1, color);
        return;
    }
    if (ry == 0) {
        _tft.drawFastHLine(x - rx, y, rx * 2 + 1, color);
        return;
    }

    // 自己管理一次 SPI 写事务，并只调用 writeFastHLine。
    // 避免 Adafruit_GFX::fillEllipse() 在 startWrite() 后再次调用
    // drawFastHLine() 所产生的嵌套事务。
    const int64_t rx2 = static_cast<int64_t>(rx) * rx;
    const int64_t ry2 = static_cast<int64_t>(ry) * ry;
    const int64_t limit = rx2 * ry2;
    int span = rx;

    _tft.startWrite();
    for (int row = 0; row <= ry; row++) {
        const int64_t rowTerm = static_cast<int64_t>(row) * row * rx2;
        while (span > 0 &&
               static_cast<int64_t>(span) * span * ry2 + rowTerm > limit) {
            span--;
        }

        const int lineWidth = span * 2 + 1;
        _tft.writeFastHLine(x - span, y + row, lineWidth, color);
        if (row != 0) {
            _tft.writeFastHLine(x - span, y - row, lineWidth, color);
        }
    }
    _tft.endWrite();
}

int TftDisplay::getTextWidth(const char* text, uint8_t size) {
    _tft.setTextSize(size);
    int16_t x1, y1;
    uint16_t w, h;
    _tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    return w;
}

int TftDisplay::getTextHeight(uint8_t size) {
    _tft.setTextSize(size);
    return size * 8;
}

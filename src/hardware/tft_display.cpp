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
    _tft.fillRoundRect(x, y, w, h, r, color);
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

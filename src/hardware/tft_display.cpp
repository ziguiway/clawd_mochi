#include "tft_display.h"

ScaledTextGfx::ScaledTextGfx(Adafruit_ST7789& target)
    : Adafruit_GFX(CFG_DISPLAY_WIDTH, CFG_DISPLAY_HEIGHT)
    , _target(target)
    , _originX(0)
    , _originY(0)
    , _scaleXQ8(256)
    , _scaleYQ8(256)
{
}

void ScaledTextGfx::configure(int16_t originX, int16_t originY,
                              uint16_t scaleXQ8, uint16_t scaleYQ8) {
    _originX = originX;
    _originY = originY;
    _scaleXQ8 = scaleXQ8;
    _scaleYQ8 = scaleYQ8;
}

void ScaledTextGfx::beginWrite() {
    _target.startWrite();
}

void ScaledTextGfx::endWrite() {
    _target.endWrite();
}

void ScaledTextGfx::drawScaledRect(int16_t x0, int16_t y0, int16_t x1,
                                   int16_t y1, uint16_t color) {
    const int16_t left = _originX +
        static_cast<int32_t>(x0) * _scaleXQ8 / 256;
    const int16_t top = _originY +
        static_cast<int32_t>(y0) * _scaleYQ8 / 256;
    const int16_t right = _originX +
        static_cast<int32_t>(x1) * _scaleXQ8 / 256;
    const int16_t bottom = _originY +
        static_cast<int32_t>(y1) * _scaleYQ8 / 256;
    if (right <= left || bottom <= top) return;
    _target.writeFillRect(left, top, right - left, bottom - top, color);
}

void ScaledTextGfx::drawPixel(int16_t x, int16_t y, uint16_t color) {
    drawScaledRect(x, y, x + 1, y + 1, color);
}

void ScaledTextGfx::drawFastHLine(int16_t x, int16_t y, int16_t w,
                                  uint16_t color) {
    drawScaledRect(x, y, x + w, y + 1, color);
}

void ScaledTextGfx::drawFastVLine(int16_t x, int16_t y, int16_t h,
                                  uint16_t color) {
    drawScaledRect(x, y, x + 1, y + h, color);
}

TftDisplay::TftDisplay()
    : _tft(CFG_DISPLAY_PIN_CS, CFG_DISPLAY_PIN_DC, CFG_DISPLAY_PIN_RST)
    , _scaledTextGfx(_tft)
    , _fontStyle(FontStyle::PIXEL)
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
    _u8Text.begin(_scaledTextGfx);
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
    if (_fontStyle == FontStyle::PIXEL) {
        _tft.setTextSize(size);
        _tft.setTextColor(color, bgColor);
        _tft.setCursor(x, y);
        _tft.print(text);
        return;
    }
    drawU8Text(x, y, text, color, bgColor, size);
}

void TftDisplay::drawTextCentered(int y, const char* text, uint16_t color, uint16_t bgColor, uint8_t size) {
    if (_fontStyle == FontStyle::PIXEL) {
        _tft.setTextSize(size);
        int16_t x1, y1;
        uint16_t w, h;
        _tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
        _tft.setTextColor(color, bgColor);
        _tft.setCursor((CFG_DISPLAY_WIDTH - w) / 2, y);
        _tft.print(text);
        return;
    }
    const int x = (CFG_DISPLAY_WIDTH - getTextWidth(text, size)) / 2;
    drawU8Text(x, y, text, color, bgColor, size);
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

void TftDisplay::pushRgb565Rect(int16_t x, int16_t y, uint16_t width,
                                uint16_t height, const uint16_t* pixels) {
    if (!pixels || x < 0 || y < 0 || width == 0 || height == 0 ||
        x + width > CFG_DISPLAY_WIDTH || y + height > CFG_DISPLAY_HEIGHT) {
        return;
    }
    _tft.startWrite();
    _tft.setAddrWindow(x, y, width, height);
    _tft.writePixels(const_cast<uint16_t*>(pixels),
                     static_cast<uint32_t>(width) * height, true, false);
    _tft.endWrite();
}

void TftDisplay::pushRgb565Row(int16_t x, int16_t y, const uint16_t* pixels,
                               uint16_t width) {
    pushRgb565Rect(x, y, width, 1, pixels);
}

int TftDisplay::getTextWidth(const char* text, uint8_t size) {
    return text ? strlen(text) * 6 * size : 0;
}

int TftDisplay::getTextHeight(uint8_t size) {
    return size * 8;
}

bool TftDisplay::isNumericText(const char* text) const {
    if (!text || !text[0]) return false;
    for (const char* cursor = text; *cursor; ++cursor) {
        const char c = *cursor;
        if (!((c >= '0' && c <= '9') || c == '.' || c == ':' ||
              c == '%' || c == '+' || c == '-' || c == '/' || c == ' ')) {
            return false;
        }
    }
    return true;
}

const uint8_t* TftDisplay::u8FontForText(const char* text, uint8_t size) const {
    const bool numeric = isNumericText(text);
    switch (_fontStyle) {
        case FontStyle::PIXEL:
            if (size <= FONT_SMALL) return u8g2_font_t0_11b_tr;
            if (size == FONT_MEDIUM) return u8g2_font_t0_15b_tr;
            if (size == FONT_LARGE) return u8g2_font_t0_18b_tr;
            return u8g2_font_t0_22b_tr;
        case FontStyle::COURIER:
            if (size <= FONT_SMALL) return u8g2_font_courB08_tr;
            if (size == FONT_MEDIUM) return u8g2_font_courB14_tr;
            return u8g2_font_courB24_tr;
        case FontStyle::TERMINAL:
            if (size <= FONT_SMALL) return u8g2_font_profont10_tr;
            if (size == FONT_MEDIUM) return u8g2_font_profont17_tr;
            if (size == FONT_LARGE) return u8g2_font_profont22_tr;
            return u8g2_font_profont29_tr;
        case FontStyle::DASHBOARD:
            if (numeric) {
                if (size <= FONT_SMALL) return u8g2_font_helvB08_tr;
                if (size == FONT_MEDIUM) return u8g2_font_logisoso20_tn;
                if (size == FONT_LARGE) return u8g2_font_logisoso24_tn;
                if (size == 4) return u8g2_font_logisoso32_tn;
                return u8g2_font_logisoso38_tn;
            }
            if (size <= FONT_SMALL) return u8g2_font_helvB08_tr;
            if (size == FONT_MEDIUM) return u8g2_font_helvB14_tr;
            return u8g2_font_helvB24_tr;
        default:
            return u8g2_font_t0_11b_tr;
    }
}

uint16_t TftDisplay::u8ScaleXQ8(const char* text, uint8_t size) {
    _u8Text.setFont(u8FontForText(text, size));
    const int rawWidth = _u8Text.getUTF8Width(text);
    if (rawWidth <= 0) return 256;
    const uint32_t targetWidth = strlen(text) * 6U * size;
    return static_cast<uint16_t>(
        (targetWidth * 256U + rawWidth / 2) / rawWidth);
}

uint16_t TftDisplay::u8ScaleYQ8(const char* text, uint8_t size) {
    _u8Text.setFont(u8FontForText(text, size));
    const int fontHeight =
        _u8Text.getFontAscent() - _u8Text.getFontDescent();
    if (fontHeight <= 0) return 256;
    const uint16_t targetHeight = static_cast<uint16_t>(size) * 8U;
    return static_cast<uint16_t>(
        (static_cast<uint32_t>(targetHeight) * 256U + fontHeight / 2) /
        fontHeight);
}

void TftDisplay::drawU8Text(int x, int y, const char* text, uint16_t color,
                            uint16_t bgColor, uint8_t size) {
    const uint8_t* font = u8FontForText(text, size);
    _u8Text.setFont(font);
    _scaledTextGfx.configure(x, y, u8ScaleXQ8(text, size),
                             u8ScaleYQ8(text, size));
    _u8Text.setFontDirection(0);
    _u8Text.setFontMode(0);
    _u8Text.setForegroundColor(color);
    _u8Text.setBackgroundColor(bgColor);
    _scaledTextGfx.beginWrite();
    _u8Text.drawUTF8(0, _u8Text.getFontAscent(), text);
    _scaledTextGfx.endWrite();
}

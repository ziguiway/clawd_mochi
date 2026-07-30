#include "arcade_canvas.h"

#include "../hardware/tft_display.h"
#include <algorithm>

ArcadeCanvas::ArcadeCanvas(TftDisplay* tft)
    : Adafruit_GFX(WIDTH, HEIGHT)
    , _tft(tft)
    , _pixels{}
    , _stripY(0)
    , _stripRows(STRIP_HEIGHT)
    , _background(0) {
    setTextWrap(false);
}

void ArcadeCanvas::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= WIDTH || y < _stripY ||
        y >= _stripY + _stripRows) {
        return;
    }
    _pixels[static_cast<uint32_t>(y - _stripY) * WIDTH + x] = color;
}

void ArcadeCanvas::writePixel(int16_t x, int16_t y, uint16_t color) {
    drawPixel(x, y, color);
}

void ArcadeCanvas::writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                                 uint16_t color) {
    if (w <= 0 || h <= 0 || x >= WIDTH || x + w <= 0 ||
        y >= _stripY + _stripRows || y + h <= _stripY) {
        return;
    }

    const int16_t left = std::max<int16_t>(0, x);
    const int16_t right = std::min<int16_t>(WIDTH, x + w);
    const int16_t top = std::max<int16_t>(_stripY, y);
    const int16_t bottom = std::min<int16_t>(_stripY + _stripRows, y + h);
    for (int16_t yy = top; yy < bottom; yy++) {
        uint16_t* row = _pixels +
            static_cast<uint32_t>(yy - _stripY) * WIDTH + left;
        std::fill(row, row + (right - left), color);
    }
}

void ArcadeCanvas::writeFastVLine(int16_t x, int16_t y, int16_t h,
                                  uint16_t color) {
    writeFillRect(x, y, 1, h, color);
}

void ArcadeCanvas::writeFastHLine(int16_t x, int16_t y, int16_t w,
                                  uint16_t color) {
    writeFillRect(x, y, w, 1, color);
}

void ArcadeCanvas::drawFastVLine(int16_t x, int16_t y, int16_t h,
                                 uint16_t color) {
    writeFastVLine(x, y, h, color);
}

void ArcadeCanvas::drawFastHLine(int16_t x, int16_t y, int16_t w,
                                 uint16_t color) {
    writeFastHLine(x, y, w, color);
}

void ArcadeCanvas::fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                            uint16_t color) {
    writeFillRect(x, y, w, h, color);
}

void ArcadeCanvas::fillScreen(uint16_t color) {
    writeFillRect(0, 0, WIDTH, HEIGHT, color);
}

void ArcadeCanvas::beginFrame(uint16_t background) {
    _background = background;
    _stripY = 0;
    _stripRows = STRIP_HEIGHT;
    clearStrip();
}

bool ArcadeCanvas::nextStrip() {
    flushStrip();
    _stripY += _stripRows;
    if (_stripY >= HEIGHT) return false;
    _stripRows = std::min<int16_t>(STRIP_HEIGHT, HEIGHT - _stripY);
    clearStrip();
    return true;
}

void ArcadeCanvas::clearStrip() {
    std::fill(_pixels, _pixels + WIDTH * _stripRows, _background);
}

void ArcadeCanvas::flushStrip() {
    Adafruit_ST7789& screen = _tft->getTft();
    screen.startWrite();
    screen.setAddrWindow(0, _stripY, WIDTH, _stripRows);
    screen.writePixels(_pixels,
                       static_cast<uint32_t>(WIDTH) * _stripRows,
                       true, false);
    screen.endWrite();
}

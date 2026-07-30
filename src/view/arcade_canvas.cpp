#include "arcade_canvas.h"

#include "../hardware/tft_display.h"

ArcadeCanvas::ArcadeCanvas(TftDisplay* tft)
    : Adafruit_GFX(240, 240), _tft(tft), _pixels{} {
    setTextWrap(false);
}

void ArcadeCanvas::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= 240 || y < 0 || y >= 240) return;
    _pixels[static_cast<uint32_t>(y) * 240 + x] = color;
}

void ArcadeCanvas::clear(uint16_t color) {
    for (uint32_t i = 0; i < 240UL * 240UL; i++) _pixels[i] = color;
}

void ArcadeCanvas::flush() {
    Adafruit_ST7789& screen = _tft->getTft();
    screen.startWrite();
    screen.setAddrWindow(0, 0, 240, 240);
    for (uint16_t y = 0; y < 240; y++) {
        screen.writePixels(_pixels + static_cast<uint32_t>(y) * 240,
                           240, true, false);
    }
    screen.endWrite();
}


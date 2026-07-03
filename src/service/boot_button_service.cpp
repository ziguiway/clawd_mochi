#include "boot_button_service.h"
#include "../hardware/tft_display.h"
#include "wifi_config_service.h"
#include "../config/cfg_display.h"
#include "../utils/logger.h"

#define BOOT_BUTTON_PIN 9
#define BOOT_RESET_DURATION_MS 5000

BootButtonService::BootButtonService(TftDisplay* tft, WifiConfigService* wifiService)
    : _tft(tft), _wifiService(wifiService), _pressStart(0), _pressed(false)
{
}

void BootButtonService::init() {
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
}

void BootButtonService::update() {
    bool current = (digitalRead(BOOT_BUTTON_PIN) == LOW);
    if (current && !_pressed) {
        _pressStart = millis();
        _pressed = true;
    } else if (current && _pressed) {
        if (millis() - _pressStart > BOOT_RESET_DURATION_MS) {
            LOG_WARN("BootBtn", "长按 5 秒,恢复出厂设置");
            _tft->clear(COLOR_RED);
            _tft->drawTextCentered(110, "RESET", COLOR_WHITE, COLOR_RED, FONT_LARGE);
            delay(1000);
            _wifiService->reset();
        }
    } else if (!current && _pressed) {
        _pressed = false;
    }
}

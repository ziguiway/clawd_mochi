#include "mode_select_state.h"
#include "app_state_machine.h"
#include "../config/cfg_display.h"
#include "../utils/logger.h"

void ModeSelectState::onEnter() {
    _startMs = millis();
    TftDisplay* tft = _ctx->tft();
    tft->clear(COLOR_DARKBG);
    tft->fillRect(0, 0, CFG_DISPLAY_WIDTH, 3, COLOR_ORANGE);
    tft->fillRect(0, CFG_DISPLAY_HEIGHT - 3, CFG_DISPLAY_WIDTH, 3, COLOR_ORANGE);
    tft->drawTextCentered(60, "Select Mode", COLOR_ORANGE, COLOR_DARKBG, FONT_MEDIUM);
    tft->drawTextCentered(110, "Serial: send any key", COLOR_WHITE, COLOR_DARKBG, FONT_SMALL);
    tft->drawTextCentered(140, "LAN: wait 3s", COLOR_GRAY, COLOR_DARKBG, FONT_SMALL);
}

void ModeSelectState::onUpdate() {
    if (Serial.available() > 0) {
        while (Serial.available()) Serial.read();
        _ctx->opMode()->setMode(OperationModeService::Mode::SERIAL);
        LOG_INFO("ModeSelect", "检测到串口输入 → SERIAL");
        static_cast<AppStateMachine*>(_ctx)->transitionTo(AppStateMachine::SERIAL_IDLE);
        return;
    }
    if (millis() - _startMs >= DETECT_MS) {
        _ctx->opMode()->setMode(OperationModeService::Mode::LAN);
        LOG_INFO("ModeSelect", "超时 → LAN");
        static_cast<AppStateMachine*>(_ctx)->transitionTo(AppStateMachine::PROVISIONING);
    }
}

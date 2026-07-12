#include "mode_select_state.h"
#include "app_state_machine.h"
#include "../config/cfg_display.h"
#include "../utils/logger.h"

namespace {
[[maybe_unused]] bool isSerialReady() {
    return Serial || Serial.available() > 0;
}
}

void ModeSelectState::onEnter() {
    _startMs = millis();
    TftDisplay* tft = _ctx->tft();
    tft->clear(COLOR_DARKBG);
    tft->fillRect(0, 0, CFG_DISPLAY_WIDTH, 3, COLOR_ORANGE);
    tft->fillRect(0, CFG_DISPLAY_HEIGHT - 3, CFG_DISPLAY_WIDTH, 3, COLOR_ORANGE);
    tft->drawTextCentered(70, "Clawd Mochi", COLOR_ORANGE, COLOR_DARKBG, FONT_MEDIUM);
    tft->drawTextCentered(120, "WiFi mode", COLOR_WHITE, COLOR_DARKBG, FONT_SMALL);
    tft->drawTextCentered(140, "connecting...", COLOR_GRAY, COLOR_DARKBG, FONT_SMALL);
}

void ModeSelectState::onUpdate() {
    // 默认 LAN 模式:ESP32-C3 只要 USB 插着 Serial 就为 true,
    // 旧逻辑会一直进 SERIAL,导致 WiFi 模式没法用。串口模式后续再支持。
    _ctx->opMode()->setMode(OperationModeService::Mode::LAN);
    LOG_INFO("ModeSelect", "默认 → LAN");
    static_cast<AppStateMachine*>(_ctx)->transitionTo(AppStateMachine::PROVISIONING);

    // 串口模式入口(暂时禁用,保留逻辑以后恢复):
    // if (isSerialReady()) {
    //     while (Serial.available()) Serial.read();
    //     _ctx->opMode()->setMode(OperationModeService::Mode::SERIAL);
    //     LOG_INFO("ModeSelect", "检测到串口连接 → SERIAL");
    //     static_cast<AppStateMachine*>(_ctx)->transitionTo(AppStateMachine::SERIAL_IDLE);
    //     return;
    // }
    // if (millis() - _startMs >= DETECT_MS) {
    //     _ctx->opMode()->setMode(OperationModeService::Mode::LAN);
    //     LOG_INFO("ModeSelect", "超时 → LAN");
    //     static_cast<AppStateMachine*>(_ctx)->transitionTo(AppStateMachine::PROVISIONING);
    // }
}

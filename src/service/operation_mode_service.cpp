#include "operation_mode_service.h"
#include "../utils/logger.h"

OperationModeService* OperationModeService::_instance = nullptr;

OperationModeService::OperationModeService() : _mode(Mode::LAN) {}

void OperationModeService::detectAtStartup(uint32_t detectMs) {
    LOG_INFO("OpMode", "检测启动模式,等待串口输入 %lu ms...", (unsigned long)detectMs);
    const uint32_t start = millis();
    bool gotInput = false;
    while (millis() - start < detectMs) {
        if (Serial.available() > 0) {
            while (Serial.available()) Serial.read();
            gotInput = true;
            break;
        }
        delay(10);
    }
    _mode = gotInput ? Mode::SERIAL : Mode::LAN;
    LOG_INFO("OpMode", "启动模式: %s", getModeName());
}

void OperationModeService::setMode(Mode mode) {
    if (_mode == mode) return;
    _mode = mode;
    LOG_INFO("OpMode", "切换模式: %s", getModeName());
}

const char* OperationModeService::getModeName() const {
    switch (_mode) {
        case Mode::LAN:    return "LAN";
        case Mode::SERIAL: return "SERIAL";
        default:           return "UNKNOWN";
    }
}

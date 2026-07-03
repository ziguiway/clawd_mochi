#include "operation_mode_service.h"
#include "../utils/logger.h"

OperationModeService* OperationModeService::_instance = nullptr;

OperationModeService::OperationModeService() : _mode(Mode::LAN) {}

namespace {
bool isSerialReady() {
    return Serial || Serial.available() > 0;
}
}

void OperationModeService::detectAtStartup(uint32_t detectMs) {
    LOG_INFO("OpMode", "检测启动模式,等待串口连接 %lu ms...", (unsigned long)detectMs);
    const uint32_t start = millis();
    bool serialReady = false;
    while (millis() - start < detectMs) {
        if (isSerialReady()) {
            while (Serial.available()) Serial.read();
            serialReady = true;
            break;
        }
        delay(10);
    }
    _mode = serialReady ? Mode::SERIAL : Mode::LAN;
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

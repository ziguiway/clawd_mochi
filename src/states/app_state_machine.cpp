#include "app_state_machine.h"
#include "../config/cfg_display.h"
#include "../utils/logger.h"

AppStateMachine::AppStateMachine()
    : _cc(&_sm)
    , _display(&_tft, &_cc, &_wifi)
    , _web(&_cc, &_wifi, &_time, &_display)
    , _serial(&_wifi, &_cc, &_time)
    , _bootButton(&_tft, &_wifi)
    , _currentId(BOOT)
    , _current(nullptr)
{
}

void AppStateMachine::init() {
    OperationModeService::bind(&_opMode);
    WifiConfigService::bind(&_wifi);

    if (!LittleFS.begin(true)) {
        LOG_ERROR("App", "LittleFS 初始化失败");
    }

    Logger::getInstance().init();
    Logger::getInstance().setTimeProvider(TimeService::timestampCallback);
    LOG_INFO("App", "Clawd Mochi 启动中...");

    _tft.init();
    _tft.clear(COLOR_BLACK);
    _time.init();
    _cc.init();
    _bootButton.init();

    registerState(BOOT, &_boot);
    registerState(MODE_SELECT, &_modeSelect);
    registerState(PROVISIONING, &_provisioning);
    registerState(LAN_IDLE, &_lanIdle);
    registerState(LAN_WORKING, &_lanWorking);
    registerState(SERIAL_IDLE, &_serialIdle);
    registerState(SERIAL_WORKING, &_serialWorking);
    registerState(RESET, &_reset);

    _currentId = BOOT;
    _current = nullptr;
    transitionTo(BOOT);
}

void AppStateMachine::update() {
    if (_current) _current->onUpdate();
    _bootButton.update();
}

void AppStateMachine::transitionTo(StateId id) {
    if (id == _currentId && _current != nullptr) return;
    if (_current) _current->onExit();
    _currentId = id;
    _current = _states[id];
    if (_current) {
        _current->_ctx = this;
        LOG_INFO("App", "→ 状态: %s", _current->getName());
        _current->onEnter();
    }
}

void AppStateMachine::registerState(StateId id, State* state) {
    _states[id] = state;
}

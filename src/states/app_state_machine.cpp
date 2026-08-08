#include "app_state_machine.h"
#include "../config/cfg_display.h"
#include "../utils/logger.h"
#include "../utils/memory_monitor.h"
#include <esp_ota_ops.h>

namespace {
constexpr uint32_t OTA_BOOT_CONFIRM_DELAY_MS = 5000;
}

AppStateMachine::AppStateMachine()
    : _cc(&_sm)
    , _weather(&_wifi)
    , _crypto(&_wifi)
    , _market(&_wifi)
    , _holiday(&_wifi, &_time)
    , _ota(&_wifi, &_time)
    , _stream(&_tft)
    , _display(&_tft, &_cc, &_wifi, &_time, &_prefs, &_weather, &_crypto,
               &_market, &_holiday, &_timetable, &_stream, &_keyboardPet)
    , _web(&_cc, &_wifi, &_time, &_display, &_prefs, &_weather, &_crypto, &_market,
           &_timetable, &_ota)
    , _serial(&_wifi, &_cc, &_time)
    , _bootButton(&_tft, &_wifi)
    , _currentId(BOOT)
    , _current(nullptr)
    , _otaBootReadyMs(0)
    , _otaBootConfirmed(false)
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
    MemoryMonitor::logSnapshot("boot");

    _prefs.init();
    _weather.init();
    _crypto.init();
    _market.init();
    _holiday.init();
    _timetable.init();
    _tft.init();
    _tft.clear(COLOR_BLACK);
    _time.init();
    _ota.init();
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
    _keyboardPet.update();
    if (_current) _current->onUpdate();
    _bootButton.update();
    confirmOtaBootIfReady();
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
        if (_otaBootReadyMs == 0 && (id == LAN_IDLE || id == SERIAL_IDLE)) {
            _otaBootReadyMs = millis();
        }
    }
}

void AppStateMachine::registerState(StateId id, State* state) {
    _states[id] = state;
}

void AppStateMachine::confirmOtaBootIfReady() {
    if (_otaBootConfirmed || _otaBootReadyMs == 0 ||
        millis() - _otaBootReadyMs < OTA_BOOT_CONFIRM_DELAY_MS) {
        return;
    }

    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (!running || esp_ota_get_state_partition(running, &state) != ESP_OK) {
        LOG_ERROR("OTA", "无法读取当前启动分区状态，暂不确认固件");
        return;
    }

    if (state == ESP_OTA_IMG_PENDING_VERIFY) {
        if (esp_ota_mark_app_valid_cancel_rollback() != ESP_OK) {
            LOG_ERROR("OTA", "启动自检通过，但确认当前固件失败");
            return;
        }
        LOG_INFO("OTA", "启动自检通过，当前固件已确认");
    }
    _otaBootConfirmed = true;
}

#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include "state.h"
#include "boot_state.h"
#include "mode_select_state.h"
#include "provisioning_state.h"
#include "lan_idle_state.h"
#include "lan_working_state.h"
#include "serial_idle_state.h"
#include "serial_working_state.h"
#include "reset_state.h"
#include "../hardware/tft_display.h"
#include "../service/time_service.h"
#include "../service/claude_code_service.h"
#include "../service/wifi_config_service.h"
#include "../service/web_service.h"
#include "../service/display_service.h"
#include "../service/serial_command_service.h"
#include "../service/operation_mode_service.h"
#include "../service/boot_button_service.h"
#include "../utils/state_machine.h"
#include "../utils/logger.h"

// 应用状态机:持有所有服务实例 + 状态实例,管理当前状态
class AppStateMachine : public IAppContext {
public:
    enum StateId {
        BOOT, MODE_SELECT, PROVISIONING,
        LAN_IDLE, LAN_WORKING,
        SERIAL_IDLE, SERIAL_WORKING,
        RESET
    };

    AppStateMachine();

    void init();
    void update();

    void transitionTo(StateId id);
    StateId getCurrentStateId() const { return _currentId; }
    State* getCurrentState() const { return _current; }

    // 服务访问器(供各状态使用)
    TftDisplay*           tft()         override { return &_tft; }
    TimeService*          time()        override { return &_time; }
    ClaudeCodeService*    cc()          override { return &_cc; }
    WifiConfigService*    wifi()        override { return &_wifi; }
    DisplayService*       display()     override { return &_display; }
    WebService*           web()         override { return &_web; }
    SerialCommandService* serial()      override { return &_serial; }
    OperationModeService* opMode()      override { return &_opMode; }
    BootButtonService*    bootButton()  override { return &_bootButton; }

private:
    // 服务实例
    TftDisplay           _tft;
    TimeService          _time;
    StateMachine         _sm;
    ClaudeCodeService    _cc;
    WifiConfigService    _wifi;
    DisplayService       _display;
    WebService           _web;
    SerialCommandService _serial;
    OperationModeService _opMode;
    BootButtonService    _bootButton;

    // 状态实例
    BootState           _boot;
    ModeSelectState     _modeSelect;
    ProvisioningState   _provisioning;
    LANIdleState        _lanIdle;
    LANWorkingState     _lanWorking;
    SerialIdleState     _serialIdle;
    SerialWorkingState  _serialWorking;
    ResetState          _reset;

    State* _states[8] = {nullptr};
    StateId _currentId;
    State* _current;

    void registerState(StateId id, State* state);
};

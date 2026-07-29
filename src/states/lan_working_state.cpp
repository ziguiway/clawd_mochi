#include "lan_working_state.h"
#include "app_state_machine.h"

namespace {
constexpr unsigned long SETTLED_STATUS_HOLD_MS = 10000;
// WiFi 掉线宽限:瞬时抖动(漫游/信道切换)不打断面板,持续断开才回配网
constexpr unsigned long WIFI_LOST_GRACE_MS = 8000;

bool isSettledStatus(ClaudeCodeService::Status status) {
    return status == ClaudeCodeService::Status::DONE ||
           status == ClaudeCodeService::Status::ERROR;
}
}

void LANWorkingState::onEnter() {
    _settledSinceMs = 0;
    _wifiLostSinceMs = 0;
    _wifiLost = false;
    _ctx->display()->switchToInfoMode();
}

void LANWorkingState::onUpdate() {
    _ctx->wifi()->update();
    _ctx->web()->update();
    _ctx->time()->update();
    _ctx->cc()->update();
    _ctx->serial()->update();
    _ctx->display()->update();

    // WiFi 持续断开超过宽限期:回空闲表情,后台继续重连。
    if (!_ctx->wifi()->isConnected()) {
        if (!_wifiLost) {
            _wifiLost = true;
            _wifiLostSinceMs = millis();
        }
        if (millis() - _wifiLostSinceMs >= WIFI_LOST_GRACE_MS) {
            static_cast<AppStateMachine*>(_ctx)->transitionTo(AppStateMachine::LAN_IDLE);
            return;
        }
    } else {
        _wifiLost = false;
    }

    if (!_ctx->display()->isClaudeStatusEnabled()) {
        static_cast<AppStateMachine*>(_ctx)->transitionTo(AppStateMachine::LAN_IDLE);
        return;
    }

    auto status = _ctx->cc()->getStatus();
    if (status == ClaudeCodeService::Status::IDLE) {
        static_cast<AppStateMachine*>(_ctx)->transitionTo(AppStateMachine::LAN_IDLE);
        return;
    }

    if (isSettledStatus(status)) {
        if (_settledSinceMs == 0) _settledSinceMs = millis();
        if (millis() - _settledSinceMs >= SETTLED_STATUS_HOLD_MS) {
            static_cast<AppStateMachine*>(_ctx)->transitionTo(AppStateMachine::LAN_IDLE);
        }
    } else {
        _settledSinceMs = 0;
    }
}

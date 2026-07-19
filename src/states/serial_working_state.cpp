#include "serial_working_state.h"
#include "app_state_machine.h"

namespace {
constexpr unsigned long SETTLED_STATUS_HOLD_MS = 10000;

bool isSettledStatus(ClaudeCodeService::Status status) {
    return status == ClaudeCodeService::Status::DONE ||
           status == ClaudeCodeService::Status::ERROR;
}
}

void SerialWorkingState::onEnter() {
    _settledSinceMs = 0;
    _ctx->display()->switchToInfoMode();
}

void SerialWorkingState::onUpdate() {
    _ctx->wifi()->update();
    _ctx->web()->update();
    _ctx->time()->update();
    _ctx->cc()->update();
    _ctx->serial()->update();
    _ctx->display()->update();

    auto status = _ctx->cc()->getStatus();
    if (status == ClaudeCodeService::Status::IDLE) {
        static_cast<AppStateMachine*>(_ctx)->transitionTo(AppStateMachine::SERIAL_IDLE);
        return;
    }

    if (isSettledStatus(status)) {
        if (_settledSinceMs == 0) _settledSinceMs = millis();
        if (millis() - _settledSinceMs >= SETTLED_STATUS_HOLD_MS) {
            static_cast<AppStateMachine*>(_ctx)->transitionTo(AppStateMachine::SERIAL_IDLE);
        }
    } else {
        _settledSinceMs = 0;
    }
}

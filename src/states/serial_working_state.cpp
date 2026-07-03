#include "serial_working_state.h"
#include "app_state_machine.h"

void SerialWorkingState::onEnter() {
    _ctx->display()->switchToInfoMode();
}

void SerialWorkingState::onUpdate() {
    _ctx->cc()->update();
    _ctx->serial()->update();
    _ctx->display()->update();

    auto status = _ctx->cc()->getStatus();
    if (status == ClaudeCodeService::Status::IDLE) {
        static_cast<AppStateMachine*>(_ctx)->transitionTo(AppStateMachine::SERIAL_IDLE);
    }
}

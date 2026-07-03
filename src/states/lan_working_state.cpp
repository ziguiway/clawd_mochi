#include "lan_working_state.h"
#include "app_state_machine.h"

void LANWorkingState::onEnter() {
    _ctx->display()->switchToInfoMode();
}

void LANWorkingState::onUpdate() {
    _ctx->wifi()->update();
    _ctx->web()->update();
    _ctx->time()->update();
    _ctx->cc()->update();
    _ctx->serial()->update();
    _ctx->display()->update();

    auto status = _ctx->cc()->getStatus();
    if (status == ClaudeCodeService::Status::IDLE ||
        status == ClaudeCodeService::Status::SLEEPING) {
        static_cast<AppStateMachine*>(_ctx)->transitionTo(AppStateMachine::LAN_IDLE);
    }
}

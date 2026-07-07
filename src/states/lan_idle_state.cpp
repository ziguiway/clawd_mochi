#include "lan_idle_state.h"
#include "app_state_machine.h"

namespace {
bool shouldShowInfo(ClaudeCodeService::Status status) {
    return status == ClaudeCodeService::Status::THINKING ||
           status == ClaudeCodeService::Status::WORKING ||
           status == ClaudeCodeService::Status::PERMISSION ||
           status == ClaudeCodeService::Status::SWEEPING;
}
}

void LANIdleState::onEnter() {
    _ctx->display()->switchToExpressionMode();
}

void LANIdleState::onUpdate() {
    _ctx->wifi()->update();
    _ctx->web()->update();
    _ctx->time()->update();
    _ctx->cc()->update();
    _ctx->serial()->update();
    _ctx->display()->update();

    auto status = _ctx->cc()->getStatus();
    if (shouldShowInfo(status)) {
        static_cast<AppStateMachine*>(_ctx)->transitionTo(AppStateMachine::LAN_WORKING);
        return;
    }
    if (!_ctx->wifi()->isConnected()) {
        static_cast<AppStateMachine*>(_ctx)->transitionTo(AppStateMachine::PROVISIONING);
    }
}

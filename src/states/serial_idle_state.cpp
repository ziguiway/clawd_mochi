#include "serial_idle_state.h"
#include "app_state_machine.h"
#include "../utils/logger.h"

namespace {
    bool s_serialServicesInitialized = false;

    bool shouldShowInfo(ClaudeCodeService::Status status) {
        return status == ClaudeCodeService::Status::THINKING ||
               status == ClaudeCodeService::Status::WORKING ||
               status == ClaudeCodeService::Status::PERMISSION ||
               status == ClaudeCodeService::Status::SWEEPING;
    }
}

void SerialIdleState::onEnter() {
    if (!s_serialServicesInitialized) {
        s_serialServicesInitialized = true;
        _ctx->wifi()->init();
        _ctx->web()->init();
        _ctx->time()->init();
        _ctx->cc()->init();
        _ctx->serial()->init();
        _ctx->display()->init();
        LOG_INFO("SerialIdle", "串口模式服务初始化完成");
    }
    _ctx->display()->switchToExpressionMode();
}

void SerialIdleState::onUpdate() {
    _ctx->wifi()->update();
    _ctx->web()->update();
    _ctx->time()->update();
    _ctx->cc()->update();
    _ctx->serial()->update();
    _ctx->display()->update();

    auto status = _ctx->cc()->getStatus();
    if (shouldShowInfo(status)) {
        static_cast<AppStateMachine*>(_ctx)->transitionTo(AppStateMachine::SERIAL_WORKING);
    }
}

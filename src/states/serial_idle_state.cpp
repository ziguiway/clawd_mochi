#include "serial_idle_state.h"
#include "app_state_machine.h"
#include "../utils/logger.h"

namespace {
    bool s_serialServicesInitialized = false;
}

void SerialIdleState::onEnter() {
    if (!s_serialServicesInitialized) {
        s_serialServicesInitialized = true;
        _ctx->cc()->init();
        _ctx->serial()->init();
        _ctx->display()->init();
        LOG_INFO("SerialIdle", "串口模式服务初始化完成");
    }
    _ctx->display()->switchToExpressionMode();
}

void SerialIdleState::onUpdate() {
    _ctx->cc()->update();
    _ctx->serial()->update();
    _ctx->display()->update();

    auto status = _ctx->cc()->getStatus();
    if (status == ClaudeCodeService::Status::THINKING ||
        status == ClaudeCodeService::Status::WORKING) {
        static_cast<AppStateMachine*>(_ctx)->transitionTo(AppStateMachine::SERIAL_WORKING);
    }
}

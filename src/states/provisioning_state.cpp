#include "provisioning_state.h"
#include "app_state_machine.h"
#include "../utils/logger.h"

namespace {
    bool s_servicesInitialized = false;
}

void ProvisioningState::onEnter() {
    if (!s_servicesInitialized) {
        s_servicesInitialized = true;
        _ctx->wifi()->init();
        _ctx->web()->init();
        _ctx->time()->init();
        _ctx->cc()->init();
        _ctx->serial()->init();
        _ctx->display()->init();
    }
}

void ProvisioningState::onUpdate() {
    _ctx->wifi()->update();
    _ctx->web()->update();
    _ctx->time()->update();
    _ctx->cc()->update();
    _ctx->serial()->update();
    _ctx->display()->updateProvisioning();

    if (_ctx->wifi()->isConnected()) {
        static_cast<AppStateMachine*>(_ctx)->transitionTo(AppStateMachine::LAN_IDLE);
        return;
    }
    // 默认 LAN 模式:WiFi 未连上时停留在配网流程,不回退串口模式
}

void ProvisioningState::onExit() {
    // 切换显示模式,避免配网画面残留
    _ctx->display()->switchToExpressionMode();
}

#include "lan_idle_state.h"
#include "app_state_machine.h"

namespace {
bool shouldShowInfo(ClaudeCodeService::Status status) {
    return status == ClaudeCodeService::Status::THINKING ||
           status == ClaudeCodeService::Status::WORKING ||
           status == ClaudeCodeService::Status::PERMISSION ||
           status == ClaudeCodeService::Status::SWEEPING ||
           status == ClaudeCodeService::Status::SLEEPING;
}
}

void LANIdleState::onEnter() {
    // 配网引导(AP/连接中/重试/成功确认)优先占用屏幕
    if (_ctx->wifi()->getProvisioningMode() != WifiConfigService::ProvisioningMode::NONE) {
        return;
    }
    if (_ctx->wifi()->isConnected()) {
        _ctx->display()->switchToIdleDisplay();
    } else {
        _ctx->display()->switchToExpressionMode();
    }
}

void LANIdleState::onUpdate() {
    _ctx->wifi()->update();
    _ctx->web()->update();
    _ctx->time()->update();
    _ctx->cc()->update();
    _ctx->serial()->update();
    _ctx->display()->update();

    // 配网流程活跃时,屏幕交给配网引导屏,并抑制一切视图切换
    if (_ctx->wifi()->getProvisioningMode() != WifiConfigService::ProvisioningMode::NONE) {
        _ctx->display()->updateProvisioning();
        return;
    }

    // 游戏/媒体占用屏幕时继续接收 Codex 状态，但不抢走画面。
    if (_ctx->display()->isExclusiveDisplayActive()) return;

    auto status = _ctx->cc()->getStatus();
    if (_ctx->display()->isClaudeStatusEnabled() && shouldShowInfo(status)) {
        static_cast<AppStateMachine*>(_ctx)->transitionTo(AppStateMachine::LAN_WORKING);
        return;
    }
}

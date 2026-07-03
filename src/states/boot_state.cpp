#include "boot_state.h"
#include "app_state_machine.h"
#include "../view/boot_animation.h"
#include "../utils/logger.h"

void BootState::onEnter() {
    LOG_INFO("Boot", "启动动画");
    BootAnimation::run(*_ctx->tft());
}

void BootState::onUpdate() {
    static_cast<AppStateMachine*>(_ctx)->transitionTo(AppStateMachine::MODE_SELECT);
}

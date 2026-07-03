#include "reset_state.h"
#include "app_state_machine.h"
#include "../config/cfg_display.h"
#include "../utils/logger.h"

// RESET 状态:由 BootButtonService 长按 5 秒触发。
// 实际的清屏 + reset 逻辑由 BootButtonService 直接执行(它会调用 wifi.reset() → ESP.restart())。
// 此状态作为占位,仅在状态机记录中体现。
void ResetState::onEnter() {
    LOG_WARN("Reset", "进入 RESET 状态(实际清屏与重启由 BootButtonService 完成)");
}

#include "network_request_gate.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace {
SemaphoreHandle_t s_networkGate = nullptr;
}

bool NetworkRequestGate::tryAcquire() {
    // 令牌在主循环取走、由后台任务归还，不能使用带“持有者”概念的
    // Mutex；FreeRTOS 会在跨任务 xSemaphoreGive() 时触发断言。
    // 二值信号量正好表达“网络请求槽位是否空闲”。
    if (!s_networkGate) {
        s_networkGate = xSemaphoreCreateBinary();
        if (s_networkGate) xSemaphoreGive(s_networkGate);
    }
    return s_networkGate && xSemaphoreTake(s_networkGate, 0) == pdTRUE;
}

void NetworkRequestGate::release() {
    if (s_networkGate) xSemaphoreGive(s_networkGate);
}

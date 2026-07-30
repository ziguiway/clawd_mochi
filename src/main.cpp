/*
 * CLAWD MOCHI — ESP32-C3 Super Mini + ST7789 1.54" 240x240
 * Claude Code 实时状态显示伴侣
 *
 * Wiring: SDA→GPIO10, SCL→GPIO8, RST→GPIO2, DC→GPIO1, CS→GPIO4, BL→GPIO3
 * WiFi AP: "ClaWD-Mochi" pw: clawd1234 → http://192.168.4.1
 * UDP 端口: 4210
 *
 * 架构:AppStateMachine 持有所有服务实例 + 状态实例,
 * main 只调 init() / update()。状态转移由各 State 类驱动。
 */

#include <Arduino.h>

#include "config/app_config.h"
#include "states/app_state_machine.h"

// AppStateMachine 常驻静态 RAM。新增服务或游戏若让它超过 32 KB，
// 编译必须失败并要求重新设计生命周期/共享缓冲，而不是继续挤压 TLS 堆。
static_assert(sizeof(AppStateMachine) <= 32U * 1024U,
              "AppStateMachine exceeds the 32 KB static RAM budget");
static_assert(ArcadeCanvas::BUFFER_BYTES <= 12U * 1024U,
              "ArcadeCanvas buffer exceeds the 12 KB render budget");

AppStateMachine appState;

void setup() {
    Serial.begin(115200);
    delay(500);
    appState.init();
}

void loop() {
    appState.update();
    delay(APP_LOOP_INTERVAL_MS);
}

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

/*
 * CLAWD MOCHI — ESP32-C3 Super Mini + ST7789 1.54" 240x240
 * Claude Code 实时状态显示伴侣
 *
 * Wiring: SDA→GPIO10, SCL→GPIO8, RST→GPIO2, DC→GPIO1, CS→GPIO4, BL→GPIO3
 * WiFi AP: "ClaWD-Mochi" pw: clawd1234 → http://192.168.4.1
 * UDP 端口: 4210
 */

#include <Arduino.h>
#include <LittleFS.h>

#include "config/app_config.h"
#include "config/cfg_claude_code.h"
#include "config/cfg_display.h"
#include "config/cfg_wifi.h"

#include "utils/state_machine.h"
#include "utils/logger.h"

#include "hardware/tft_display.h"

#include "service/time_service.h"
#include "service/claude_code_service.h"
#include "service/wifi_config_service.h"
#include "service/web_service.h"
#include "service/display_service.h"
#include "service/serial_command_service.h"
#include "view/boot_animation.h"

// ── 全局实例 ──────────────────────────────────────────────────
TftDisplay           tftDisplay;
TimeService          timeService;
StateMachine         stateMachine;
ClaudeCodeService    ccService(&stateMachine);
WifiConfigService    wifiService;
DisplayService       displayService(&tftDisplay, &ccService, &wifiService);
WebService           webService(&ccService, &wifiService, &timeService, &displayService);
SerialCommandService serialCmd(&wifiService, &ccService, &timeService);

// ── BOOT 按钮 ────────────────────────────────────────────────
#define BOOT_BUTTON_PIN 9
unsigned long bootPressStart = 0;
bool bootPressed = false;
bool ccServiceStarted = false;

void setup() {
    Serial.begin(115200);
    delay(500);

    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS 初始化失败!");
    }

    Logger::getInstance().init();
    Logger::getInstance().setTimeProvider(TimeService::timestampCallback);
    LOG_INFO("Main", "Clawd Mochi 启动中...");

    tftDisplay.init();
    tftDisplay.clear(COLOR_BLACK);

    // 启动动画
    BootAnimation::run(tftDisplay);

    // WiFi 配网(SmartConfig 优先,AP 回退)
    wifiService.init();

    // Web 服务无论配网与否都启动(AP 模式下提供 /wifi/scan /wifi/connect)
    webService.init();
    displayService.init();
    serialCmd.init();

    timeService.init();

    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

    LOG_INFO("Main", "初始化完成");
}

void loop() {
    wifiService.update();
    timeService.update();

    // WiFi 连上或串口模式时启动 Claude Code 服务
    if ((wifiService.isConnected() || wifiService.isSerialMode()) && !ccServiceStarted) {
        ccService.init();
        ccServiceStarted = true;
        LOG_INFO("Main", "Claude Code 服务启动");
    } else if (!wifiService.isConnected() && !wifiService.isSerialMode() && ccServiceStarted) {
        ccServiceStarted = false;
        LOG_INFO("Main", "WiFi 断开,Claude Code 服务暂停");
    }
    if (ccServiceStarted) ccService.update();

    webService.update();
    displayService.update();
    serialCmd.update();

    // BOOT 按钮长按 5 秒恢复出厂设置
    bool bootCurrent = (digitalRead(BOOT_BUTTON_PIN) == LOW);
    if (bootCurrent && !bootPressed) {
        bootPressStart = millis();
        bootPressed = true;
    } else if (bootCurrent && bootPressed) {
        if (millis() - bootPressStart > 5000) {
            LOG_WARN("Main", "BOOT 长按 5 秒，恢复出厂设置");
            tftDisplay.clear(COLOR_RED);
            tftDisplay.drawTextCentered(110, "RESET", COLOR_WHITE, COLOR_RED, FONT_LARGE);
            delay(1000);
            wifiService.reset();
        }
    } else if (!bootCurrent && bootPressed) {
        bootPressed = false;
    }

    delay(APP_LOOP_INTERVAL_MS);
}

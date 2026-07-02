#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "../config/cfg_wifi.h"
#include "../config/cfg_display.h"
#include "claude_code_service.h"
#include "wifi_config_service.h"
#include "time_service.h"
#include "display_service.h"
#include "../utils/logger.h"

class WebService {
public:
    WebService(ClaudeCodeService* ccService, WifiConfigService* wifiService,
               TimeService* timeService, DisplayService* displayService);
    void init();
    void update();

private:
    WebServer _server;
    ClaudeCodeService* _ccService;
    WifiConfigService* _wifiService;
    TimeService* _timeService;
    DisplayService* _displayService;

    void setupRoutes();

    // Original interactive routes
    void handleRoot();
    void handleCmd();
    void handleChar();
    void handleSpeed();
    void handleRedraw();
    void handleCanvas();
    void handleDrawClear();
    void handleDrawStroke();
    void handleBacklight();
    void handleState();
    void handleSerialMode();

    // Existing routes
    void handleWifiSetup();
    void handleLogs();
    void handleFile(const char* path, const char* contentType);
    void handleCCStatus();
    void handleCCTest();
    void handleTime();
    void handleLogsApi();
    void handleLogsClear();
    void handleLogsStatus();

    String getContentType(const String& path);
    String rgb565ToHex(uint16_t c);
};

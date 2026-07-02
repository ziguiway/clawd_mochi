#pragma once

#include <Arduino.h>
#include "wifi_config_service.h"
#include "claude_code_service.h"
#include "time_service.h"

class SerialCommandService {
public:
    SerialCommandService(WifiConfigService* wifiService,
                        ClaudeCodeService* ccService,
                        TimeService* timeService);
    void init();
    void update();

private:
    WifiConfigService* _wifiService;
    ClaudeCodeService* _ccService;
    TimeService* _timeService;
    String _inputBuffer;

    void processCommand(const String& cmd);
    void printHelp();
    void printStatus();
    void printIP();
    void showTime();
    void syncTime();
    void showLogs();
    void clearLogs();
    void setLogLevel(const String& level);
    void resetFactory();
    void restart();
    void handleClaudeCommand(const String& args);
};

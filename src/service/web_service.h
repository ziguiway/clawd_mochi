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
#include "preference_service.h"
#include "crypto_service.h"
#include "market_service.h"
#include "timetable_service.h"
#include "../utils/logger.h"

class WebService {
public:
    WebService(ClaudeCodeService* ccService, WifiConfigService* wifiService,
               TimeService* timeService, DisplayService* displayService,
               PreferenceService* preferenceService,
               CryptoService* cryptoService,
               MarketService* marketService,
               TimetableService* timetableService);
    void init();
    void update();

private:
    WebServer _server;
    bool _started;
    ClaudeCodeService* _ccService;
    WifiConfigService* _wifiService;
    TimeService* _timeService;
    DisplayService* _displayService;
    PreferenceService* _preferenceService;
    CryptoService* _cryptoService;
    MarketService* _marketService;
    TimetableService* _timetableService;
    bool _mediaUploadAccepted;
    bool _mediaUploadComplete;
    int _mediaUploadStatusCode;
    size_t _mediaExpectedBytes;

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
    void handleBrightness();
    void handleTimerStatus();
    void handleTimerStart();
    void handleTimerPause();
    void handleTimerReset();
    void handleTimerConfig();
    void handleSalaryStatus();
    void handleSalaryConfig();
    void handleSalaryStart();
    void handleSalaryPause();
    void handleSalaryResume();
    void handleSalaryFinish();
    void handleSalaryReset();
    void handleDinoStart();
    void handleDinoAction();
    void handleDinoRestart();
    void handleDinoExit();
    void handleDinoState();
    void handleSokobanStart();
    void handleSokobanMove();
    void handleSokobanUndo();
    void handleSokobanRestart();
    void handleSokobanLevel();
    void handleSokobanExit();
    void handleSokobanState();
    void handleArcadeStart();
    void handleArcadeAction();
    void handleArcadeExit();
    void handleArcadeState();
    void handleArcadeCatalog();
    void handlePrefs();
    void handleState();
    void handleExpressions();
    void handleExpressionUpdate();
    void handleProfile();
    void handleProfileUpdate();
    void handleProfileReset();
    void handleConfigExport();
    void handleConfigImport();
    void handleSerialMode();
    void handleCryptoConfig();
    void handleCryptoUpdate();
    void handleCryptoRefresh();
    void handleMarketConfig();
    void handleMarketUpdate();
    void handleMarketRefresh();
    void handleMarketSearch();
    void handleTimetableGet();
    void handleTimetableSave();
    void handleTimetableStatus();
    void handleWakeUpProxy();
    void handleMediaFrame();
    void handleMediaFrameUpload();
    void handleMediaStop();
    void handleMediaStatus();

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
    void sendExpressionState(bool includeList);
    void sendSalaryStatus(int statusCode = 200);
};

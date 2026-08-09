#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "wifi_config_service.h"

class ClaudeUsageService {
public:
    explicit ClaudeUsageService(WifiConfigService* wifiService);

    void init();
    void update();
    void requestRefresh();

    bool setToken(const String& token);
    void clearToken();
    bool hasCredential() const { return _token.length() > 0; }
    bool isLoading() const { return _loading; }
    bool isValid() const { return _valid; }
    bool hasAuthError() const { return _authError; }
    const char* getStatus() const { return _status; }
    float getSessionUsedPct() const { return _sessionUsedPct; }
    float getWeeklyUsedPct() const { return _weeklyUsedPct; }
    int getSessionResetMins() const { return _sessionResetMins; }
    int getWeeklyResetMins() const { return _weeklyResetMins; }
    unsigned long getLastSuccessMs() const { return _lastSuccessMs; }
    uint32_t getVersion() const { return _version; }
    String getJson() const;

private:
    static constexpr unsigned long REFRESH_INTERVAL_MS = 5UL * 60UL * 1000UL;
    static constexpr unsigned long RETRY_INTERVAL_MS = 2UL * 60UL * 1000UL;
    static constexpr size_t MAX_TOKEN_LENGTH = 768;

    WifiConfigService* _wifiService;
    Preferences _preferences;
    String _token;
    float _sessionUsedPct;
    float _weeklyUsedPct;
    int _sessionResetMins;
    int _weeklyResetMins;
    char _status[16];
    bool _valid;
    bool _loading;
    bool _refreshRequested;
    bool _authError;
    unsigned long _lastAttemptMs;
    unsigned long _lastSuccessMs;
    uint8_t _failureCount;
    volatile uint32_t _version;
    TaskHandle_t _refreshTask;

    static void refreshTaskEntry(void* parameter);
    void runRefresh();
    bool fetchUsage();
    unsigned long retryDelayMs() const;
};

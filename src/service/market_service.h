#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "wifi_config_service.h"

struct MarketAsset {
    char secid[16];
    char code[8];
    char label[10];
    char name[40];
    float price;
    float changePercent;
    bool priceValid;
    bool changeValid;
};

class MarketService {
public:
    static constexpr uint8_t MAX_ASSETS = 5;

    explicit MarketService(WifiConfigService* wifiService);

    void init();
    void update();
    void requestRefresh();

    uint8_t getAssetCount() const { return _assetCount; }
    const MarketAsset& getAsset(uint8_t index) const { return _assets[index]; }
    bool isLoading() const { return _loading; }
    uint32_t getVersion() const { return _version; }
    unsigned long getLastSuccessMs() const { return _lastSuccessMs; }

    bool setAssets(const MarketAsset* assets, uint8_t count);
    String getJson() const;
    String searchJson(const String& query);

private:
    static constexpr unsigned long REFRESH_INTERVAL_MS = 5UL * 60UL * 1000UL;
    static constexpr unsigned long RETRY_INTERVAL_MS = 2UL * 60UL * 1000UL;

    WifiConfigService* _wifiService;
    Preferences _preferences;
    MarketAsset _assets[MAX_ASSETS];
    uint8_t _assetCount;
    volatile bool _loading;
    bool _refreshRequested;
    unsigned long _lastAttemptMs;
    unsigned long _lastSuccessMs;
    volatile uint32_t _version;
    TaskHandle_t _refreshTask;

    static void refreshTaskEntry(void* parameter);
    void runRefresh();
    void loadAssets();
    void saveAssets();
    void setDefaults();
    bool fetchQuotes();
    static void copyText(char* dest, size_t size, const char* source, bool upper);
    static String urlEncode(const String& value);
    static String tencentSymbol(const char* secid);
    static String decodeHintText(const String& value);
};

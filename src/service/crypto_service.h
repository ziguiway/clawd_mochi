#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "wifi_config_service.h"

struct CryptoAsset {
    char id[16];
    char symbol[8];
    char name[28];
    bool isGold;
    float price;
    float change24h;
    bool priceValid;
    bool changeValid;
};

class CryptoService {
public:
    static constexpr uint8_t MAX_ASSETS = 5;

    explicit CryptoService(WifiConfigService* wifiService);

    void init();
    void update();
    void requestRefresh();

    uint8_t getAssetCount() const { return _assetCount; }
    const CryptoAsset& getAsset(uint8_t index) const { return _assets[index]; }
    bool isLoading() const { return _loading; }
    bool hasAnyValidQuote() const;
    uint32_t getVersion() const { return _version; }
    unsigned long getLastSuccessMs() const { return _lastSuccessMs; }

    bool setAssets(const CryptoAsset* assets, uint8_t count);
    String getJson() const;

private:
    static constexpr unsigned long REFRESH_INTERVAL_MS = 10UL * 60UL * 1000UL;
    static constexpr unsigned long RETRY_INTERVAL_MS = 2UL * 60UL * 1000UL;

    WifiConfigService* _wifiService;
    Preferences _preferences;
    CryptoAsset _assets[MAX_ASSETS];
    uint8_t _assetCount;
    bool _loading;
    bool _refreshRequested;
    unsigned long _lastAttemptMs;
    unsigned long _lastSuccessMs;
    uint32_t _version;

    void loadAssets();
    void saveAssets();
    void setDefaults();
    bool fetchQuotes();
    bool fetchCoinLore();
    static void copyClean(char* dest, size_t size, const char* source, bool symbol);
};

#include "crypto_service.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ctype.h>

#include "../utils/logger.h"

CryptoService::CryptoService(WifiConfigService* wifiService)
    : _wifiService(wifiService)
    , _assetCount(0)
    , _loading(false)
    , _refreshRequested(false)
    , _lastAttemptMs(0)
    , _lastSuccessMs(0)
    , _version(0)
{
    memset(_assets, 0, sizeof(_assets));
}

void CryptoService::init() {
    _preferences.begin("clawd-crypto", false);
    loadAssets();
    _refreshRequested = true;
}

void CryptoService::requestRefresh() {
    _refreshRequested = true;
    _version++;
}

void CryptoService::update() {
    if (_loading || !_wifiService || !_wifiService->isConnected()) return;

    const unsigned long now = millis();
    const bool lastAttemptFailed = _lastAttemptMs != 0 &&
        (_lastSuccessMs == 0 || _lastAttemptMs > _lastSuccessMs);
    const bool retryDue = lastAttemptFailed &&
        now - _lastAttemptMs >= RETRY_INTERVAL_MS;
    const bool refreshDue = _lastAttemptMs != 0 && !lastAttemptFailed &&
        now - _lastAttemptMs >= REFRESH_INTERVAL_MS;
    if (!_refreshRequested && !retryDue && !refreshDue) return;

    _loading = true;
    _refreshRequested = false;
    _lastAttemptMs = now;
    _version++;

    if (fetchQuotes()) {
        _lastSuccessMs = millis();
    } else {
        LOG_WARN("Crypto", "行情更新失败，将在 2 分钟后重试");
    }

    _loading = false;
    _version++;
}

bool CryptoService::hasAnyValidQuote() const {
    for (uint8_t i = 0; i < _assetCount; i++) {
        if (_assets[i].priceValid) return true;
    }
    return false;
}

bool CryptoService::setAssets(const CryptoAsset* assets, uint8_t count) {
    if (!assets || count == 0 || count > MAX_ASSETS) return false;

    CryptoAsset next[MAX_ASSETS] = {};
    for (uint8_t i = 0; i < count; i++) {
        copyClean(next[i].id, sizeof(next[i].id), assets[i].id, false);
        copyClean(next[i].symbol, sizeof(next[i].symbol), assets[i].symbol, true);
        copyClean(next[i].name, sizeof(next[i].name), assets[i].name, false);
        next[i].isGold = assets[i].isGold;
        // 兼容旧版 XAU 现货配置，统一迁移为 CoinLore 的 Tether Gold (XAUT)。
        if (next[i].isGold || strcmp(next[i].id, "XAU") == 0 ||
            strcmp(next[i].symbol, "XAU") == 0) {
            copyClean(next[i].id, sizeof(next[i].id), "42855", false);
            copyClean(next[i].symbol, sizeof(next[i].symbol), "XAUT", true);
            copyClean(next[i].name, sizeof(next[i].name), "Tether Gold", false);
            next[i].isGold = false;
        }
        if (next[i].id[0] == '\0' || next[i].symbol[0] == '\0') return false;

        for (uint8_t old = 0; old < _assetCount; old++) {
            if (strcmp(next[i].id, _assets[old].id) == 0 &&
                next[i].isGold == _assets[old].isGold) {
                next[i].price = _assets[old].price;
                next[i].change24h = _assets[old].change24h;
                next[i].priceValid = _assets[old].priceValid;
                next[i].changeValid = _assets[old].changeValid;
                break;
            }
        }
    }

    bool unchanged = count == _assetCount;
    for (uint8_t i = 0; unchanged && i < count; i++) {
        unchanged = strcmp(next[i].id, _assets[i].id) == 0 &&
                    strcmp(next[i].symbol, _assets[i].symbol) == 0 &&
                    strcmp(next[i].name, _assets[i].name) == 0 &&
                    next[i].isGold == _assets[i].isGold;
    }
    if (unchanged) return true;

    memcpy(_assets, next, sizeof(_assets));
    _assetCount = count;
    saveAssets();
    requestRefresh();
    LOG_INFO("Crypto", "已保存 %u 个行情资产", _assetCount);
    return true;
}

String CryptoService::getJson() const {
    JsonDocument doc;
    doc["loading"] = _loading;
    doc["lastSuccessMs"] = _lastSuccessMs;
    if (_lastSuccessMs == 0) {
        doc["updatedAgeSec"] = nullptr;
    } else {
        doc["updatedAgeSec"] =
            static_cast<unsigned long>((millis() - _lastSuccessMs) / 1000UL);
    }
    JsonArray list = doc["assets"].to<JsonArray>();
    for (uint8_t i = 0; i < _assetCount; i++) {
        JsonObject item = list.add<JsonObject>();
        item["id"] = _assets[i].id;
        item["symbol"] = _assets[i].symbol;
        item["name"] = _assets[i].name;
        item["gold"] = _assets[i].isGold;
        if (_assets[i].priceValid) item["price"] = _assets[i].price;
        else item["price"] = nullptr;
        if (_assets[i].changeValid) item["change"] = _assets[i].change24h;
        else item["change"] = nullptr;
    }
    String json;
    serializeJson(doc, json);
    return json;
}

void CryptoService::loadAssets() {
    const String stored = _preferences.getString("assets", "");
    if (stored.isEmpty()) {
        setDefaults();
        saveAssets();
        return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, stored) || !doc.is<JsonArray>()) {
        setDefaults();
        saveAssets();
        return;
    }

    JsonArray list = doc.as<JsonArray>();
    if (list.size() == 0 || list.size() > MAX_ASSETS) {
        setDefaults();
        saveAssets();
        return;
    }

    _assetCount = 0;
    bool migrated = false;
    for (JsonObject item : list) {
        CryptoAsset& asset = _assets[_assetCount];
        copyClean(asset.id, sizeof(asset.id), item["id"] | "", false);
        copyClean(asset.symbol, sizeof(asset.symbol), item["symbol"] | "", true);
        copyClean(asset.name, sizeof(asset.name), item["name"] | "", false);
        asset.isGold = item["gold"] | false;
        if (asset.isGold || strcmp(asset.id, "XAU") == 0 ||
            strcmp(asset.symbol, "XAU") == 0) {
            copyClean(asset.id, sizeof(asset.id), "42855", false);
            copyClean(asset.symbol, sizeof(asset.symbol), "XAUT", true);
            copyClean(asset.name, sizeof(asset.name), "Tether Gold", false);
            asset.isGold = false;
            migrated = true;
        }
        if (asset.id[0] == '\0' || asset.symbol[0] == '\0') {
            setDefaults();
            saveAssets();
            return;
        }
        _assetCount++;
    }
    if (migrated) {
        saveAssets();
        LOG_INFO("Crypto", "已将旧 XAU 配置迁移为 XAUT");
    }
}

void CryptoService::saveAssets() {
    JsonDocument doc;
    JsonArray list = doc.to<JsonArray>();
    for (uint8_t i = 0; i < _assetCount; i++) {
        JsonObject item = list.add<JsonObject>();
        item["id"] = _assets[i].id;
        item["symbol"] = _assets[i].symbol;
        item["name"] = _assets[i].name;
        item["gold"] = _assets[i].isGold;
    }
    String json;
    serializeJson(doc, json);
    _preferences.putString("assets", json);
}

void CryptoService::setDefaults() {
    memset(_assets, 0, sizeof(_assets));
    _assetCount = 3;
    copyClean(_assets[0].id, sizeof(_assets[0].id), "90", false);
    copyClean(_assets[0].symbol, sizeof(_assets[0].symbol), "BTC", true);
    copyClean(_assets[0].name, sizeof(_assets[0].name), "Bitcoin", false);
    copyClean(_assets[1].id, sizeof(_assets[1].id), "80", false);
    copyClean(_assets[1].symbol, sizeof(_assets[1].symbol), "ETH", true);
    copyClean(_assets[1].name, sizeof(_assets[1].name), "Ethereum", false);
    copyClean(_assets[2].id, sizeof(_assets[2].id), "42855", false);
    copyClean(_assets[2].symbol, sizeof(_assets[2].symbol), "XAUT", true);
    copyClean(_assets[2].name, sizeof(_assets[2].name), "Tether Gold", false);
}

bool CryptoService::fetchQuotes() {
    return _assetCount > 0 && fetchCoinLore();
}

bool CryptoService::fetchCoinLore() {
    String ids;
    for (uint8_t i = 0; i < _assetCount; i++) {
        if (_assets[i].isGold) continue;
        if (!ids.isEmpty()) ids += ',';
        ids += _assets[i].id;
    }
    if (ids.isEmpty()) return true;

    const String url = "https://api.coinlore.net/api/ticker/?id=" + ids;
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(8000);
    if (!http.begin(client, url)) return false;
    http.useHTTP10(true);
    http.addHeader("Accept-Encoding", "identity");

    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        LOG_WARN("Crypto", "CoinLore 请求失败 HTTP=%d", code);
        http.end();
        return false;
    }
    const String payload = http.getString();
    http.end();

    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, payload);
    if (error || !doc.is<JsonArray>()) {
        LOG_WARN("Crypto", "CoinLore JSON 解析失败: %s", error.c_str());
        return false;
    }

    uint8_t updated = 0;
    for (JsonObject quote : doc.as<JsonArray>()) {
        const char* id = quote["id"] | "";
        for (uint8_t i = 0; i < _assetCount; i++) {
            if (_assets[i].isGold || strcmp(_assets[i].id, id) != 0) continue;
            _assets[i].price = quote["price_usd"].as<float>();
            _assets[i].change24h = quote["percent_change_24h"].as<float>();
            _assets[i].priceValid = _assets[i].price > 0.0f;
            _assets[i].changeValid = !quote["percent_change_24h"].isNull();
            if (_assets[i].priceValid) updated++;
        }
    }
    LOG_INFO("Crypto", "CoinLore 更新 %u 个资产", updated);
    return updated > 0;
}

void CryptoService::copyClean(char* dest, size_t size, const char* source, bool symbol) {
    if (!dest || size == 0) return;
    if (!source) source = "";
    size_t out = 0;
    for (size_t i = 0; source[i] != '\0' && out < size - 1; i++) {
        unsigned char c = static_cast<unsigned char>(source[i]);
        if (symbol && c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        const bool allowed = isalnum(c) || c == ' ' || c == '-' || c == '.' || c == '_';
        if (allowed) dest[out++] = static_cast<char>(c);
    }
    dest[out] = '\0';
}

#include "market_service.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ctype.h>

#include "../utils/logger.h"
#include "../utils/memory_monitor.h"
#include "../utils/network_request_gate.h"

namespace {
String quoteField(const String& value, uint8_t wanted) {
    uint8_t field = 0;
    int start = 0;
    while (start <= static_cast<int>(value.length())) {
        const int end = value.indexOf('~', start);
        if (field == wanted) {
            return end < 0 ? value.substring(start) : value.substring(start, end);
        }
        if (end < 0) break;
        start = end + 1;
        field++;
    }
    return "";
}
}

MarketService::MarketService(WifiConfigService* wifiService)
    : _wifiService(wifiService)
    , _assetCount(0)
    , _loading(false)
    , _refreshRequested(false)
    , _lastAttemptMs(0)
    , _lastSuccessMs(0)
    , _version(0)
    , _refreshTask(nullptr)
{
    memset(_assets, 0, sizeof(_assets));
}

void MarketService::init() {
    _preferences.begin("clawd-market", false);
    loadAssets();
    _refreshRequested = true;
}

void MarketService::requestRefresh() {
    _refreshRequested = true;
    _version++;
}

void MarketService::update() {
    if (_loading || !_wifiService || !_wifiService->isConnected()) return;

    const unsigned long now = millis();
    const bool lastAttemptFailed = _lastAttemptMs != 0 &&
        (_lastSuccessMs == 0 || _lastAttemptMs > _lastSuccessMs);
    const bool retryDue = lastAttemptFailed &&
        now - _lastAttemptMs >= RETRY_INTERVAL_MS;
    const bool refreshDue = _lastAttemptMs != 0 && !lastAttemptFailed &&
        now - _lastAttemptMs >= REFRESH_INTERVAL_MS;
    if (!_refreshRequested && !retryDue && !refreshDue) return;

    if (!NetworkRequestGate::tryAcquire()) return;

    _loading = true;
    _refreshRequested = false;
    _lastAttemptMs = now;
    _version++;
    if (!MemoryMonitor::hasTlsHeadroom("Market")) {
        _loading = false;
        _version++;
        NetworkRequestGate::release();
        return;
    }
    if (xTaskCreate(refreshTaskEntry, "MarketNet", 8192, this, 1,
                    &_refreshTask) != pdPASS) {
        _refreshTask = nullptr;
        _loading = false;
        _refreshRequested = true;
        NetworkRequestGate::release();
        LOG_WARN("Market", "后台请求任务创建失败");
    }
}

void MarketService::refreshTaskEntry(void* parameter) {
    MarketService* service = static_cast<MarketService*>(parameter);
    service->runRefresh();
    service->_loading = false;
    service->_refreshTask = nullptr;
    service->_version++;
    NetworkRequestGate::release();
    vTaskDelete(nullptr);
}

void MarketService::runRefresh() {
    if (fetchQuotes()) {
        _lastSuccessMs = millis();
    } else {
        LOG_WARN("Market", "A股行情更新失败，将在 2 分钟后重试");
    }

}

bool MarketService::setAssets(const MarketAsset* assets, uint8_t count) {
    if (!assets || count == 0 || count > MAX_ASSETS) return false;

    MarketAsset next[MAX_ASSETS] = {};
    for (uint8_t i = 0; i < count; i++) {
        copyText(next[i].secid, sizeof(next[i].secid), assets[i].secid, false);
        copyText(next[i].code, sizeof(next[i].code), assets[i].code, true);
        copyText(next[i].label, sizeof(next[i].label), assets[i].label, true);
        copyText(next[i].name, sizeof(next[i].name), assets[i].name, false);
        if (next[i].secid[0] == '\0' || next[i].code[0] == '\0') return false;
        if (next[i].label[0] == '\0') {
            copyText(next[i].label, sizeof(next[i].label), next[i].code, true);
        }

        for (uint8_t old = 0; old < _assetCount; old++) {
            if (strcmp(next[i].secid, _assets[old].secid) == 0) {
                next[i].price = _assets[old].price;
                next[i].changePercent = _assets[old].changePercent;
                next[i].priceValid = _assets[old].priceValid;
                next[i].changeValid = _assets[old].changeValid;
                break;
            }
        }
    }

    bool unchanged = count == _assetCount;
    for (uint8_t i = 0; unchanged && i < count; i++) {
        unchanged = strcmp(next[i].secid, _assets[i].secid) == 0 &&
                    strcmp(next[i].code, _assets[i].code) == 0 &&
                    strcmp(next[i].label, _assets[i].label) == 0 &&
                    strcmp(next[i].name, _assets[i].name) == 0;
    }
    if (unchanged) return true;

    memcpy(_assets, next, sizeof(_assets));
    _assetCount = count;
    saveAssets();
    requestRefresh();
    LOG_INFO("Market", "已保存 %u 个 A 股行情项目", _assetCount);
    return true;
}

String MarketService::getJson() const {
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
        item["secid"] = _assets[i].secid;
        item["code"] = _assets[i].code;
        item["label"] = _assets[i].label;
        item["name"] = _assets[i].name;
        if (_assets[i].priceValid) item["price"] = _assets[i].price;
        else item["price"] = nullptr;
        if (_assets[i].changeValid) item["change"] = _assets[i].changePercent;
        else item["change"] = nullptr;
    }
    String json;
    serializeJson(doc, json);
    return json;
}

void MarketService::loadAssets() {
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
    for (JsonObject item : list) {
        MarketAsset& asset = _assets[_assetCount];
        copyText(asset.secid, sizeof(asset.secid), item["secid"] | "", false);
        copyText(asset.code, sizeof(asset.code), item["code"] | "", true);
        copyText(asset.label, sizeof(asset.label), item["label"] | "", true);
        copyText(asset.name, sizeof(asset.name), item["name"] | "", false);
        if (asset.secid[0] == '\0' || asset.code[0] == '\0') {
            setDefaults();
            saveAssets();
            return;
        }
        if (asset.label[0] == '\0') {
            copyText(asset.label, sizeof(asset.label), asset.code, true);
        }
        _assetCount++;
    }
}

void MarketService::saveAssets() {
    JsonDocument doc;
    JsonArray list = doc.to<JsonArray>();
    for (uint8_t i = 0; i < _assetCount; i++) {
        JsonObject item = list.add<JsonObject>();
        item["secid"] = _assets[i].secid;
        item["code"] = _assets[i].code;
        item["label"] = _assets[i].label;
        item["name"] = _assets[i].name;
    }
    String json;
    serializeJson(doc, json);
    _preferences.putString("assets", json);
}

void MarketService::setDefaults() {
    memset(_assets, 0, sizeof(_assets));
    _assetCount = 3;

    copyText(_assets[0].secid, sizeof(_assets[0].secid), "1.000001", false);
    copyText(_assets[0].code, sizeof(_assets[0].code), "000001", true);
    copyText(_assets[0].label, sizeof(_assets[0].label), "SSE", true);
    copyText(_assets[0].name, sizeof(_assets[0].name), "上证指数", false);

    copyText(_assets[1].secid, sizeof(_assets[1].secid), "0.399001", false);
    copyText(_assets[1].code, sizeof(_assets[1].code), "399001", true);
    copyText(_assets[1].label, sizeof(_assets[1].label), "SZSE", true);
    copyText(_assets[1].name, sizeof(_assets[1].name), "深证成指", false);

    copyText(_assets[2].secid, sizeof(_assets[2].secid), "0.399006", false);
    copyText(_assets[2].code, sizeof(_assets[2].code), "399006", true);
    copyText(_assets[2].label, sizeof(_assets[2].label), "CYB", true);
    copyText(_assets[2].name, sizeof(_assets[2].name), "创业板指", false);
}

bool MarketService::fetchQuotes() {
    if (_assetCount == 0) return false;

    String symbols;
    for (uint8_t i = 0; i < _assetCount; i++) {
        const String symbol = tencentSymbol(_assets[i].secid);
        if (symbol.isEmpty()) continue;
        if (!symbols.isEmpty()) symbols += ',';
        // s_ 是腾讯的精简报价格式，字段足够显示价格和涨跌幅，响应更小。
        symbols += "s_" + symbol;
    }
    if (symbols.isEmpty()) return false;

    const String url = "https://qt.gtimg.cn/q=" + symbols;
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(8000);
    if (!http.begin(client, url)) return false;
    http.useHTTP10(true);
    http.addHeader("Accept-Encoding", "identity");
    http.addHeader("User-Agent", "Clawd-Mochi/1.0");

    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        LOG_WARN("Market", "腾讯行情请求失败 HTTP=%d", code);
        http.end();
        return false;
    }
    const String payload = http.getString();
    http.end();

    uint8_t updated = 0;
    for (uint8_t i = 0; i < _assetCount; i++) {
        const String symbol = tencentSymbol(_assets[i].secid);
        const String prefix = "v_s_" + symbol + "=\"";
        const int valueStart = payload.indexOf(prefix);
        if (valueStart < 0) {
            _assets[i].priceValid = false;
            _assets[i].changeValid = false;
            continue;
        }
        const int quoteStart = valueStart + prefix.length();
        const int quoteEnd = payload.indexOf('"', quoteStart);
        const String quote = quoteEnd < 0 ? payload.substring(quoteStart)
                                          : payload.substring(quoteStart, quoteEnd);
        const float price = quoteField(quote, 3).toFloat();
        const String changeText = quoteField(quote, 5);
        _assets[i].price = price;
        _assets[i].priceValid = price > 0.0f;
        _assets[i].changePercent = changeText.toFloat();
        _assets[i].changeValid = !changeText.isEmpty();
        if (_assets[i].priceValid) updated++;
    }
    LOG_INFO("Market", "腾讯更新 %u 个行情项目", updated);
    return updated > 0;
}

String MarketService::searchJson(const String& query) {
    JsonDocument output;
    JsonArray results = output["results"].to<JsonArray>();
    if (!_wifiService || !_wifiService->isConnected() || query.isEmpty()) {
        String json;
        serializeJson(output, json);
        return json;
    }

    const String url = "https://smartbox.gtimg.cn/s3/?q=" +
        urlEncode(query) + "&t=all";
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(8000);
    if (!http.begin(client, url)) {
        output["error"] = "search unavailable";
        String json;
        serializeJson(output, json);
        return json;
    }
    http.useHTTP10(true);
    http.addHeader("Accept-Encoding", "identity");

    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        LOG_WARN("Market", "腾讯搜索失败 HTTP=%d", code);
        output["error"] = "search failed";
        http.end();
        String json;
        serializeJson(output, json);
        return json;
    }
    const String payload = http.getString();
    http.end();

    const int valueStart = payload.indexOf('"');
    const int valueEnd = payload.lastIndexOf('"');
    if (valueStart < 0 || valueEnd <= valueStart) {
        output["error"] = "invalid search response";
    } else {
        const String hints = payload.substring(valueStart + 1, valueEnd);
        uint8_t count = 0;
        int start = 0;
        while (start <= hints.length() && count < 10) {
            const int end = hints.indexOf('^', start);
            const String item = end < 0 ? hints.substring(start) : hints.substring(start, end);
            start = end < 0 ? hints.length() + 1 : end + 1;

            const String market = quoteField(item, 0);
            const String stockCode = quoteField(item, 1);
            const String kind = quoteField(item, 4);
            if ((market != "sh" && market != "sz") || stockCode.isEmpty() ||
                (!kind.startsWith("GP") && kind != "ZS")) continue;

            JsonObject result = results.add<JsonObject>();
            result["secid"] = String(market == "sh" ? "1." : "0.") + stockCode;
            result["code"] = stockCode;
            result["label"] = stockCode;
            result["name"] = decodeHintText(quoteField(item, 2));
            count++;
        }
    }

    String json;
    serializeJson(output, json);
    return json;
}

void MarketService::copyText(char* dest, size_t size, const char* source, bool upper) {
    if (!dest || size == 0) return;
    if (!source) source = "";
    size_t out = 0;
    for (size_t i = 0; source[i] != '\0' && out < size - 1; i++) {
        unsigned char c = static_cast<unsigned char>(source[i]);
        if (c < 0x20 || c == 0x7F) continue;
        if (upper && c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        dest[out++] = static_cast<char>(c);
    }
    dest[out] = '\0';
}

String MarketService::urlEncode(const String& value) {
    static const char HEX_DIGITS[] = "0123456789ABCDEF";
    String encoded;
    encoded.reserve(value.length() * 3);
    for (size_t i = 0; i < value.length(); i++) {
        const uint8_t c = static_cast<uint8_t>(value[i]);
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
            c == '~') {
            encoded += static_cast<char>(c);
        } else {
            encoded += '%';
            encoded += HEX_DIGITS[c >> 4];
            encoded += HEX_DIGITS[c & 0x0F];
        }
    }
    return encoded;
}

String MarketService::tencentSymbol(const char* secid) {
    if (!secid || strlen(secid) < 3 || secid[1] != '.') return "";
    if (secid[0] == '1') return "sh" + String(secid + 2);
    if (secid[0] == '0') return "sz" + String(secid + 2);
    return "";
}

String MarketService::decodeHintText(const String& value) {
    String decoded;
    decoded.reserve(value.length());
    for (size_t i = 0; i < value.length(); i++) {
        if (value[i] != '\\' || i + 5 >= value.length() || value[i + 1] != 'u') {
            decoded += value[i];
            continue;
        }
        uint16_t codepoint = 0;
        bool valid = true;
        for (uint8_t digit = 0; digit < 4; digit++) {
            const char c = value[i + 2 + digit];
            uint8_t nibble;
            if (c >= '0' && c <= '9') nibble = c - '0';
            else if (c >= 'a' && c <= 'f') nibble = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') nibble = c - 'A' + 10;
            else { valid = false; break; }
            codepoint = static_cast<uint16_t>((codepoint << 4) | nibble);
        }
        if (!valid) {
            decoded += value[i];
            continue;
        }
        if (codepoint < 0x80) decoded += static_cast<char>(codepoint);
        else if (codepoint < 0x800) {
            decoded += static_cast<char>(0xC0 | (codepoint >> 6));
            decoded += static_cast<char>(0x80 | (codepoint & 0x3F));
        } else {
            decoded += static_cast<char>(0xE0 | (codepoint >> 12));
            decoded += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            decoded += static_cast<char>(0x80 | (codepoint & 0x3F));
        }
        i += 5;
    }
    return decoded;
}

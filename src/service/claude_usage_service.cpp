#include "claude_usage_service.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <cstdlib>
#include <time.h>

#include "../utils/logger.h"
#include "../utils/memory_monitor.h"
#include "../utils/network_request_gate.h"

namespace {
constexpr const char* API_URL = "https://api.anthropic.com/v1/messages";
constexpr const char* API_VERSION = "2023-06-01";
constexpr const char* API_BETA = "oauth-2025-04-20";
constexpr const char* API_MODEL = "claude-haiku-4-5-20251001";
}

ClaudeUsageService::ClaudeUsageService(WifiConfigService* wifiService)
    : _wifiService(wifiService)
    , _sessionUsedPct(0.0f)
    , _weeklyUsedPct(0.0f)
    , _sessionResetMins(-1)
    , _weeklyResetMins(-1)
    , _valid(false)
    , _loading(false)
    , _refreshRequested(false)
    , _authError(false)
    , _lastAttemptMs(0)
    , _lastSuccessMs(0)
    , _failureCount(0)
    , _version(0)
    , _refreshTask(nullptr)
{
    strncpy(_status, "UNKNOWN", sizeof(_status) - 1);
    _status[sizeof(_status) - 1] = '\0';
}

void ClaudeUsageService::init() {
    _preferences.begin("clawd-usage", false);
    _token = _preferences.getString("token", "");
    if (_token.length() > MAX_TOKEN_LENGTH) _token = "";
    _refreshRequested = hasCredential();
}

bool ClaudeUsageService::setToken(const String& token) {
    String value = token;
    value.trim();
    if (value.length() < 20 || value.length() > MAX_TOKEN_LENGTH) return false;
    _token = value;
    _preferences.putString("token", _token);
    _valid = false;
    _authError = false;
    _refreshRequested = true;
    _version++;
    return true;
}

void ClaudeUsageService::clearToken() {
    _token = "";
    _preferences.remove("token");
    _valid = false;
    _authError = false;
    _refreshRequested = false;
    _version++;
}

void ClaudeUsageService::requestRefresh() {
    if (!hasCredential()) return;
    _refreshRequested = true;
    _authError = false;
    _version++;
}

unsigned long ClaudeUsageService::retryDelayMs() const {
    const uint8_t exponent = min<uint8_t>(_failureCount, 3);
    return RETRY_INTERVAL_MS << exponent;
}

void ClaudeUsageService::update() {
    if (_loading || !_wifiService || !_wifiService->isConnected() || !hasCredential()) return;

    const unsigned long now = millis();
    const bool failed = _lastAttemptMs != 0 &&
        (_lastSuccessMs == 0 || _lastAttemptMs > _lastSuccessMs);
    const bool retryDue = failed && now - _lastAttemptMs >= retryDelayMs();
    const bool refreshDue = !failed && _lastSuccessMs != 0 &&
        now - _lastSuccessMs >= REFRESH_INTERVAL_MS;
    if (!_refreshRequested && !retryDue && !refreshDue) return;
    if (!NetworkRequestGate::tryAcquire()) return;
    if (!MemoryMonitor::hasTlsHeadroom("ClaudeUsage")) {
        NetworkRequestGate::release();
        return;
    }

    _loading = true;
    _refreshRequested = false;
    _lastAttemptMs = now;
    _version++;
    if (xTaskCreate(refreshTaskEntry, "UsageNet", 8192, this, 1, &_refreshTask) != pdPASS) {
        _refreshTask = nullptr;
        _loading = false;
        _refreshRequested = true;
        NetworkRequestGate::release();
        LOG_WARN("ClaudeUsage", "后台请求任务创建失败");
    }
}

void ClaudeUsageService::refreshTaskEntry(void* parameter) {
    auto* service = static_cast<ClaudeUsageService*>(parameter);
    service->runRefresh();
    service->_loading = false;
    service->_refreshTask = nullptr;
    service->_version++;
    NetworkRequestGate::release();
    vTaskDelete(nullptr);
}

void ClaudeUsageService::runRefresh() {
    if (fetchUsage()) {
        _failureCount = 0;
        _lastSuccessMs = millis();
        _authError = false;
    } else if (_failureCount < 5) {
        _failureCount++;
        LOG_WARN("ClaudeUsage", "额度更新失败，将在 %lu 秒后重试", retryDelayMs() / 1000UL);
    }
}

bool ClaudeUsageService::fetchUsage() {
    WiFiClientSecure client;
    // 当前工程的其他 HTTPS 服务使用相同的 CA 配置策略；请求仍然保持 HTTPS。
    client.setInsecure();
    client.setHandshakeTimeout(15);

    HTTPClient http;
    http.setConnectTimeout(12000);
    http.setTimeout(12000);
    if (!http.begin(client, API_URL)) return false;
    http.useHTTP10(true);
    http.addHeader("Authorization", "Bearer " + _token);
    http.addHeader("anthropic-version", API_VERSION);
    http.addHeader("anthropic-beta", API_BETA);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Accept-Encoding", "identity");
    const char* responseHeaders[] = {
        "anthropic-ratelimit-unified-5h-utilization",
        "anthropic-ratelimit-unified-5h-reset",
        "anthropic-ratelimit-unified-7d-utilization",
        "anthropic-ratelimit-unified-7d-reset",
        "anthropic-ratelimit-unified-5h-status"
    };
    http.collectHeaders(responseHeaders, 5);

    const int code = http.POST("{\"model\":\"claude-haiku-4-5-20251001\",\"max_tokens\":1,\"messages\":[{\"role\":\"user\",\"content\":\"hi\"}]}");
    if (code == 401 || code == 403) {
        _authError = true;
        LOG_WARN("ClaudeUsage", "凭证失效 HTTP=%d", code);
        http.end();
        return false;
    }
    if (code < 200 || code >= 300) {
        LOG_WARN("ClaudeUsage", "额度请求失败 HTTP=%d", code);
        http.end();
        return false;
    }

    const float session = http.header("anthropic-ratelimit-unified-5h-utilization").toFloat() * 100.0f;
    const float weekly = http.header("anthropic-ratelimit-unified-7d-utilization").toFloat() * 100.0f;
    const String sessionReset = http.header("anthropic-ratelimit-unified-5h-reset");
    const String weeklyReset = http.header("anthropic-ratelimit-unified-7d-reset");
    const String status = http.header("anthropic-ratelimit-unified-5h-status");
    http.end();
    if (sessionReset.length() == 0 || weeklyReset.length() == 0) return false;

    const time_t now = time(nullptr);
    _sessionUsedPct = constrain(session, 0.0f, 100.0f);
    _weeklyUsedPct = constrain(weekly, 0.0f, 100.0f);
    _sessionResetMins = max(0, static_cast<int>((strtoll(sessionReset.c_str(), nullptr, 10) - now) / 60));
    _weeklyResetMins = max(0, static_cast<int>((strtoll(weeklyReset.c_str(), nullptr, 10) - now) / 60));
    strncpy(_status, status.length() ? status.c_str() : "ALLOWED", sizeof(_status) - 1);
    _status[sizeof(_status) - 1] = '\0';
    _valid = true;
    return true;
}

String ClaudeUsageService::getJson() const {
    JsonDocument doc;
    doc["configured"] = hasCredential();
    doc["loading"] = _loading;
    doc["valid"] = _valid;
    doc["authError"] = _authError;
    doc["status"] = _status;
    doc["sessionUsed"] = _valid ? _sessionUsedPct : -1;
    doc["sessionLeft"] = _valid ? max(0.0f, 100.0f - _sessionUsedPct) : -1;
    doc["sessionResetMins"] = _valid ? _sessionResetMins : -1;
    doc["weeklyUsed"] = _valid ? _weeklyUsedPct : -1;
    doc["weeklyLeft"] = _valid ? max(0.0f, 100.0f - _weeklyUsedPct) : -1;
    doc["weeklyResetMins"] = _valid ? _weeklyResetMins : -1;
    doc["lastSuccessAgeSec"] = _lastSuccessMs == 0 ? -1 : static_cast<long>((millis() - _lastSuccessMs) / 1000UL);
    String output;
    serializeJson(doc, output);
    return output;
}

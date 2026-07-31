#include "holiday_service.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFiClientSecure.h>

#include "../config/cfg_holiday.h"
#include "../utils/logger.h"
#include "../utils/memory_monitor.h"
#include "../utils/network_request_gate.h"

HolidayService::HolidayService(WifiConfigService* wifiService,
                               TimeService* timeService)
    : _wifiService(wifiService)
    , _timeService(timeService)
    , _loading(false)
    , _resolvedForToday(false)
    , _holidayToday(false)
    , _activeDate{0}
    , _holidayName{0}
    , _cachedDate{0}
    , _cachedHolidayName{0}
    , _cacheAvailable(false)
    , _cachedHoliday(false)
    , _lastAttemptMs(0)
    , _failureCount(0)
    , _version(0)
    , _refreshTask(nullptr)
{
}

void HolidayService::init() {
    _loading = false;
    _resolvedForToday = false;
    _holidayToday = false;
    _activeDate[0] = '\0';
    _holidayName[0] = '\0';
    _cacheAvailable = loadCache();
}

void HolidayService::update() {
    if (!_timeService || _timeService->getEpoch() <= 1000000000UL) return;

    syncActiveDate();
    if (_resolvedForToday || _loading ||
        !_wifiService || !_wifiService->isConnected()) {
        return;
    }

    const unsigned long now = millis();
    if (_lastAttemptMs != 0 &&
        now - _lastAttemptMs < retryDelayMs()) {
        return;
    }
    if (!NetworkRequestGate::tryAcquire()) return;

    _loading = true;
    _lastAttemptMs = now;
    _version++;
    if (!MemoryMonitor::hasTlsHeadroom("Holiday")) {
        _loading = false;
        NetworkRequestGate::release();
        return;
    }
    if (xTaskCreate(refreshTaskEntry, "HolidayNet", 7168, this, 1,
                    &_refreshTask) != pdPASS) {
        _refreshTask = nullptr;
        _loading = false;
        NetworkRequestGate::release();
        LOG_WARN("Holiday", "后台请求任务创建失败");
    }
}

void HolidayService::refreshTaskEntry(void* parameter) {
    HolidayService* service = static_cast<HolidayService*>(parameter);
    service->runRefresh();
    service->_loading = false;
    service->_refreshTask = nullptr;
    service->_version++;
    NetworkRequestGate::release();
    vTaskDelete(nullptr);
}

void HolidayService::runRefresh() {
    char requestedDate[DATE_SIZE];
    strncpy(requestedDate, _activeDate, sizeof(requestedDate) - 1);
    requestedDate[sizeof(requestedDate) - 1] = '\0';

    bool holiday = false;
    char name[HOLIDAY_NAME_SIZE] = {0};
    if (!fetchDate(requestedDate, holiday, name, sizeof(name))) {
        if (_failureCount < 6) _failureCount++;
        LOG_WARN("Holiday", "节假日更新失败，%lu 秒后重试",
                 retryDelayMs() / 1000UL);
        return;
    }

    // 请求跨过午夜时丢弃旧日期响应，避免短暂显示昨天的节日。
    if (strcmp(requestedDate, _activeDate) != 0) return;

    _holidayToday = holiday;
    strncpy(_holidayName, name, sizeof(_holidayName) - 1);
    _holidayName[sizeof(_holidayName) - 1] = '\0';
    _resolvedForToday = true;
    _failureCount = 0;
    saveCache(requestedDate, holiday, name);

    LOG_INFO("Holiday", "%s %s",
             requestedDate, holiday ? _holidayName : "普通日期");
}

void HolidayService::syncActiveDate() {
    char today[DATE_SIZE];
    formatToday(_timeService, today, sizeof(today));
    if (strcmp(today, _activeDate) == 0) return;

    strncpy(_activeDate, today, sizeof(_activeDate) - 1);
    _activeDate[sizeof(_activeDate) - 1] = '\0';
    _resolvedForToday = false;
    _holidayToday = false;
    _holidayName[0] = '\0';
    _lastAttemptMs = 0;
    _failureCount = 0;
    _version++;

    if (_cacheAvailable && strcmp(_cachedDate, _activeDate) == 0) {
        _holidayToday = _cachedHoliday;
        strncpy(_holidayName, _cachedHolidayName,
                sizeof(_holidayName) - 1);
        _holidayName[sizeof(_holidayName) - 1] = '\0';
        _resolvedForToday = true;
        LOG_INFO("Holiday", "使用当天缓存 %s", _activeDate);
    }
}

bool HolidayService::fetchDate(const char* date, bool& holiday,
                               char* name, size_t nameSize) {
    if (!date || date[0] == '\0' || !name || nameSize == 0) return false;

    String url = CFG_HOLIDAY_API_BASE;
    url += date;

    WiFiClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(15);

    HTTPClient http;
    http.setConnectTimeout(CFG_HOLIDAY_CONNECT_TIMEOUT_MS);
    http.setTimeout(CFG_HOLIDAY_REQUEST_TIMEOUT_MS);
    if (!http.begin(client, url)) return false;
    http.useHTTP10(true);
    http.addHeader("Accept-Encoding", "identity");

    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        const String reason = HTTPClient::errorToString(code);
        LOG_WARN("Holiday", "API 请求失败 HTTP=%d reason=%s",
                 code, reason.c_str());
        http.end();
        return false;
    }

    const String payload = http.getString();
    http.end();
    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, payload);
    if (error || (doc["code"] | -1) != 0) {
        LOG_WARN("Holiday", "API 响应解析失败 json=%s bytes=%u",
                 error ? error.c_str() : "invalid code",
                 static_cast<unsigned int>(payload.length()));
        return false;
    }

    JsonVariant holidayNode = doc["holiday"];
    const int dayType = doc["type"]["type"] | -1;
    // API 也会为普通周末返回 holiday 对象；只有 type=2 才是中国
    // 法定节假日。周末(type=1)和补班日保持默认时钟布局。
    holiday = dayType == 2 &&
              !holidayNode.isNull() &&
              (holidayNode["holiday"] | false);
    name[0] = '\0';
    if (!holiday) return true;

    const char* apiName = holidayNode["target"] | "";
    if (!apiName || apiName[0] == '\0') {
        apiName = holidayNode["name"] | "";
    }
    translateHolidayName(apiName, name, nameSize);
    return true;
}

bool HolidayService::loadCache() {
    File file = LittleFS.open(CFG_HOLIDAY_CACHE_PATH, "r");
    if (!file) return false;

    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) {
        LOG_WARN("Holiday", "缓存解析失败: %s", error.c_str());
        return false;
    }

    const char* date = doc["date"] | "";
    const char* name = doc["name"] | "";
    if (strlen(date) != DATE_SIZE - 1) return false;

    strncpy(_cachedDate, date, sizeof(_cachedDate) - 1);
    _cachedDate[sizeof(_cachedDate) - 1] = '\0';
    _cachedHoliday = doc["holiday"] | false;
    strncpy(_cachedHolidayName, name, sizeof(_cachedHolidayName) - 1);
    _cachedHolidayName[sizeof(_cachedHolidayName) - 1] = '\0';
    return true;
}

void HolidayService::saveCache(const char* date, bool holiday,
                               const char* name) {
    JsonDocument doc;
    doc["date"] = date;
    doc["holiday"] = holiday;
    doc["name"] = name ? name : "";

    File file = LittleFS.open(CFG_HOLIDAY_CACHE_PATH, "w");
    if (!file) {
        LOG_WARN("Holiday", "缓存文件打开失败");
        return;
    }
    if (serializeJson(doc, file) == 0) {
        LOG_WARN("Holiday", "缓存写入失败");
    }
    file.close();

    strncpy(_cachedDate, date, sizeof(_cachedDate) - 1);
    _cachedDate[sizeof(_cachedDate) - 1] = '\0';
    _cachedHoliday = holiday;
    strncpy(_cachedHolidayName, name ? name : "",
            sizeof(_cachedHolidayName) - 1);
    _cachedHolidayName[sizeof(_cachedHolidayName) - 1] = '\0';
    _cacheAvailable = true;
}

unsigned long HolidayService::retryDelayMs() const {
    if (_failureCount == 0) return CFG_HOLIDAY_FIRST_RETRY_MS;
    const uint8_t shift = min<uint8_t>(_failureCount - 1, 6);
    const unsigned long delayMs = CFG_HOLIDAY_FIRST_RETRY_MS << shift;
    return min<unsigned long>(delayMs, CFG_HOLIDAY_MAX_RETRY_MS);
}

void HolidayService::formatToday(TimeService* timeService,
                                 char* output, size_t size) {
    if (!output || size == 0) return;
    snprintf(output, size, "%04d-%02d-%02d",
             timeService->getYear(),
             timeService->getMonth(),
             timeService->getDay());
}

void HolidayService::translateHolidayName(const char* apiName,
                                          char* output, size_t size) {
    if (!output || size == 0) return;

    const char* translated = "PUBLIC HOLIDAY";
    if (apiName) {
        if (strstr(apiName, "元旦")) translated = "NEW YEAR'S DAY";
        else if (strstr(apiName, "春节") || strstr(apiName, "除夕")) {
            translated = "SPRING FESTIVAL";
        } else if (strstr(apiName, "清明")) {
            translated = "QINGMING FEST";
        } else if (strstr(apiName, "劳动")) {
            translated = "LABOUR DAY";
        } else if (strstr(apiName, "端午")) {
            translated = "DRAGON BOAT FEST";
        } else if (strstr(apiName, "中秋")) {
            translated = "MID-AUTUMN FEST";
        } else if (strstr(apiName, "国庆")) {
            translated = "NATIONAL DAY";
        }
    }
    strncpy(output, translated, size - 1);
    output[size - 1] = '\0';
}

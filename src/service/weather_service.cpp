#include "weather_service.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <math.h>

#include "../utils/logger.h"
#include "../utils/memory_monitor.h"
#include "../utils/network_request_gate.h"

WeatherService::WeatherService(WifiConfigService* wifiService)
    : _wifiService(wifiService)
    , _valid(false)
    , _loading(false)
    , _locationValid(false)
    , _locationOverride(false)
    , _refreshRequested(false)
    , _latitude(0.0f)
    , _longitude(0.0f)
    , _fallbackLatitude(0.0f)
    , _fallbackLongitude(0.0f)
    , _fallbackSource(LocationSource::MANUAL)
    , _locationSource(LocationSource::IP)
    , _temperature(0)
    , _highTemperature(0)
    , _lowTemperature(0)
    , _humidity(0)
    , _weatherCode(0)
    , _lastWeatherMs(0)
    , _lastLocationMs(0)
    , _lastAttemptMs(0)
    , _failureCount(0)
    , _version(0)
    , _refreshTask(nullptr)
{
    strncpy(_city, "LOCATING", sizeof(_city) - 1);
    _city[sizeof(_city) - 1] = '\0';
    strncpy(_locationLabel, "LOCATING", sizeof(_locationLabel) - 1);
    _locationLabel[sizeof(_locationLabel) - 1] = '\0';
    _fallbackCity[0] = '\0';
}

void WeatherService::init() {
    _prefs.begin("clawd-weather", false);
    const uint8_t mode = _prefs.getUChar("mode", 0);
    _locationOverride = mode == 1 || mode == 2;
    _fallbackLatitude = _prefs.getFloat("lat", 0.0f);
    _fallbackLongitude = _prefs.getFloat("lon", 0.0f);
    const String savedCity = _prefs.getString("city", "");
    strncpy(_fallbackCity, savedCity.c_str(), sizeof(_fallbackCity) - 1);
    _fallbackCity[sizeof(_fallbackCity) - 1] = '\0';
    strncpy(_locationLabel, _fallbackCity[0] ? _fallbackCity : "LOCATING",
            sizeof(_locationLabel) - 1);
    _locationLabel[sizeof(_locationLabel) - 1] = '\0';
    const uint8_t fallbackMode = _prefs.getUChar("src", mode);
    _fallbackSource = fallbackMode == 1 ? LocationSource::GPS : LocationSource::MANUAL;
    if (_locationOverride && !_fallbackLatitude && !_fallbackLongitude) {
        _locationOverride = false;
    }
    _refreshRequested = false;
    _loading = false;
}

void WeatherService::requestRefresh() {
    _refreshRequested = true;
    _version++;
}

bool WeatherService::setLocationOverride(float latitude, float longitude,
                                          const String& city,
                                          LocationSource source) {
    if (latitude < -90.0f || latitude > 90.0f || longitude < -180.0f ||
        longitude > 180.0f || (latitude == 0.0f && longitude == 0.0f) ||
        city.length() == 0 || city.length() >= sizeof(_fallbackCity) ||
        source == LocationSource::IP) {
        return false;
    }
    _fallbackLatitude = latitude;
    _fallbackLongitude = longitude;
    strncpy(_fallbackCity, city.c_str(), sizeof(_fallbackCity) - 1);
    _fallbackCity[sizeof(_fallbackCity) - 1] = '\0';
    strncpy(_locationLabel, _fallbackCity, sizeof(_locationLabel) - 1);
    _locationLabel[sizeof(_locationLabel) - 1] = '\0';
    _fallbackSource = source;
    _locationOverride = true;
    _prefs.putUChar("mode", source == LocationSource::GPS ? 1 : 2);
    _prefs.putUChar("src", source == LocationSource::GPS ? 1 : 2);
    _prefs.putFloat("lat", latitude);
    _prefs.putFloat("lon", longitude);
    _prefs.putString("city", city);
    _locationValid = false;
    requestRefresh();
    return true;
}

void WeatherService::clearLocationOverride() {
    _locationOverride = false;
    _prefs.putUChar("mode", 0);
    _locationValid = false;
    requestRefresh();
}

const char* WeatherService::getLocationSourceName() const {
    switch (_locationSource) {
        case LocationSource::GPS: return "gps";
        case LocationSource::MANUAL: return "manual";
        case LocationSource::IP:
        default: return "ip";
    }
}

void WeatherService::update() {
    if (_loading || !_wifiService || !_wifiService->isConnected()) return;

    const unsigned long now = millis();
    const bool firstAttempt = _lastAttemptMs == 0;
    const bool lastAttemptFailed = !firstAttempt &&
        (!_valid || _lastAttemptMs > _lastWeatherMs);
    const bool retryDue = lastAttemptFailed &&
        now - _lastAttemptMs >= retryDelayMs();
    const bool weatherDue = _valid && !lastAttemptFailed &&
        now - _lastWeatherMs >= WEATHER_REFRESH_MS;
    if (!_refreshRequested && !firstAttempt && !retryDue && !weatherDue) return;

    if (!NetworkRequestGate::tryAcquire()) return;

    _loading = true;
    _refreshRequested = false;
    _lastAttemptMs = now;
    _version++;
    if (!MemoryMonitor::hasTlsHeadroom("Weather")) {
        _loading = false;
        _version++;
        NetworkRequestGate::release();
        return;
    }
    if (xTaskCreate(refreshTaskEntry, "WeatherNet", 8192, this, 1,
                    &_refreshTask) != pdPASS) {
        _refreshTask = nullptr;
        _loading = false;
        _refreshRequested = true;
        NetworkRequestGate::release();
        LOG_WARN("Weather", "后台请求任务创建失败");
    }
}

void WeatherService::refreshTaskEntry(void* parameter) {
    WeatherService* service = static_cast<WeatherService*>(parameter);
    service->runRefresh();
    service->_loading = false;
    service->_refreshTask = nullptr;
    service->_version++;
    NetworkRequestGate::release();
    vTaskDelete(nullptr);
}

void WeatherService::runRefresh() {
    const unsigned long now = millis();
    const bool locationDue = !_locationValid ||
        _lastLocationMs == 0 ||
        now - _lastLocationMs >= LOCATION_REFRESH_MS;
    bool ok = true;
    if (locationDue) ok = fetchLocation();
    if (ok && _locationValid) ok = fetchWeather();

    if (ok) {
        _failureCount = 0;
    } else {
        if (_failureCount < 5) _failureCount++;
        LOG_WARN("Weather", "天气更新失败，%lu 秒后重试",
                 retryDelayMs() / 1000UL);
    }
}

bool WeatherService::fetchLocation() {
    if (_locationOverride) return applyFallbackLocation();
    WiFiClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(15);

    HTTPClient http;
    http.setConnectTimeout(12000);
    http.setTimeout(10000);
    if (!http.begin(client, "https://ipwho.is/")) return applyFallbackLocation();
    http.useHTTP10(true);
    http.addHeader("Accept-Encoding", "identity");

    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        const String reason = HTTPClient::errorToString(code);
        LOG_WARN("Weather", "IP 定位请求失败 HTTP=%d reason=%s",
                 code, reason.c_str());
        http.end();
        return applyFallbackLocation();
    }

    const String payload = http.getString();
    http.end();
    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, payload);
    if (error || !doc["success"].as<bool>()) {
        LOG_WARN("Weather", "IP 定位响应解析失败 json=%s bytes=%u",
                 error ? error.c_str() : "invalid response",
                 static_cast<unsigned int>(payload.length()));
        return applyFallbackLocation();
    }

    _latitude = doc["latitude"] | 0.0f;
    _longitude = doc["longitude"] | 0.0f;
    const char* city = doc["city"] | "LOCAL";
    if (_latitude == 0.0f && _longitude == 0.0f) return applyFallbackLocation();

    copyCity(city);
    strncpy(_locationLabel, city, sizeof(_locationLabel) - 1);
    _locationLabel[sizeof(_locationLabel) - 1] = '\0';
    _locationValid = true;
    _locationSource = LocationSource::IP;
    _lastLocationMs = millis();
    LOG_INFO("Weather", "IP 定位城市=%s lat=%.3f lon=%.3f",
             _city, _latitude, _longitude);
    return true;
}

bool WeatherService::applyFallbackLocation() {
    if (_fallbackLatitude < -90.0f || _fallbackLatitude > 90.0f ||
        _fallbackLongitude < -180.0f || _fallbackLongitude > 180.0f ||
        (_fallbackLatitude == 0.0f && _fallbackLongitude == 0.0f) ||
        _fallbackCity[0] == '\0') {
        return false;
    }
    _latitude = _fallbackLatitude;
    _longitude = _fallbackLongitude;
    copyCity(_fallbackCity);
    strncpy(_locationLabel, _fallbackCity, sizeof(_locationLabel) - 1);
    _locationLabel[sizeof(_locationLabel) - 1] = '\0';
    _locationSource = _fallbackSource;
    _locationValid = true;
    _lastLocationMs = millis();
    LOG_INFO("Weather", "使用备用定位 source=%s city=%s lat=%.3f lon=%.3f",
             getLocationSourceName(), _city, _latitude, _longitude);
    return true;
}

bool WeatherService::fetchWeather() {
    String url = "https://api.open-meteo.com/v1/forecast?latitude=";
    url += String(_latitude, 4);
    url += "&longitude=";
    url += String(_longitude, 4);
    url += "&current=temperature_2m,relative_humidity_2m,weather_code";
    url += "&daily=temperature_2m_max,temperature_2m_min";
    url += "&timezone=auto&forecast_days=1";

    WiFiClientSecure client;
    client.setInsecure();
    client.setHandshakeTimeout(15);

    HTTPClient http;
    http.setConnectTimeout(12000);
    http.setTimeout(10000);
    if (!http.begin(client, url)) return false;
    http.useHTTP10(true);
    http.addHeader("Accept-Encoding", "identity");

    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        const String reason = HTTPClient::errorToString(code);
        LOG_WARN("Weather", "天气请求失败 HTTP=%d reason=%s",
                 code, reason.c_str());
        http.end();
        return false;
    }

    const String payload = http.getString();
    http.end();
    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        LOG_WARN("Weather", "天气 JSON 解析失败 json=%s bytes=%u",
                 error.c_str(), static_cast<unsigned int>(payload.length()));
        return false;
    }

    const bool missingCurrent = doc["current"]["temperature_2m"].isNull();
    const bool missingHigh = doc["daily"]["temperature_2m_max"][0].isNull();
    const bool missingLow = doc["daily"]["temperature_2m_min"][0].isNull();
    if (missingCurrent || missingHigh || missingLow) {
        LOG_WARN("Weather", "天气响应缺少字段 current=%d high=%d low=%d bytes=%u",
                 missingCurrent, missingHigh, missingLow,
                 static_cast<unsigned int>(payload.length()));
        return false;
    }

    _temperature = lroundf(doc["current"]["temperature_2m"].as<float>());
    _humidity = constrain(doc["current"]["relative_humidity_2m"].as<int>(), 0, 100);
    _weatherCode = doc["current"]["weather_code"].as<int>();
    _highTemperature = lroundf(doc["daily"]["temperature_2m_max"][0].as<float>());
    _lowTemperature = lroundf(doc["daily"]["temperature_2m_min"][0].as<float>());
    _valid = true;
    _lastWeatherMs = millis();
    LOG_INFO("Weather", "%s %dC H%d L%d HUM%d%% code=%d",
             _city, _temperature, _highTemperature, _lowTemperature,
             _humidity, _weatherCode);
    return true;
}

unsigned long WeatherService::retryDelayMs() const {
    if (_failureCount == 0) return FIRST_RETRY_MS;
    const uint8_t shift = min<uint8_t>(_failureCount - 1, 3);
    const unsigned long delayMs = FIRST_RETRY_MS << shift;
    return min<unsigned long>(delayMs, MAX_RETRY_MS);
}

void WeatherService::copyCity(const char* city) {
    if (!city || city[0] == '\0') city = "LOCAL";
    size_t out = 0;
    for (size_t i = 0; city[i] != '\0' && out < sizeof(_city) - 1; i++) {
        const unsigned char c = static_cast<unsigned char>(city[i]);
        if (c >= 'a' && c <= 'z') {
            _city[out++] = static_cast<char>(c - 'a' + 'A');
        } else if ((c >= 'A' && c <= 'Z') || c == ' ' || c == '-') {
            _city[out++] = static_cast<char>(c);
        }
    }
    if (out == 0) {
        strncpy(_city, "LOCAL", sizeof(_city) - 1);
        out = strlen(_city);
    }
    _city[out] = '\0';
}

const char* WeatherService::getCondition() const {
    if (_weatherCode == 0) return "CLEAR";
    if (_weatherCode <= 2) return "PARTLY";
    if (_weatherCode == 3) return "CLOUDY";
    if (_weatherCode == 45 || _weatherCode == 48) return "FOG";
    if (_weatherCode >= 51 && _weatherCode <= 57) return "DRIZZLE";
    if ((_weatherCode >= 61 && _weatherCode <= 67) ||
        (_weatherCode >= 80 && _weatherCode <= 82)) return "RAIN";
    if ((_weatherCode >= 71 && _weatherCode <= 77) ||
        (_weatherCode >= 85 && _weatherCode <= 86)) return "SNOW";
    if (_weatherCode >= 95) return "STORM";
    return "WEATHER";
}

#include "weather_service.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <math.h>

#include "../utils/logger.h"

WeatherService::WeatherService(WifiConfigService* wifiService)
    : _wifiService(wifiService)
    , _valid(false)
    , _loading(false)
    , _locationValid(false)
    , _refreshRequested(false)
    , _latitude(0.0f)
    , _longitude(0.0f)
    , _temperature(0)
    , _highTemperature(0)
    , _lowTemperature(0)
    , _humidity(0)
    , _weatherCode(0)
    , _lastWeatherMs(0)
    , _lastLocationMs(0)
    , _lastAttemptMs(0)
    , _version(0)
{
    strncpy(_city, "LOCATING", sizeof(_city) - 1);
    _city[sizeof(_city) - 1] = '\0';
}

void WeatherService::init() {
    _refreshRequested = false;
    _loading = false;
}

void WeatherService::requestRefresh() {
    _refreshRequested = true;
    _version++;
}

void WeatherService::update() {
    if (_loading || !_wifiService || !_wifiService->isConnected()) return;

    const unsigned long now = millis();
    const bool retryDue = !_valid &&
        (_lastAttemptMs == 0 || now - _lastAttemptMs >= RETRY_INTERVAL_MS);
    const bool weatherDue = _valid && now - _lastWeatherMs >= WEATHER_REFRESH_MS;
    if (!_refreshRequested && !retryDue && !weatherDue) return;

    _loading = true;
    _refreshRequested = false;
    _lastAttemptMs = now;
    _version++;

    const bool locationDue = !_locationValid ||
        _lastLocationMs == 0 ||
        now - _lastLocationMs >= LOCATION_REFRESH_MS;
    bool ok = true;
    if (locationDue) ok = fetchLocation();
    if (ok && _locationValid) ok = fetchWeather();

    _loading = false;
    if (!ok) LOG_WARN("Weather", "天气更新失败，将稍后重试");
    _version++;
}

bool WeatherService::fetchLocation() {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(7000);
    if (!http.begin(client, "https://ipwho.is/")) return false;
    http.useHTTP10(true);
    http.addHeader("Accept-Encoding", "identity");

    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        LOG_WARN("Weather", "IP 定位请求失败 HTTP=%d", code);
        http.end();
        return false;
    }

    const String payload = http.getString();
    http.end();
    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, payload);
    if (error || !doc["success"].as<bool>()) {
        LOG_WARN("Weather", "IP 定位响应解析失败 json=%s bytes=%u",
                 error ? error.c_str() : "invalid response",
                 static_cast<unsigned int>(payload.length()));
        return false;
    }

    _latitude = doc["latitude"] | 0.0f;
    _longitude = doc["longitude"] | 0.0f;
    const char* city = doc["city"] | "LOCAL";
    if (_latitude == 0.0f && _longitude == 0.0f) return false;

    copyCity(city);
    _locationValid = true;
    _lastLocationMs = millis();
    LOG_INFO("Weather", "IP 定位城市=%s lat=%.3f lon=%.3f",
             _city, _latitude, _longitude);
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

    HTTPClient http;
    http.setTimeout(7000);
    if (!http.begin(client, url)) return false;
    http.useHTTP10(true);
    http.addHeader("Accept-Encoding", "identity");

    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        LOG_WARN("Weather", "天气请求失败 HTTP=%d", code);
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

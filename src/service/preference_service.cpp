#include "preference_service.h"
#include "time_service.h"
#include "../config/cfg_display.h"

void PreferenceService::init() {
    _prefs.begin("clawd-prefs", false);
    _defaultBgHex = _prefs.getString("bg", _defaultBgHex);
    if (!isValidHexColor(_defaultBgHex)) _defaultBgHex = "#aa4818";
    _animSpeed = constrain(_prefs.getUChar("speed", _animSpeed), 1, 3);
    _startupView = _prefs.getUChar("startup", _startupView);
    if (!isStartupViewAllowed(_startupView)) _startupView = VIEW_EYES_NORMAL;
    _brightnessPercent = constrain(_prefs.getUChar("bright", _brightnessPercent), 0, 100);
    _claudeStatusEnabled = _prefs.getBool("ccstatus", _claudeStatusEnabled);
    _displayTheme = _prefs.getUChar("theme", _displayTheme);
    if (_displayTheme != THEME_ORANGE_BLACK &&
        _displayTheme != THEME_ORANGE_WHITE) {
        _displayTheme = THEME_ORANGE_WHITE;
    }
    _carouselEnabled = _prefs.getBool("carousel", _carouselEnabled);
    _carouselSpeedSeconds = constrain(
        _prefs.getUChar("cspeed", _carouselSpeedSeconds), 5, 60);
    _carouselFixedView = _prefs.getUChar("cfixed", _carouselFixedView);
    if (!isCarouselView(_carouselFixedView)) _carouselFixedView = VIEW_WEATHER;
    const String storedOrder = _prefs.getString("corder", "");
    if (!storedOrder.isEmpty()) {
        uint8_t order[CAROUSEL_VIEW_COUNT] = {};
        uint8_t index = 0;
        int start = 0;
        while (index < CAROUSEL_VIEW_COUNT && start >= 0) {
            const int comma = storedOrder.indexOf(',', start);
            const String part = storedOrder.substring(start,
                comma < 0 ? storedOrder.length() : comma);
            order[index++] = static_cast<uint8_t>(part.toInt());
            start = comma < 0 ? -1 : comma + 1;
        }
        // 兼容旧版三页轮播配置，保留原顺序并把时钟追加到末尾。
        if (index == CAROUSEL_VIEW_COUNT - 1) {
            order[index++] = VIEW_CLOCK;
        }
        if (index == CAROUSEL_VIEW_COUNT) setCarouselOrder(order);
    }
    _nightDimEnabled = _prefs.getBool("night", _nightDimEnabled);
    _nightStartHour = constrain(_prefs.getUChar("nstart", _nightStartHour), 0, 23);
    _nightEndHour = constrain(_prefs.getUChar("nend", _nightEndHour), 0, 23);
    _nightBrightnessPercent = constrain(_prefs.getUChar("nbright", _nightBrightnessPercent), 0, 100);
}

void PreferenceService::setDefaultBgHex(const String& hex) {
    if (!isValidHexColor(hex)) return;
    _defaultBgHex = hex;
    _prefs.putString("bg", _defaultBgHex);
}

void PreferenceService::setAnimSpeed(uint8_t speed) {
    _animSpeed = constrain(speed, 1, 3);
    _prefs.putUChar("speed", _animSpeed);
}

void PreferenceService::setStartupView(uint8_t view) {
    if (!isStartupViewAllowed(view)) view = VIEW_EYES_NORMAL;
    _startupView = view;
    _prefs.putUChar("startup", _startupView);
}

void PreferenceService::setBrightnessPercent(uint8_t percent) {
    _brightnessPercent = constrain(percent, 0, 100);
    _prefs.putUChar("bright", _brightnessPercent);
}

void PreferenceService::setClaudeStatusEnabled(bool enabled) {
    _claudeStatusEnabled = enabled;
    _prefs.putBool("ccstatus", _claudeStatusEnabled);
}

void PreferenceService::setDisplayTheme(uint8_t theme) {
    if (theme != THEME_ORANGE_BLACK && theme != THEME_ORANGE_WHITE) return;
    _displayTheme = theme;
    _prefs.putUChar("theme", _displayTheme);
}

void PreferenceService::setCarouselEnabled(bool enabled) {
    _carouselEnabled = enabled;
    _prefs.putBool("carousel", _carouselEnabled);
}

void PreferenceService::setCarouselSpeedSeconds(uint8_t seconds) {
    _carouselSpeedSeconds = constrain(seconds, 5, 60);
    _prefs.putUChar("cspeed", _carouselSpeedSeconds);
}

uint8_t PreferenceService::getCarouselView(uint8_t index) const {
    return index < CAROUSEL_VIEW_COUNT ? _carouselOrder[index] : VIEW_WEATHER;
}

bool PreferenceService::setCarouselOrder(
    const uint8_t order[CAROUSEL_VIEW_COUNT]) {
    if (!order) return false;
    bool seenClock = false, seenWeather = false;
    bool seenCrypto = false, seenMarket = false;
    for (uint8_t i = 0; i < CAROUSEL_VIEW_COUNT; i++) {
        if (order[i] == VIEW_CLOCK) seenClock = true;
        else if (order[i] == VIEW_WEATHER) seenWeather = true;
        else if (order[i] == VIEW_CRYPTO) seenCrypto = true;
        else if (order[i] == VIEW_MARKET) seenMarket = true;
        else return false;
    }
    if (!seenClock || !seenWeather || !seenCrypto || !seenMarket) return false;
    memcpy(_carouselOrder, order, sizeof(_carouselOrder));
    String serialized = String(_carouselOrder[0]) + "," + String(_carouselOrder[1]) +
                        "," + String(_carouselOrder[2]) + "," +
                        String(_carouselOrder[3]);
    _prefs.putString("corder", serialized);
    return true;
}

void PreferenceService::setCarouselFixedView(uint8_t view) {
    if (!isCarouselView(view)) return;
    _carouselFixedView = view;
    _prefs.putUChar("cfixed", _carouselFixedView);
}

void PreferenceService::setNightDimEnabled(bool enabled) {
    _nightDimEnabled = enabled;
    _prefs.putBool("night", _nightDimEnabled);
}

void PreferenceService::setNightHours(uint8_t startHour, uint8_t endHour) {
    _nightStartHour = constrain(startHour, 0, 23);
    _nightEndHour = constrain(endHour, 0, 23);
    _prefs.putUChar("nstart", _nightStartHour);
    _prefs.putUChar("nend", _nightEndHour);
}

void PreferenceService::setNightBrightnessPercent(uint8_t percent) {
    _nightBrightnessPercent = constrain(percent, 0, 100);
    _prefs.putUChar("nbright", _nightBrightnessPercent);
}

bool PreferenceService::isNightDimActive(TimeService* timeService) const {
    if (!_nightDimEnabled || !timeService || !timeService->isSynced()) return false;
    const uint8_t hour = constrain(timeService->getHour(), 0, 23);
    if (_nightStartHour == _nightEndHour) return false;
    if (_nightStartHour < _nightEndHour) {
        return hour >= _nightStartHour && hour < _nightEndHour;
    }
    return hour >= _nightStartHour || hour < _nightEndHour;
}

String PreferenceService::getJson() const {
    String json = "{";
    json += "\"bg\":\"" + _defaultBgHex + "\"";
    json += ",\"speed\":" + String(_animSpeed);
    json += ",\"startup\":" + String(_startupView);
    json += ",\"brightness\":" + String(_brightnessPercent);
    json += ",\"claudeStatus\":" + String(_claudeStatusEnabled ? "true" : "false");
    json += ",\"theme\":" + String(_displayTheme);
    json += ",\"carousel\":" + String(_carouselEnabled ? "true" : "false");
    json += ",\"carouselSpeed\":" + String(_carouselSpeedSeconds);
    json += ",\"carouselOrder\":[" + String(_carouselOrder[0]) + "," +
            String(_carouselOrder[1]) + "," + String(_carouselOrder[2]) + "," +
            String(_carouselOrder[3]) + "]";
    json += ",\"carouselFixed\":" + String(_carouselFixedView);
    json += ",\"nightDim\":" + String(_nightDimEnabled ? "true" : "false");
    json += ",\"nightStart\":" + String(_nightStartHour);
    json += ",\"nightEnd\":" + String(_nightEndHour);
    json += ",\"nightBrightness\":" + String(_nightBrightnessPercent);
    json += "}";
    return json;
}

bool PreferenceService::isValidHexColor(const String& hex) const {
    if (hex.length() != 7 || hex[0] != '#') return false;
    for (int i = 1; i < 7; i++) {
        const char c = hex[i];
        if (!isxdigit((unsigned char)c)) return false;
    }
    return true;
}

bool PreferenceService::isStartupViewAllowed(uint8_t view) const {
    return view == VIEW_EYES_NORMAL ||
           view == VIEW_EYES_SQUISH ||
           view == VIEW_CLOCK ||
           view == VIEW_POMODORO;
}

bool PreferenceService::isCarouselView(uint8_t view) const {
    return view == VIEW_CLOCK || view == VIEW_WEATHER ||
           view == VIEW_CRYPTO || view == VIEW_MARKET;
}

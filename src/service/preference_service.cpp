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

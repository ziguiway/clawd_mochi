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
    _dinoHighScore = _prefs.getUInt("dinohi", _dinoHighScore);
    _sokobanLevel = constrain(_prefs.getUChar("sokolevel", _sokobanLevel), 0, 7);
    _sokobanCompletedMask = _prefs.getUInt("sokomask", _sokobanCompletedMask);
    _tetrisHighScore = _prefs.getUInt("tetrishi", _tetrisHighScore);
    _snakeHighScore = _prefs.getUInt("snakehi", _snakeHighScore);
    _game2048BestScore = _prefs.getUInt("2048best", _game2048BestScore);
    _breakoutHighScore = _prefs.getUInt("breakhi", _breakoutHighScore);
    _displayTheme = _prefs.getUChar("theme", _displayTheme);
    if (_displayTheme >= THEME_COUNT) {
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
        // 兼容旧版四页轮播配置，保留原顺序并把 Live Ledger 追加到末尾。
        if (index == CAROUSEL_VIEW_COUNT - 1) {
            order[index++] = VIEW_SALARY;
        }
        if (index == CAROUSEL_VIEW_COUNT) setCarouselOrder(order);
    }
    _nightDimEnabled = _prefs.getBool("night", _nightDimEnabled);
    _nightStartHour = constrain(_prefs.getUChar("nstart", _nightStartHour), 0, 23);
    _nightEndHour = constrain(_prefs.getUChar("nend", _nightEndHour), 0, 23);
    _nightBrightnessPercent = constrain(_prefs.getUChar("nbright", _nightBrightnessPercent), 0, 100);
    _salaryAutoEnabled = _prefs.getBool("yauto", _salaryAutoEnabled);
    _salaryStartMinutes = constrain(
        _prefs.getUShort("ystart", _salaryStartMinutes), 0, 1439);
    _salaryEndMinutes = constrain(
        _prefs.getUShort("yend", _salaryEndMinutes), 1, 1440);
    if (_salaryStartMinutes >= _salaryEndMinutes) {
        _salaryStartMinutes = 9 * 60 + 30;
        _salaryEndMinutes = 19 * 60;
    }
    _salaryLastAutoDate = _prefs.getUInt("ydate", 0);
    _salaryLastAutoEndDate = _prefs.getUInt("yenddate", 0);

    _deviceName = _prefs.getString("devname", _deviceName);
    if (!isValidProfileText(_deviceName, 12, false)) _deviceName = "MOCHI";
    _bootLine1 = _prefs.getString("boot1", _bootLine1);
    if (!isValidProfileText(_bootLine1, 16, true)) _bootLine1 = "HELLO";
    _bootLine2 = _prefs.getString("boot2", _bootLine2);
    if (!isValidProfileText(_bootLine2, 16, true)) _bootLine2 = "MOCHI";
    ExpressionId storedExpression;
    if (!expressionIdFromName(_prefs.getString("defexpr", "normal"),
                              storedExpression)) {
        storedExpression = ExpressionId::NORMAL;
    }
    _defaultExpression = storedExpression;
    _expressionMode = _prefs.getString("exprmode", "manual") == "auto"
        ? ExpressionMode::AUTO : ExpressionMode::MANUAL;
}

bool PreferenceService::isValidProfileText(const String& value,
                                           size_t maxLength,
                                           bool allowEmpty) {
    if ((!allowEmpty && value.isEmpty()) || value.length() > maxLength) return false;
    for (size_t i = 0; i < value.length(); i++) {
        const uint8_t c = static_cast<uint8_t>(value[i]);
        if (c < 0x20 || c > 0x7E) return false;
    }
    return true;
}

bool PreferenceService::setDeviceName(const String& value) {
    if (!isValidProfileText(value, 12, false)) return false;
    _deviceName = value;
    return _prefs.putString("devname", value) > 0;
}

bool PreferenceService::setBootLine1(const String& value) {
    if (!isValidProfileText(value, 16, true)) return false;
    _bootLine1 = value;
    return _prefs.putString("boot1", value) > 0 || value.isEmpty();
}

bool PreferenceService::setBootLine2(const String& value) {
    if (!isValidProfileText(value, 16, true)) return false;
    _bootLine2 = value;
    return _prefs.putString("boot2", value) > 0 || value.isEmpty();
}

bool PreferenceService::setDefaultExpression(ExpressionId value) {
    if (static_cast<uint8_t>(value) >= EXPRESSION_COUNT) return false;
    _defaultExpression = value;
    return _prefs.putString("defexpr", expressionIdToName(value)) > 0;
}

bool PreferenceService::setExpressionMode(ExpressionMode value) {
    _expressionMode = value;
    return _prefs.putString("exprmode",
        value == ExpressionMode::AUTO ? "auto" : "manual") > 0;
}

void PreferenceService::resetProfile() {
    _deviceName = "MOCHI";
    _bootLine1 = "HELLO";
    _bootLine2 = "MOCHI";
    _defaultExpression = ExpressionId::NORMAL;
    _expressionMode = ExpressionMode::MANUAL;
    _prefs.remove("devname");
    _prefs.remove("boot1");
    _prefs.remove("boot2");
    _prefs.remove("defexpr");
    _prefs.remove("exprmode");
}

String PreferenceService::getProfileJson() const {
    auto escaped = [](const String& value) {
        String result;
        result.reserve(value.length() + 4);
        for (size_t i = 0; i < value.length(); i++) {
            if (value[i] == '"' || value[i] == '\\') result += '\\';
            result += value[i];
        }
        return result;
    };
    String json = "{\"deviceName\":\"" + escaped(_deviceName);
    json += "\",\"bootLine1\":\"" + escaped(_bootLine1);
    json += "\",\"bootLine2\":\"" + escaped(_bootLine2);
    json += "\",\"defaultExpression\":\"" +
        String(expressionIdToName(_defaultExpression));
    json += "\",\"expressionMode\":\"";
    json += _expressionMode == ExpressionMode::AUTO ? "auto" : "manual";
    return json + "\"}";
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
    if (_startupView == view) return;
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

void PreferenceService::setDinoHighScore(uint32_t score) {
    if (score <= _dinoHighScore) return;
    _dinoHighScore = score;
    _prefs.putUInt("dinohi", _dinoHighScore);
}

void PreferenceService::setSokobanLevel(uint8_t level) {
    _sokobanLevel = constrain(level, 0, 7);
    _prefs.putUChar("sokolevel", _sokobanLevel);
}

void PreferenceService::setSokobanCompletedMask(uint32_t mask) {
    _sokobanCompletedMask = mask;
    _prefs.putUInt("sokomask", _sokobanCompletedMask);
}

void PreferenceService::setTetrisHighScore(uint32_t score) {
    if (score <= _tetrisHighScore) return;
    _tetrisHighScore = score;
    _prefs.putUInt("tetrishi", score);
}

void PreferenceService::setSnakeHighScore(uint32_t score) {
    if (score <= _snakeHighScore) return;
    _snakeHighScore = score;
    _prefs.putUInt("snakehi", score);
}

void PreferenceService::setGame2048BestScore(uint32_t score) {
    if (score <= _game2048BestScore) return;
    _game2048BestScore = score;
    _prefs.putUInt("2048best", score);
}

void PreferenceService::setBreakoutHighScore(uint32_t score) {
    if (score <= _breakoutHighScore) return;
    _breakoutHighScore = score;
    _prefs.putUInt("breakhi", score);
}

void PreferenceService::setDisplayTheme(uint8_t theme) {
    if (theme >= THEME_COUNT) return;
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
    bool seenCrypto = false, seenMarket = false, seenSalary = false;
    for (uint8_t i = 0; i < CAROUSEL_VIEW_COUNT; i++) {
        if (order[i] == VIEW_CLOCK) seenClock = true;
        else if (order[i] == VIEW_WEATHER) seenWeather = true;
        else if (order[i] == VIEW_CRYPTO) seenCrypto = true;
        else if (order[i] == VIEW_MARKET) seenMarket = true;
        else if (order[i] == VIEW_SALARY) seenSalary = true;
        else return false;
    }
    if (!seenClock || !seenWeather || !seenCrypto || !seenMarket ||
        !seenSalary) {
        return false;
    }
    memcpy(_carouselOrder, order, sizeof(_carouselOrder));
    String serialized = String(_carouselOrder[0]) + "," + String(_carouselOrder[1]) +
                        "," + String(_carouselOrder[2]) + "," +
                        String(_carouselOrder[3]) + "," +
                        String(_carouselOrder[4]);
    _prefs.putString("corder", serialized);
    return true;
}

void PreferenceService::setCarouselFixedView(uint8_t view) {
    if (!isCarouselView(view)) return;
    if (_carouselFixedView == view) return;
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

bool PreferenceService::setSalarySchedule(bool enabled,
                                          uint16_t startMinutes,
                                          uint16_t endMinutes) {
    if (startMinutes >= 1440 || endMinutes > 1440 ||
        startMinutes >= endMinutes) {
        return false;
    }
    _salaryAutoEnabled = enabled;
    _salaryStartMinutes = startMinutes;
    _salaryEndMinutes = endMinutes;
    _prefs.putBool("yauto", enabled);
    _prefs.putUShort("ystart", startMinutes);
    _prefs.putUShort("yend", endMinutes);
    return true;
}

void PreferenceService::setSalaryLastAutoDate(uint32_t dateKey) {
    if (_salaryLastAutoDate == dateKey) return;
    _salaryLastAutoDate = dateKey;
    _prefs.putUInt("ydate", dateKey);
}

void PreferenceService::setSalaryLastAutoEndDate(uint32_t dateKey) {
    if (_salaryLastAutoEndDate == dateKey) return;
    _salaryLastAutoEndDate = dateKey;
    _prefs.putUInt("yenddate", dateKey);
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
            String(_carouselOrder[3]) + "," + String(_carouselOrder[4]) + "]";
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
           view == VIEW_POMODORO ||
           view == VIEW_WEATHER ||
           view == VIEW_CRYPTO ||
           view == VIEW_MARKET ||
           view == VIEW_SALARY;
}

bool PreferenceService::isCarouselView(uint8_t view) const {
    return view == VIEW_CLOCK || view == VIEW_WEATHER ||
           view == VIEW_CRYPTO || view == VIEW_MARKET ||
           view == VIEW_SALARY;
}

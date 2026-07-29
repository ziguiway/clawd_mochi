#include "web_service.h"
#include "operation_mode_service.h"
#include "../config/cfg_display.h"
#include <ArduinoJson.h>

// ── Original interactive HTML (PROGMEM) ────────────────────────
// ── Constructor ────────────────────────────────────────────────
WebService::WebService(ClaudeCodeService* ccService, WifiConfigService* wifiService,
                       TimeService* timeService, DisplayService* displayService,
                       PreferenceService* preferenceService,
                       CryptoService* cryptoService,
                       MarketService* marketService)
    : _server(CFG_WIFI_WEB_PORT)
    , _started(false)
    , _ccService(ccService), _wifiService(wifiService)
    , _timeService(timeService), _displayService(displayService)
    , _preferenceService(preferenceService)
    , _cryptoService(cryptoService)
    , _marketService(marketService)
{
}

void WebService::init() {
    if (_started) return;
    setupRoutes();
    _server.begin();
    _started = true;
    LOG_INFO("Web", "HTTP 服务器启动 端口: %d", CFG_WIFI_WEB_PORT);
}

void WebService::update() {
    if (!_started) return;
    _server.handleClient();
}

// ── Routes ─────────────────────────────────────────────────────
void WebService::setupRoutes() {
    // Original interactive routes
    _server.on("/",            HTTP_GET, [this]() { handleRoot(); });
    _server.on("/cmd",         HTTP_GET, [this]() { handleCmd(); });
    _server.on("/char",        HTTP_GET, [this]() { handleChar(); });
    _server.on("/speed",       HTTP_GET, [this]() { handleSpeed(); });
    _server.on("/redraw",      HTTP_GET, [this]() { handleRedraw(); });
    _server.on("/canvas",      HTTP_GET, [this]() { handleCanvas(); });
    _server.on("/draw/clear",  HTTP_GET, [this]() { handleDrawClear(); });
    _server.on("/draw/stroke", HTTP_GET, [this]() { handleDrawStroke(); });
    _server.on("/backlight",   HTTP_GET, [this]() { handleBacklight(); });
    _server.on("/brightness",  HTTP_GET, [this]() { handleBrightness(); });
    _server.on("/timer/status", HTTP_GET, [this]() { handleTimerStatus(); });
    _server.on("/timer/start",  HTTP_GET, [this]() { handleTimerStart(); });
    _server.on("/timer/pause",  HTTP_GET, [this]() { handleTimerPause(); });
    _server.on("/timer/reset",  HTTP_GET, [this]() { handleTimerReset(); });
    _server.on("/timer/config", HTTP_GET, [this]() { handleTimerConfig(); });
    _server.on("/prefs",       HTTP_GET, [this]() { handlePrefs(); });
    _server.on("/state",       HTTP_GET, [this]() { handleState(); });
    _server.on("/expressions", HTTP_GET, [this]() { handleExpressions(); });
    _server.on("/expression/current", HTTP_GET, [this]() { sendExpressionState(false); });
    _server.on("/expression",  HTTP_POST, [this]() { handleExpressionUpdate(); });
    _server.on("/profile",     HTTP_GET, [this]() { handleProfile(); });
    _server.on("/profile",     HTTP_POST, [this]() { handleProfileUpdate(); });
    _server.on("/profile/reset", HTTP_POST, [this]() { handleProfileReset(); });
    _server.on("/serial_mode", HTTP_GET, [this]() { handleSerialMode(); });
    _server.on("/crypto/config", HTTP_GET, [this]() { handleCryptoConfig(); });
    _server.on("/crypto/config", HTTP_POST, [this]() { handleCryptoUpdate(); });
    _server.on("/crypto/refresh", HTTP_POST, [this]() { handleCryptoRefresh(); });
    _server.on("/market/config", HTTP_GET, [this]() { handleMarketConfig(); });
    _server.on("/market/config", HTTP_POST, [this]() { handleMarketUpdate(); });
    _server.on("/market/refresh", HTTP_POST, [this]() { handleMarketRefresh(); });
    _server.on("/market/search", HTTP_GET, [this]() { handleMarketSearch(); });

    // Existing routes
    _server.on("/wifi_setup", [this]() { handleWifiSetup(); });
    _server.on("/wifi_setup.html", [this]() { handleWifiSetup(); });
    _server.on("/logs", [this]() { handleLogs(); });
    _server.on("/logs.html", [this]() { handleLogs(); });
    _server.on("/cc/status", [this]() { handleCCStatus(); });
    _server.on("/cc/test", [this]() { handleCCTest(); });
    _server.on("/wifi/scan", [this]() { _wifiService->handleScanRequest(_server); });
    _server.on("/wifi/connect", HTTP_POST, [this]() { _wifiService->handleConnectRequest(_server); });
    _server.on("/wifi/status", [this]() { _wifiService->handleStatusRequest(_server); });
    _server.on("/logs/api", [this]() { handleLogsApi(); });
    _server.on("/logs/clear", HTTP_POST, [this]() { handleLogsClear(); });
    _server.on("/logs/status", [this]() { handleLogsStatus(); });
    _server.on("/time", [this]() { handleTime(); });

    // Static files from LittleFS
    _server.serveStatic("/style.css", LittleFS, "/style.css");
    _server.serveStatic("/app.js", LittleFS, "/app.js");
    _server.serveStatic("/claude_code.js", LittleFS, "/claude_code.js");
    _server.serveStatic("/wifi.js", LittleFS, "/wifi.js");

    _server.onNotFound([this]() {
        String path = _server.uri();
        if (LittleFS.exists(path)) {
            handleFile(path.c_str(), getContentType(path).c_str());
        } else {
            _server.send(404, "text/plain", "Not Found");
        }
    });
}

// ── Original interactive handlers ──────────────────────────────
void WebService::handleRoot() {
    _server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    _server.sendHeader("Pragma", "no-cache");
    handleFile("/controller.html", "text/html");
}

void WebService::handleCmd() {
    if (!_server.hasArg("k") || _server.arg("k").isEmpty()) {
        _server.send(400, "application/json", "{\"e\":1}"); return;
    }
    const char c = _server.arg("k")[0];

    if (_displayService->isTermMode()) {
        if (c == 'q') {
            _displayService->exitTerminal();
        }
        _server.send(200, "application/json", "{\"ok\":1}");
        return;
    }

    _server.send(200, "application/json", "{\"ok\":1}");
    switch (c) {
        case 'w': _displayService->setInteractiveView(VIEW_EYES_NORMAL); break;
        case 's': _displayService->setInteractiveView(VIEW_EYES_SQUISH); break;
        case 'd': _displayService->setInteractiveView(VIEW_CODE); break;
        case 'c': _displayService->setInteractiveView(VIEW_CLOCK); break;
        case 'p': _displayService->setInteractiveView(VIEW_POMODORO); break;
        case 'e': _displayService->setInteractiveView(VIEW_WEATHER); break;
        case 'm': _displayService->setInteractiveView(VIEW_CRYPTO); break;
        case 'k': _displayService->setInteractiveView(VIEW_MARKET); break;
        case 'a': _displayService->animLogoReveal(); break;
    }
}

void WebService::handleChar() {
    if (!_displayService->isTermMode()) {
        _server.send(200, "application/json", "{\"ok\":1}"); return;
    }
    const String val = _server.arg("c");
    if (val.length() > 0) _displayService->termAddChar(val[0]);
    _server.send(200, "application/json", "{\"ok\":1}");
}

void WebService::handleSpeed() {
    if (_server.hasArg("v")) {
        const uint8_t speed = constrain(_server.arg("v").toInt(), 1, 3);
        _displayService->setAnimSpeed(speed);
        if (_preferenceService) _preferenceService->setAnimSpeed(speed);
    }
    _server.send(200, "application/json", "{\"ok\":1}");
}

void WebService::handleRedraw() {
    if (_server.hasArg("bg")) {
        const String bg = _server.arg("bg");
        _displayService->setAnimBgColor(_displayService->hexToRgb565(bg));
        if (_preferenceService) _preferenceService->setDefaultBgHex(bg);
    }
    _displayService->redrawCurrentView();
    _server.send(200, "application/json", "{\"ok\":1}");
}

void WebService::handleCanvas() {
    const bool on = _server.hasArg("on") && _server.arg("on") == "1";
    if (on) {
        _displayService->enterInteractive();
        _displayService->setInteractiveView(VIEW_DRAW);
    }
    _server.send(200, "application/json", "{\"ok\":1}");
}

void WebService::handleDrawClear() {
    const String bg = _server.hasArg("bg") ? _server.arg("bg") : "#aa4818";
    _displayService->drawClear(_displayService->hexToRgb565(bg));
    _server.send(200, "application/json", "{\"ok\":1}");
}

void WebService::handleDrawStroke() {
    if (!_server.hasArg("pts") || !_server.hasArg("pen")) {
        _server.send(200, "application/json", "{\"ok\":1}"); return;
    }
    const uint16_t color = _displayService->hexToRgb565(_server.arg("pen"));
    _displayService->drawStroke(color, _server.arg("pts"));
    _server.send(200, "application/json", "{\"ok\":1}");
}

void WebService::handleBacklight() {
    if (_server.hasArg("on")) {
        const uint8_t brightness = _server.arg("on") == "1" ? 100 : 0;
        _displayService->setBrightnessPercent(brightness);
        if (_preferenceService) _preferenceService->setBrightnessPercent(brightness);
        _server.send(200, "application/json", "{\"ok\":true}");
    } else {
        _server.send(400, "application/json", "{\"error\":\"missing on parameter\"}");
    }
}

void WebService::handleBrightness() {
    if (_server.hasArg("v")) {
        const uint8_t brightness = constrain(_server.arg("v").toInt(), 0, 100);
        _displayService->setBrightnessPercent(brightness);
        if (_preferenceService) _preferenceService->setBrightnessPercent(brightness);
    }
    String json = "{\"ok\":true,\"brightness\":";
    json += _displayService->getBrightnessPercent();
    json += "}";
    _server.send(200, "application/json", json);
}

void WebService::handleTimerStatus() {
    const bool isBreak = _displayService->getPomodoroPhase() == PomodoroPhase::BREAK;
    String json = "{\"phase\":\"";
    json += isBreak ? "break" : "focus";
    json += "\",\"running\":";
    json += _displayService->isPomodoroRunning() ? "true" : "false";
    json += ",\"paused\":";
    json += _displayService->isPomodoroPaused() ? "true" : "false";
    json += ",\"remaining\":";
    json += _displayService->getPomodoroRemainingSec();
    json += ",\"duration\":";
    json += _displayService->getPomodoroDurationSec();
    json += ",\"focus\":";
    json += _displayService->getFocusMinutes();
    json += ",\"break\":";
    json += _displayService->getBreakMinutes();
    json += "}";
    _server.send(200, "application/json", json);
}

void WebService::handleTimerStart() {
    const String phase = _server.hasArg("phase") ? _server.arg("phase") : "focus";
    _displayService->startPomodoro(phase == "break" ? PomodoroPhase::BREAK : PomodoroPhase::FOCUS);
    handleTimerStatus();
}

void WebService::handleTimerPause() {
    _displayService->pausePomodoro();
    handleTimerStatus();
}

void WebService::handleTimerReset() {
    _displayService->resetPomodoro();
    handleTimerStatus();
}

void WebService::handleTimerConfig() {
    const uint16_t focus = _server.hasArg("focus") ? _server.arg("focus").toInt() : _displayService->getFocusMinutes();
    const uint16_t breakMinutes = _server.hasArg("break") ? _server.arg("break").toInt() : _displayService->getBreakMinutes();
    _displayService->setPomodoroDurations(focus, breakMinutes);
    handleTimerStatus();
}

void WebService::handlePrefs() {
    if (!_preferenceService) {
        _server.send(500, "application/json", "{\"error\":\"preferences unavailable\"}");
        return;
    }

    if (_server.hasArg("bg")) {
        const String bg = _server.arg("bg");
        _preferenceService->setDefaultBgHex(bg);
        const uint16_t color = _displayService->hexToRgb565(bg);
        _displayService->setAnimBgColor(color);
        _displayService->setDrawBgColor(color);
    }
    if (_server.hasArg("speed")) {
        const uint8_t speed = constrain(_server.arg("speed").toInt(), 1, 3);
        _preferenceService->setAnimSpeed(speed);
        _displayService->setAnimSpeed(speed);
    }
    if (_server.hasArg("startup")) {
        _preferenceService->setStartupView(constrain(_server.arg("startup").toInt(), 0, 7));
    }
    if (_server.hasArg("brightness")) {
        const uint8_t brightness = constrain(_server.arg("brightness").toInt(), 0, 100);
        _preferenceService->setBrightnessPercent(brightness);
        _displayService->setBrightnessPercent(brightness);
    }
    if (_server.hasArg("claudeStatus")) {
        const bool enabled = _server.arg("claudeStatus") == "1" ||
                             _server.arg("claudeStatus") == "true";
        _preferenceService->setClaudeStatusEnabled(enabled);
        _displayService->setClaudeStatusEnabled(enabled);
    }
    if (_server.hasArg("theme")) {
        const uint8_t theme = static_cast<uint8_t>(_server.arg("theme").toInt());
        _preferenceService->setDisplayTheme(theme);
        _displayService->setDisplayTheme(_preferenceService->getDisplayTheme());
    }
    if (_server.hasArg("carousel")) {
        const bool enabled = _server.arg("carousel") == "1" ||
                             _server.arg("carousel") == "true";
        _preferenceService->setCarouselEnabled(enabled);
    }
    if (_server.hasArg("carouselSpeed")) {
        _preferenceService->setCarouselSpeedSeconds(
            constrain(_server.arg("carouselSpeed").toInt(), 5, 60));
    }
    if (_server.hasArg("carouselFixed")) {
        _preferenceService->setCarouselFixedView(
            static_cast<uint8_t>(_server.arg("carouselFixed").toInt()));
    }
    if (_server.hasArg("carouselOrder")) {
        const String value = _server.arg("carouselOrder");
        uint8_t order[CAROUSEL_VIEW_COUNT] = {};
        uint8_t index = 0;
        int start = 0;
        while (index < CAROUSEL_VIEW_COUNT && start >= 0) {
            const int comma = value.indexOf(',', start);
            const String part = value.substring(start,
                comma < 0 ? value.length() : comma);
            order[index++] = static_cast<uint8_t>(part.toInt());
            start = comma < 0 ? -1 : comma + 1;
        }
        if (index != CAROUSEL_VIEW_COUNT ||
            !_preferenceService->setCarouselOrder(order)) {
            _server.send(400, "application/json", "{\"error\":\"invalid carousel order\"}");
            return;
        }
    }
    if (_server.hasArg("night")) {
        _preferenceService->setNightDimEnabled(_server.arg("night") == "1" ||
                                               _server.arg("night") == "true");
    }
    if (_server.hasArg("nightStart") || _server.hasArg("nightEnd")) {
        const uint8_t startHour = _server.hasArg("nightStart")
            ? _server.arg("nightStart").toInt()
            : _preferenceService->getNightStartHour();
        const uint8_t endHour = _server.hasArg("nightEnd")
            ? _server.arg("nightEnd").toInt()
            : _preferenceService->getNightEndHour();
        _preferenceService->setNightHours(startHour, endHour);
    }
    if (_server.hasArg("nightBrightness")) {
        _preferenceService->setNightBrightnessPercent(
            constrain(_server.arg("nightBrightness").toInt(), 0, 100));
    }

    _displayService->setBrightnessPercent(_preferenceService->getBrightnessPercent());
    _displayService->reloadIdleDisplayPreferences();

    String json = _preferenceService->getJson();
    json.remove(json.length() - 1);
    json += ",\"nightActive\":";
    json += _preferenceService->isNightDimActive(_timeService) ? "true" : "false";
    json += "}";
    _server.send(200, "application/json", json);
}

void WebService::handleState() {
    String j = "{\"view\":"; j += _displayService->getInteractiveView();
    j += ",\"busy\":";   j += _displayService->isBusy()       ? "true" : "false";
    j += ",\"term\":";   j += _displayService->isTermMode()   ? "true" : "false";
    j += ",\"bl\":";     j += _displayService->getBrightnessPercent() > 0 ? "true" : "false";
    j += ",\"brightness\":"; j += _displayService->getBrightnessPercent();
    j += ",\"speed\":";  j += _displayService->getAnimSpeed();
    j += ",\"claudeStatus\":"; j += _displayService->isClaudeStatusEnabled() ? "true" : "false";
    j += ",\"theme\":"; j += _displayService->getDisplayTheme();
    j += ",\"carousel\":"; j += _displayService->isCarouselEnabled() ? "true" : "false";
    j += ",\"expressionMode\":\"";
    j += _displayService->getExpressionMode() == ExpressionMode::AUTO ? "auto" : "manual";
    j += "\",\"expression\":\"";
    j += expressionIdToName(_displayService->getSelectedExpression());
    j += "\",\"renderedExpression\":\"";
    j += expressionIdToName(_displayService->getRenderedExpression());
    j += "\"";
    j += ",\"serial\":"; j += _wifiService->isSerialMode() ? "true" : "false";
    j += "}";
    _server.send(200, "application/json", j);
}

void WebService::handleExpressions() {
    sendExpressionState(true);
}

void WebService::handleExpressionUpdate() {
    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, _server.arg("plain"));
    if (error) {
        _server.send(400, "application/json", "{\"error\":\"invalid json\"}");
        return;
    }

    const char* id = doc["id"] | "";
    const char* mode = doc["mode"] | "";
    if (id[0] != '\0' && mode[0] != '\0' && strcmp(mode, "manual") != 0) {
        _server.send(400, "application/json", "{\"error\":\"id requires manual mode\"}");
        return;
    }

    if (id[0] != '\0') {
        ExpressionId expression;
        if (!expressionIdFromName(String(id), expression)) {
            _server.send(400, "application/json", "{\"error\":\"unknown expression\"}");
            return;
        }
        _displayService->setExpression(expression);
        LOG_INFO("Web", "Expression manual: %s", expressionIdToName(expression));
    } else if (strcmp(mode, "auto") == 0) {
        _displayService->setExpressionMode(ExpressionMode::AUTO);
        LOG_INFO("Web", "Expression mode: auto");
    } else if (strcmp(mode, "manual") == 0) {
        _displayService->setExpressionMode(ExpressionMode::MANUAL);
        LOG_INFO("Web", "Expression mode: manual");
    } else {
        _server.send(400, "application/json", "{\"error\":\"id or mode required\"}");
        return;
    }

    sendExpressionState(false);
}

void WebService::handleProfile() {
    if (!_preferenceService) {
        _server.send(503, "application/json", "{\"error\":\"profile unavailable\"}");
        return;
    }
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", _preferenceService->getProfileJson());
}

void WebService::handleProfileUpdate() {
    if (!_preferenceService) {
        _server.send(503, "application/json", "{\"error\":\"profile unavailable\"}");
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, _server.arg("plain"))) {
        _server.send(400, "application/json", "{\"error\":\"invalid json\"}");
        return;
    }
    if (!doc["deviceName"].is<const char*>() ||
        !doc["bootLine1"].is<const char*>() ||
        !doc["bootLine2"].is<const char*>() ||
        !doc["defaultExpression"].is<const char*>() ||
        !doc["expressionMode"].is<const char*>()) {
        _server.send(400, "application/json", "{\"error\":\"all profile fields required\"}");
        return;
    }

    const String deviceName = doc["deviceName"].as<String>();
    const String bootLine1 = doc["bootLine1"].as<String>();
    const String bootLine2 = doc["bootLine2"].as<String>();
    const String expressionName = doc["defaultExpression"].as<String>();
    const String modeName = doc["expressionMode"].as<String>();
    ExpressionId expression;
    if (!PreferenceService::isValidProfileText(deviceName, 12, false)) {
        _server.send(400, "application/json", "{\"error\":\"deviceName must be 1-12 printable ASCII characters\"}");
        return;
    }
    if (!PreferenceService::isValidProfileText(bootLine1, 16, true) ||
        !PreferenceService::isValidProfileText(bootLine2, 16, true)) {
        _server.send(400, "application/json", "{\"error\":\"boot lines must be 0-16 printable ASCII characters\"}");
        return;
    }
    if (!expressionIdFromName(expressionName, expression)) {
        _server.send(400, "application/json", "{\"error\":\"unknown default expression\"}");
        return;
    }
    if (modeName != "auto" && modeName != "manual") {
        _server.send(400, "application/json", "{\"error\":\"expressionMode must be auto or manual\"}");
        return;
    }

    const ExpressionMode mode = modeName == "auto"
        ? ExpressionMode::AUTO : ExpressionMode::MANUAL;
    const bool saved = _preferenceService->setDeviceName(deviceName) &&
        _preferenceService->setBootLine1(bootLine1) &&
        _preferenceService->setBootLine2(bootLine2) &&
        _preferenceService->setDefaultExpression(expression) &&
        _preferenceService->setExpressionMode(mode);
    if (!saved) {
        _server.send(500, "application/json", "{\"error\":\"profile storage failed\"}");
        return;
    }
    _displayService->setExpression(expression);
    _displayService->setExpressionMode(mode);
    LOG_INFO("Web", "Profile saved: device=%s expression=%s mode=%s",
             deviceName.c_str(), expressionIdToName(expression),
             mode == ExpressionMode::AUTO ? "auto" : "manual");
    handleProfile();
}

void WebService::handleProfileReset() {
    if (!_preferenceService) {
        _server.send(503, "application/json", "{\"error\":\"profile unavailable\"}");
        return;
    }
    _preferenceService->resetProfile();
    _displayService->setExpression(ExpressionId::NORMAL);
    _displayService->setExpressionMode(ExpressionMode::MANUAL);
    LOG_INFO("Web", "Profile reset to defaults");
    handleProfile();
}

void WebService::sendExpressionState(bool includeList) {
    String json = "{\"mode\":\"";
    json += _displayService->getExpressionMode() == ExpressionMode::AUTO
        ? "auto" : "manual";
    json += "\",\"selected\":\"";
    json += expressionIdToName(_displayService->getSelectedExpression());
    json += "\",\"rendered\":\"";
    json += expressionIdToName(_displayService->getRenderedExpression());
    json += "\"";
    if (includeList) {
        json += ",\"expressions\":[";
        for (uint8_t index = 0; index < EXPRESSION_COUNT; index++) {
            if (index > 0) json += ",";
            const ExpressionId expression = static_cast<ExpressionId>(index);
            json += "{\"id\":\"";
            json += expressionIdToName(expression);
            json += "\",\"label\":\"";
            json += expressionIdToLabel(expression);
            json += "\"}";
        }
        json += "]";
    }
    json += "}";
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", json);
}

void WebService::handleSerialMode() {
    _wifiService->skipProvisioning();
    _server.send(200, "application/json", "{\"ok\":true,\"mode\":\"serial\"}");
}

void WebService::handleCryptoConfig() {
    if (!_cryptoService) {
        _server.send(503, "application/json", "{\"error\":\"crypto unavailable\"}");
        return;
    }
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", _cryptoService->getJson());
}

void WebService::handleCryptoUpdate() {
    if (!_cryptoService) {
        _server.send(503, "application/json", "{\"error\":\"crypto unavailable\"}");
        return;
    }

    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, _server.arg("plain"));
    JsonArray list = doc["assets"].as<JsonArray>();
    if (error || list.isNull() || list.size() == 0 ||
        list.size() > CryptoService::MAX_ASSETS) {
        _server.send(400, "application/json", "{\"error\":\"choose 1 to 5 assets\"}");
        return;
    }

    CryptoAsset assets[CryptoService::MAX_ASSETS] = {};
    uint8_t count = 0;
    for (JsonObject item : list) {
        const char* id = item["id"] | "";
        const char* symbol = item["symbol"] | "";
        const char* name = item["name"] | symbol;
        if (id[0] == '\0' || symbol[0] == '\0') {
            _server.send(400, "application/json", "{\"error\":\"invalid asset\"}");
            return;
        }
        for (uint8_t previous = 0; previous < count; previous++) {
            if (strcmp(assets[previous].id, id) == 0) {
                _server.send(409, "application/json", "{\"error\":\"duplicate asset\"}");
                return;
            }
        }
        strlcpy(assets[count].id, id, sizeof(assets[count].id));
        strlcpy(assets[count].symbol, symbol, sizeof(assets[count].symbol));
        strlcpy(assets[count].name, name, sizeof(assets[count].name));
        assets[count].isGold = item["gold"] | false;
        count++;
    }

    if (!_cryptoService->setAssets(assets, count)) {
        _server.send(400, "application/json", "{\"error\":\"invalid asset configuration\"}");
        return;
    }
    if (_displayService->getInteractiveView() == VIEW_CRYPTO) {
        _displayService->redrawCurrentView();
    }
    handleCryptoConfig();
}

void WebService::handleCryptoRefresh() {
    if (!_cryptoService) {
        _server.send(503, "application/json", "{\"error\":\"crypto unavailable\"}");
        return;
    }
    _cryptoService->requestRefresh();
    _server.send(202, "application/json", "{\"ok\":true}");
}

void WebService::handleMarketConfig() {
    if (!_marketService) {
        _server.send(503, "application/json", "{\"error\":\"market unavailable\"}");
        return;
    }
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", _marketService->getJson());
}

void WebService::handleMarketUpdate() {
    if (!_marketService) {
        _server.send(503, "application/json", "{\"error\":\"market unavailable\"}");
        return;
    }

    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, _server.arg("plain"));
    JsonArray list = doc["assets"].as<JsonArray>();
    if (error || list.isNull() || list.size() == 0 ||
        list.size() > MarketService::MAX_ASSETS) {
        _server.send(400, "application/json", "{\"error\":\"choose 1 to 5 stocks\"}");
        return;
    }

    MarketAsset assets[MarketService::MAX_ASSETS] = {};
    uint8_t count = 0;
    for (JsonObject item : list) {
        const char* secid = item["secid"] | "";
        const char* code = item["code"] | "";
        const char* label = item["label"] | code;
        const char* name = item["name"] | code;
        if (secid[0] == '\0' || code[0] == '\0') {
            _server.send(400, "application/json", "{\"error\":\"invalid stock\"}");
            return;
        }
        for (uint8_t previous = 0; previous < count; previous++) {
            if (strcmp(assets[previous].secid, secid) == 0) {
                _server.send(409, "application/json", "{\"error\":\"duplicate stock\"}");
                return;
            }
        }
        strlcpy(assets[count].secid, secid, sizeof(assets[count].secid));
        strlcpy(assets[count].code, code, sizeof(assets[count].code));
        strlcpy(assets[count].label, label, sizeof(assets[count].label));
        strlcpy(assets[count].name, name, sizeof(assets[count].name));
        count++;
    }

    if (!_marketService->setAssets(assets, count)) {
        _server.send(400, "application/json",
                     "{\"error\":\"invalid market configuration\"}");
        return;
    }
    if (_displayService->getInteractiveView() == VIEW_MARKET) {
        _displayService->redrawCurrentView();
    }
    handleMarketConfig();
}

void WebService::handleMarketRefresh() {
    if (!_marketService) {
        _server.send(503, "application/json", "{\"error\":\"market unavailable\"}");
        return;
    }
    _marketService->requestRefresh();
    _server.send(202, "application/json", "{\"ok\":true}");
}

void WebService::handleMarketSearch() {
    if (!_marketService) {
        _server.send(503, "application/json", "{\"error\":\"market unavailable\"}");
        return;
    }
    const String query = _server.hasArg("q") ? _server.arg("q") : "";
    if (query.isEmpty() || query.length() > 48) {
        _server.send(400, "application/json", "{\"error\":\"invalid query\"}");
        return;
    }
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", _marketService->searchJson(query));
}

// ── Existing handlers ──────────────────────────────────────────
void WebService::handleWifiSetup() { handleFile("/wifi_setup.html", "text/html"); }
void WebService::handleLogs() { handleFile("/logs.html", "text/html"); }

void WebService::handleFile(const char* path, const char* contentType) {
    File file = LittleFS.open(path, "r");
    if (!file) { _server.send(404, "text/plain", "File not found"); return; }
    _server.streamFile(file, contentType);
    file.close();
}

void WebService::handleCCStatus() { _server.send(200, "application/json", _ccService->getStatusJson()); }
void WebService::handleCCTest() { _server.send(200, "application/json", "{\"status\":\"ok\",\"device\":\"ClawdMochi\"}"); }

void WebService::handleTime() {
    String json = "{\"time\":\"" + _timeService->getDateTime() + "\",\"synced\":" + String(_timeService->isSynced() ? "true" : "false") + "}";
    _server.send(200, "application/json", json);
}

void WebService::handleLogsApi() {
    size_t maxLines = _server.hasArg("lines") ? _server.arg("lines").toInt() : 100;
    _server.send(200, "text/plain", Logger::getInstance().getLogs(maxLines));
}

void WebService::handleLogsClear() {
    Logger::getInstance().clearLogs();
    _server.send(200, "application/json", "{\"ok\":true}");
}

void WebService::handleLogsStatus() {
    _server.send(200, "application/json", "{\"size\":" + String(Logger::getInstance().getLogSize()) + "}");
}

String WebService::getContentType(const String& path) {
    if (path.endsWith(".html")) return "text/html";
    if (path.endsWith(".css")) return "text/css";
    if (path.endsWith(".js")) return "application/javascript";
    if (path.endsWith(".json")) return "application/json";
    if (path.endsWith(".png")) return "image/png";
    if (path.endsWith(".ico")) return "image/x-icon";
    return "text/plain";
}

String WebService::rgb565ToHex(uint16_t c) {
    uint8_t r = ((c >> 11) & 0x1F) << 3;
    uint8_t g = ((c >> 5)  & 0x3F) << 2;
    uint8_t b = (c & 0x1F) << 3;
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
    return String(buf);
}

#include "web_service.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_ota_ops.h>

#include "../utils/memory_monitor.h"
#include "../utils/network_request_gate.h"
#include "../config/app_config.h"
#include "operation_mode_service.h"
#include "ota_service.h"
#include "../config/cfg_display.h"
#include <ArduinoJson.h>
#include <new>

namespace {
void appendUInt64(String& output, uint64_t value) {
    char buffer[24];
    snprintf(buffer, sizeof(buffer), "%llu",
             static_cast<unsigned long long>(value));
    output += buffer;
}
}

// ── Original interactive HTML (PROGMEM) ────────────────────────
// ── Constructor ────────────────────────────────────────────────
WebService::WebService(ClaudeCodeService* ccService, WifiConfigService* wifiService,
                       TimeService* timeService, DisplayService* displayService,
                       PreferenceService* preferenceService,
                       CryptoService* cryptoService,
                       MarketService* marketService,
                       TimetableService* timetableService,
                       OtaService* otaService)
    : _server(CFG_WIFI_WEB_PORT)
    , _started(false)
    , _ccService(ccService), _wifiService(wifiService)
    , _timeService(timeService), _displayService(displayService)
    , _preferenceService(preferenceService)
    , _cryptoService(cryptoService)
    , _marketService(marketService)
    , _timetableService(timetableService)
    , _otaService(otaService)
    , _mediaUploadAccepted(false)
    , _mediaUploadComplete(false)
    , _mediaUploadStatusCode(400)
    , _mediaExpectedBytes(0)
    , _mediaAnimationUploadFile(nullptr)
    , _mediaAnimationUploadAccepted(false)
    , _mediaAnimationUploadComplete(false)
    , _mediaAnimationUploadStatusCode(400)
    , _mediaAnimationUploadBytes(0)
    , _mediaAnimationMaxBytes(0)
    , _mediaAnimationUploadStartedMs(0)
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
    if (_otaService) _otaService->update();
    _server.handleClient();
}

// ── Routes ─────────────────────────────────────────────────────
void WebService::setupRoutes() {
    // Original interactive routes
    _server.on("/",            HTTP_GET, [this]() { handleRoot(); });
    _server.on("/onboarding", HTTP_GET, [this]() { handleOnboarding(); });
    _server.on("/onboarding/status", HTTP_GET,
        [this]() { handleOnboardingStatus(); });
    _server.on("/onboarding/offline", HTTP_POST,
        [this]() { handleOfflineMode(); });
    const auto redirectToOnboarding = [this]() {
        _server.sendHeader("Location", "/onboarding", true);
        _server.send(302, "text/plain", "Captive portal");
    };
    _server.on("/generate_204", HTTP_GET, redirectToOnboarding);
    _server.on("/hotspot-detect.html", HTTP_GET, redirectToOnboarding);
    _server.on("/connecttest.txt", HTTP_GET, redirectToOnboarding);
    _server.on("/ncsi.txt", HTTP_GET, redirectToOnboarding);
    _server.on("/wakeup_import.js", HTTP_GET, [this]() {
        handleFile("/wakeup_import.js", "application/javascript");
    });
    _server.on("/onboarding.js", HTTP_GET, [this]() {
        handleFile("/onboarding.js", "application/javascript");
    });
    _server.on("/media.js", HTTP_GET, [this]() {
        handleFile("/media.js", "application/javascript");
    });
    _server.on("/gif_reader.js", HTTP_GET, [this]() {
        handleFile("/gif_reader.js", "application/javascript");
    });
    _server.on("/gif_encoder.js", HTTP_GET, [this]() {
        handleFile("/gif_encoder.js", "application/javascript");
    });
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
    _server.on("/salary/status", HTTP_GET, [this]() { handleSalaryStatus(); });
    _server.on("/salary/config", HTTP_GET, [this]() { handleSalaryConfig(); });
    _server.on("/salary/config", HTTP_POST, [this]() { handleSalaryConfig(); });
    _server.on("/salary/start", HTTP_POST, [this]() { handleSalaryStart(); });
    _server.on("/salary/pause", HTTP_POST, [this]() { handleSalaryPause(); });
    _server.on("/salary/resume", HTTP_POST, [this]() { handleSalaryResume(); });
    _server.on("/salary/finish", HTTP_POST, [this]() { handleSalaryFinish(); });
    _server.on("/salary/reset", HTTP_POST, [this]() { handleSalaryReset(); });
    _server.on("/game/dino/start", HTTP_POST, [this]() { handleDinoStart(); });
    _server.on("/game/dino/action", HTTP_POST, [this]() { handleDinoAction(); });
    _server.on("/game/dino/restart", HTTP_POST, [this]() { handleDinoRestart(); });
    _server.on("/game/dino/exit", HTTP_POST, [this]() { handleDinoExit(); });
    _server.on("/game/dino/state", HTTP_GET, [this]() { handleDinoState(); });
    _server.on("/game/sokoban/start", HTTP_POST, [this]() { handleSokobanStart(); });
    _server.on("/game/sokoban/move", HTTP_POST, [this]() { handleSokobanMove(); });
    _server.on("/game/sokoban/undo", HTTP_POST, [this]() { handleSokobanUndo(); });
    _server.on("/game/sokoban/restart", HTTP_POST, [this]() { handleSokobanRestart(); });
    _server.on("/game/sokoban/level", HTTP_POST, [this]() { handleSokobanLevel(); });
    _server.on("/game/sokoban/exit", HTTP_POST, [this]() { handleSokobanExit(); });
    _server.on("/game/sokoban/state", HTTP_GET, [this]() { handleSokobanState(); });
    _server.on("/game/start", HTTP_POST, [this]() { handleArcadeStart(); });
    _server.on("/game/action", HTTP_POST, [this]() { handleArcadeAction(); });
    _server.on("/game/exit", HTTP_POST, [this]() { handleArcadeExit(); });
    _server.on("/game/state", HTTP_GET, [this]() { handleArcadeState(); });
    _server.on("/game/catalog", HTTP_GET, [this]() { handleArcadeCatalog(); });
    _server.on("/prefs",       HTTP_GET, [this]() { handlePrefs(); });
    _server.on("/state",       HTTP_GET, [this]() { handleState(); });
    _server.on("/expressions", HTTP_GET, [this]() { handleExpressions(); });
    _server.on("/expression/current", HTTP_GET, [this]() { sendExpressionState(false); });
    _server.on("/expression",  HTTP_POST, [this]() { handleExpressionUpdate(); });
    _server.on("/profile",     HTTP_GET, [this]() { handleProfile(); });
    _server.on("/profile",     HTTP_POST, [this]() { handleProfileUpdate(); });
    _server.on("/profile/reset", HTTP_POST, [this]() { handleProfileReset(); });
    _server.on("/config/export", HTTP_GET, [this]() { handleConfigExport(); });
    _server.on("/config/import", HTTP_POST, [this]() { handleConfigImport(); });
    _server.on("/serial_mode", HTTP_GET, [this]() { handleSerialMode(); });
    _server.on("/crypto/config", HTTP_GET, [this]() { handleCryptoConfig(); });
    _server.on("/crypto/config", HTTP_POST, [this]() { handleCryptoUpdate(); });
    _server.on("/crypto/refresh", HTTP_POST, [this]() { handleCryptoRefresh(); });
    _server.on("/market/config", HTTP_GET, [this]() { handleMarketConfig(); });
    _server.on("/market/config", HTTP_POST, [this]() { handleMarketUpdate(); });
    _server.on("/market/refresh", HTTP_POST, [this]() { handleMarketRefresh(); });
    _server.on("/market/search", HTTP_GET, [this]() { handleMarketSearch(); });
    _server.on("/timetable", HTTP_GET, [this]() { handleTimetableGet(); });
    _server.on("/timetable", HTTP_POST, [this]() { handleTimetableSave(); });
    _server.on("/timetable/status", HTTP_GET, [this]() { handleTimetableStatus(); });
    _server.on("/timetable/import/wakeup/proxy", HTTP_POST, [this]() { handleWakeUpProxy(); });
    _server.on("/media/frame", HTTP_POST,
        [this]() { handleMediaFrame(); },
        [this]() { handleMediaFrameUpload(); });
    _server.on("/media/animation", HTTP_POST,
        [this]() { handleMediaAnimation(); },
        [this]() { handleMediaAnimationUpload(); });
    _server.on("/media/stop", HTTP_POST, [this]() { handleMediaStop(); });
    _server.on("/media/status", HTTP_GET, [this]() { handleMediaStatus(); });
    _server.on("/stream/enter", HTTP_POST, [this]() { handleStreamEnter(); });
    _server.on("/stream/exit", HTTP_POST, [this]() { handleStreamExit(); });
    _server.on("/stream/status", HTTP_GET, [this]() { handleStreamStatus(); });
    _server.on("/keyboard_pet/start", HTTP_POST, [this]() { handleKeyboardPetStart(); });
    _server.on("/keyboard_pet/stop", HTTP_POST, [this]() { handleKeyboardPetStop(); });
    _server.on("/ota/status", HTTP_GET, [this]() { handleOtaStatus(); });
    _server.on("/ota/check", HTTP_POST, [this]() { handleOtaCheck(); });
    _server.on("/ota/install", HTTP_POST, [this]() { handleOtaInstall(); });
    _server.on("/ota/cancel", HTTP_POST, [this]() { handleOtaCancel(); });
    _server.on("/ota/upload", HTTP_POST,
        [this]() { handleOtaUpload(); },
        [this]() { handleOtaUploadData(); });
    _server.on("/ota/status", HTTP_OPTIONS, [this]() { handleOtaOptions(); });
    _server.on("/ota/check", HTTP_OPTIONS, [this]() { handleOtaOptions(); });
    _server.on("/ota/install", HTTP_OPTIONS, [this]() { handleOtaOptions(); });
    _server.on("/ota/cancel", HTTP_OPTIONS, [this]() { handleOtaOptions(); });
    _server.on("/ota/upload", HTTP_OPTIONS, [this]() { handleOtaOptions(); });

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
    _server.serveStatic("/controller.css", LittleFS, "/controller.css");
    _server.serveStatic("/controller.js", LittleFS, "/controller.js");
    _server.serveStatic("/app.js", LittleFS, "/app.js");
    _server.serveStatic("/claude_code.js", LittleFS, "/claude_code.js");
    _server.serveStatic("/wifi.js", LittleFS, "/wifi.js");

    _server.onNotFound([this]() {
        String path = _server.uri();
        if (LittleFS.exists(path)) {
            handleFile(path.c_str(), getContentType(path).c_str());
        } else if (!_wifiService->isConfigured() &&
                   !_wifiService->isOfflineMode()) {
            _server.sendHeader("Location", "/onboarding", true);
            _server.send(302, "text/plain", "Captive portal");
        } else {
            _server.send(404, "text/plain", "Not Found");
        }
    });
}

// ── Original interactive handlers ──────────────────────────────
void WebService::handleRoot() {
    if (!_wifiService->isConfigured() && !_wifiService->isOfflineMode()) {
        handleOnboarding();
        return;
    }
    _server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    _server.sendHeader("Pragma", "no-cache");
    handleFile("/controller.html", "text/html");
}

void WebService::handleOnboarding() {
    handleFile("/onboarding.html", "text/html");
}

void WebService::handleOnboardingStatus() {
    String json = "{\"configured\":";
    json += _wifiService->isConfigured() ? "true" : "false";
    json += ",\"offline\":";
    json += _wifiService->isOfflineMode() ? "true" : "false";
    json += ",\"apSsid\":\"" + String(CFG_WIFI_AP_SSID) +
            "\",\"apIp\":\"" + _wifiService->getAPIP() + "\"}";
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", json);
}

void WebService::handleOfflineMode() {
    _wifiService->setOfflineMode(true);
    _server.send(200, "application/json",
                 "{\"ok\":true,\"mode\":\"offline\"}");
}

void WebService::handleMediaFrameUpload() {
    HTTPUpload& upload = _server.upload();
    switch (upload.status) {
        case UPLOAD_FILE_START:
            _mediaUploadComplete = false;
            _mediaUploadStatusCode = 400;
            _mediaExpectedBytes = 0;
            {
                const int x = _server.hasArg("x")
                    ? _server.arg("x").toInt() : 0;
                const int y = _server.hasArg("y")
                    ? _server.arg("y").toInt() : 0;
                const int width = _server.hasArg("w")
                    ? _server.arg("w").toInt() : CFG_DISPLAY_WIDTH;
                const int height = _server.hasArg("h")
                    ? _server.arg("h").toInt() : CFG_DISPLAY_HEIGHT;
                if (x < 0 || y < 0 || width <= 0 || height <= 0 ||
                    x + width > CFG_DISPLAY_WIDTH ||
                    y + height > CFG_DISPLAY_HEIGHT) {
                    _mediaUploadAccepted = false;
                    break;
                }
                _mediaExpectedBytes =
                    static_cast<size_t>(width) * height * sizeof(uint16_t);
                _mediaUploadAccepted = _displayService->beginMediaFrame(
                    x, y, width, height);
                if (!_mediaUploadAccepted) _mediaUploadStatusCode = 503;
            }
            break;
        case UPLOAD_FILE_WRITE:
            if (_mediaUploadAccepted &&
                !_displayService->writeMediaFrameBytes(
                    upload.buf, upload.currentSize)) {
                _displayService->abortMediaFrame();
                _mediaUploadAccepted = false;
                _mediaUploadStatusCode = 400;
            }
            break;
        case UPLOAD_FILE_END:
            if (_mediaUploadAccepted &&
                upload.totalSize == _mediaExpectedBytes &&
                _displayService->finishMediaFrame()) {
                _mediaUploadComplete = true;
                _mediaUploadStatusCode = 200;
            } else {
                _displayService->abortMediaFrame();
                _mediaUploadComplete = false;
                if (_mediaUploadStatusCode != 503) {
                    _mediaUploadStatusCode = 400;
                }
            }
            break;
        case UPLOAD_FILE_ABORTED:
            _displayService->abortMediaFrame();
            _mediaUploadAccepted = false;
            _mediaUploadComplete = false;
            _mediaUploadStatusCode = 400;
            break;
    }
}

void WebService::handleMediaFrame() {
    if (_mediaUploadComplete) {
        _server.send(200, "application/json",
                     "{\"ok\":true}");
    } else if (_mediaUploadStatusCode == 503) {
        _server.send(503, "application/json",
                     "{\"error\":\"media buffer unavailable\"}");
    } else {
        _server.send(400, "application/json",
                     "{\"error\":\"invalid RGB565 frame region\"}");
    }
    _mediaUploadAccepted = false;
    _mediaUploadComplete = false;
}

void WebService::handleMediaAnimationUpload() {
    static constexpr size_t MEDIA_FS_RESERVE_BYTES = 64U * 1024U;
    HTTPUpload& upload = _server.upload();
    switch (upload.status) {
        case UPLOAD_FILE_START: {
            _displayService->stopMedia();
            if (_mediaAnimationUploadFile) {
                _mediaAnimationUploadFile->close();
                delete _mediaAnimationUploadFile;
                _mediaAnimationUploadFile = nullptr;
            }
            if (LittleFS.exists("/media_animation.tmp")) {
                LittleFS.remove("/media_animation.tmp");
            }
            if (LittleFS.exists("/media_animation.previous")) {
                LittleFS.remove("/media_animation.previous");
            }
            if (LittleFS.exists("/media_animation.gif")) {
                LittleFS.rename("/media_animation.gif",
                                "/media_animation.previous");
            }
            const size_t freeBytes = LittleFS.totalBytes() - LittleFS.usedBytes();
            const size_t fsLimit = freeBytes > MEDIA_FS_RESERVE_BYTES
                ? freeBytes - MEDIA_FS_RESERVE_BYTES : 0;
            _mediaAnimationMaxBytes = min(fsLimit, MEDIA_ANIMATION_MAX_BYTES);
            _mediaAnimationUploadBytes = 0;
            _mediaAnimationUploadComplete = false;
            _mediaAnimationUploadStatusCode = 400;
            _mediaAnimationUploadStartedMs = millis();
            fs::File opened = LittleFS.open("/media_animation.tmp", "w");
            _mediaAnimationUploadFile = new (std::nothrow) fs::File(opened);
            _mediaAnimationUploadAccepted = _mediaAnimationUploadFile &&
                *_mediaAnimationUploadFile && _mediaAnimationMaxBytes >= 16;
            if (!_mediaAnimationUploadAccepted) {
                if (_mediaAnimationUploadFile) {
                    _mediaAnimationUploadFile->close();
                    delete _mediaAnimationUploadFile;
                    _mediaAnimationUploadFile = nullptr;
                }
                if (LittleFS.exists("/media_animation.previous")) {
                    LittleFS.rename("/media_animation.previous",
                                    "/media_animation.gif");
                }
                _mediaAnimationUploadStatusCode = 507;
            }
            break;
        }
        case UPLOAD_FILE_WRITE:
            if (_mediaAnimationUploadAccepted) {
                if (_mediaAnimationUploadBytes + upload.currentSize >
                        _mediaAnimationMaxBytes ||
                    _mediaAnimationUploadFile->write(
                        upload.buf, upload.currentSize) != upload.currentSize) {
                    _mediaAnimationUploadAccepted = false;
                    _mediaAnimationUploadStatusCode =
                        _mediaAnimationUploadBytes + upload.currentSize >
                            MEDIA_ANIMATION_MAX_BYTES ? 413 : 507;
                } else {
                    _mediaAnimationUploadBytes += upload.currentSize;
                }
            }
            break;
        case UPLOAD_FILE_END: {
            if (_mediaAnimationUploadFile) {
                _mediaAnimationUploadFile->flush();
                _mediaAnimationUploadFile->close();
                delete _mediaAnimationUploadFile;
                _mediaAnimationUploadFile = nullptr;
            }
            const bool stored = _mediaAnimationUploadAccepted &&
                upload.totalSize == _mediaAnimationUploadBytes &&
                _mediaAnimationUploadBytes >= 16 &&
                LittleFS.rename("/media_animation.tmp", "/media_animation.gif");
            _mediaAnimationUploadComplete = stored &&
                _displayService->startMediaGif("/media_animation.gif");
            _mediaAnimationUploadStatusCode = _mediaAnimationUploadComplete
                ? 200
                : (_mediaAnimationUploadStatusCode == 413 ? 413
                   : (_mediaAnimationUploadStatusCode == 507 ? 507 : 400));
            if (!_mediaAnimationUploadComplete) {
                LittleFS.remove("/media_animation.tmp");
                if (stored) LittleFS.remove("/media_animation.gif");
                if (LittleFS.exists("/media_animation.previous")) {
                    LittleFS.rename("/media_animation.previous",
                                    "/media_animation.gif");
                }
            } else {
                LittleFS.remove("/media_animation.previous");
                LOG_INFO("Media", "GIF upload=%u bytes elapsed=%lums",
                         static_cast<unsigned int>(_mediaAnimationUploadBytes),
                         millis() - _mediaAnimationUploadStartedMs);
            }
            break;
        }
        case UPLOAD_FILE_ABORTED:
            if (_mediaAnimationUploadFile) {
                _mediaAnimationUploadFile->close();
                delete _mediaAnimationUploadFile;
                _mediaAnimationUploadFile = nullptr;
            }
            LittleFS.remove("/media_animation.tmp");
            if (LittleFS.exists("/media_animation.previous")) {
                LittleFS.rename("/media_animation.previous",
                                "/media_animation.gif");
            }
            _mediaAnimationUploadAccepted = false;
            _mediaAnimationUploadComplete = false;
            _mediaAnimationUploadStatusCode = 400;
            break;
    }
}

void WebService::handleMediaAnimation() {
    String json;
    if (_mediaAnimationUploadComplete) {
        json.reserve(96);
        json = "{\"ok\":true,\"bytes\":";
        json += static_cast<unsigned int>(_mediaAnimationUploadBytes);
        json += ",\"uploadMs\":";
        json += millis() - _mediaAnimationUploadStartedMs;
        json += "}";
        _server.send(200, "application/json", json);
    } else if (_mediaAnimationUploadStatusCode == 413) {
        _server.send(413, "application/json",
                     "{\"error\":\"GIF exceeds 560 KB\"}");
    } else if (_mediaAnimationUploadStatusCode == 507) {
        _server.send(507, "application/json",
                     "{\"error\":\"not enough LittleFS space\"}");
    } else {
        _server.send(400, "application/json",
                     "{\"error\":\"invalid GIF animation\"}");
    }
    _mediaAnimationUploadAccepted = false;
    _mediaAnimationUploadComplete = false;
}

void WebService::handleMediaStop() {
    _displayService->stopMedia();
    _server.send(200, "application/json", "{\"ok\":true}");
}

void WebService::handleStreamEnter() {
    const bool ok = _displayService->enterDesktopStream();
    _server.send(ok ? 200 : 507, "application/json",
                 _displayService->getStreamStatusJson());
}

void WebService::handleStreamExit() {
    _displayService->exitDesktopStream();
    _server.send(200, "application/json", _displayService->getStreamStatusJson());
}

void WebService::handleStreamStatus() {
    _server.send(200, "application/json", _displayService->getStreamStatusJson());
}

void WebService::handleKeyboardPetStart() {
    _displayService->setInteractiveView(VIEW_KEYBOARD_PET);
    _server.send(200, "application/json", "{\"active\":true}");
}

void WebService::handleKeyboardPetStop() {
    _displayService->exitInteractive();
    _server.send(200, "application/json", "{\"active\":false}");
}

void WebService::handleMediaStatus() {
    String json = "{\"active\":";
    json += _displayService->isMediaActive() ? "true" : "false";
    json += ",\"width\":240,\"height\":240,\"pixelFormat\":\"rgb565be\"";
    json += ",\"fsFree\":";
    json += static_cast<unsigned int>(LittleFS.totalBytes() - LittleFS.usedBytes());
    json += ",\"renderMs\":";
    json += _displayService->getMediaLastRenderMs();
    json += ",\"frames\":";
    json += _displayService->getMediaRenderedFrames();
    json += "}";
    _server.send(200, "application/json", json);
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
    uint8_t rememberedView = 0xFF;
    switch (c) {
        case 'w':
            rememberedView = VIEW_EYES_NORMAL;
            _displayService->setInteractiveView(VIEW_EYES_NORMAL);
            break;
        case 's':
            rememberedView = VIEW_EYES_SQUISH;
            _displayService->setInteractiveView(VIEW_EYES_SQUISH);
            break;
        case 'd': _displayService->setInteractiveView(VIEW_CODE); break;
        case 'c':
            rememberedView = VIEW_CLOCK;
            _displayService->setInteractiveView(VIEW_CLOCK);
            break;
        case 'p':
            rememberedView = VIEW_POMODORO;
            _displayService->setInteractiveView(VIEW_POMODORO);
            break;
        case 'e':
            rememberedView = VIEW_WEATHER;
            _displayService->setInteractiveView(VIEW_WEATHER);
            break;
        case 'm':
            rememberedView = VIEW_CRYPTO;
            _displayService->setInteractiveView(VIEW_CRYPTO);
            break;
        case 'k':
            rememberedView = VIEW_MARKET;
            _displayService->setInteractiveView(VIEW_MARKET);
            break;
        case 'y':
            rememberedView = VIEW_SALARY;
            _displayService->setInteractiveView(VIEW_SALARY);
            break;
        case 'u':
            rememberedView = VIEW_TIMETABLE;
            _displayService->setInteractiveView(VIEW_TIMETABLE);
            break;
        case 'a': _displayService->animLogoReveal(); break;
    }
    if (_preferenceService && rememberedView != 0xFF) {
        // 只记住用户主动选择且适合上电恢复的页面；终端、画板、动画和游戏
        // 都是临时状态，不能让设备下次启动停在不可交互的中间界面。
        _preferenceService->setStartupView(rememberedView);
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
    const String bg = _server.hasArg("bg") ? _server.arg("bg") : "#da1100";
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

void WebService::handleDinoStart() {
    _displayService->startDinoGame();
    _server.send(200, "application/json", _displayService->getDinoGameStateJson());
}

void WebService::handleDinoAction() {
    if (!_server.hasArg("action") || _server.arg("action") != "jump") {
        _server.send(400, "application/json", "{\"error\":\"unknown action\"}");
        return;
    }
    if (!_displayService->isDinoGameActive()) {
        _server.send(409, "application/json", "{\"error\":\"game is not active\"}");
        return;
    }
    _displayService->dinoJump();
    _server.send(200, "application/json", _displayService->getDinoGameStateJson());
}

void WebService::handleDinoRestart() {
    if (!_displayService->isDinoGameActive()) {
        _server.send(409, "application/json", "{\"error\":\"game is not active\"}");
        return;
    }
    _displayService->restartDinoGame();
    _server.send(200, "application/json", _displayService->getDinoGameStateJson());
}

void WebService::handleDinoExit() {
    _displayService->exitDinoGame();
    _server.send(200, "application/json", _displayService->getDinoGameStateJson());
}

void WebService::handleDinoState() {
    _server.send(200, "application/json", _displayService->getDinoGameStateJson());
}

void WebService::handleSokobanStart() {
    _displayService->startSokobanGame();
    _server.send(200, "application/json", _displayService->getSokobanStateJson());
}

void WebService::handleSokobanMove() {
    if (!_displayService->isSokobanGameActive()) {
        _server.send(409, "application/json", "{\"error\":\"game is not active\"}");
        return;
    }
    if (!_server.hasArg("direction")) {
        _server.send(400, "application/json", "{\"error\":\"missing direction\"}");
        return;
    }
    const String direction = _server.arg("direction");
    int8_t dx = 0;
    int8_t dy = 0;
    if (direction == "up") dy = -1;
    else if (direction == "down") dy = 1;
    else if (direction == "left") dx = -1;
    else if (direction == "right") dx = 1;
    else {
        _server.send(400, "application/json", "{\"error\":\"unknown direction\"}");
        return;
    }
    _displayService->moveSokoban(dx, dy);
    _server.send(200, "application/json", _displayService->getSokobanStateJson());
}

void WebService::handleSokobanUndo() {
    if (!_displayService->isSokobanGameActive()) {
        _server.send(409, "application/json", "{\"error\":\"game is not active\"}");
        return;
    }
    _displayService->undoSokoban();
    _server.send(200, "application/json", _displayService->getSokobanStateJson());
}

void WebService::handleSokobanRestart() {
    if (!_displayService->isSokobanGameActive()) {
        _server.send(409, "application/json", "{\"error\":\"game is not active\"}");
        return;
    }
    _displayService->restartSokoban();
    _server.send(200, "application/json", _displayService->getSokobanStateJson());
}

void WebService::handleSokobanLevel() {
    if (!_displayService->isSokobanGameActive()) {
        _server.send(409, "application/json", "{\"error\":\"game is not active\"}");
        return;
    }
    if (!_server.hasArg("index")) {
        _server.send(400, "application/json", "{\"error\":\"missing level\"}");
        return;
    }
    const int index = _server.arg("index").toInt();
    if (index < 1 || !_displayService->selectSokobanLevel(index - 1)) {
        _server.send(400, "application/json", "{\"error\":\"invalid level\"}");
        return;
    }
    _server.send(200, "application/json", _displayService->getSokobanStateJson());
}

void WebService::handleSokobanExit() {
    _displayService->exitSokobanGame();
    _server.send(200, "application/json", _displayService->getSokobanStateJson());
}

void WebService::handleSokobanState() {
    _server.send(200, "application/json", _displayService->getSokobanStateJson());
}

void WebService::handleArcadeStart() {
    if (!_server.hasArg("id") ||
        !_displayService->startArcadeGame(_server.arg("id"))) {
        _server.send(400, "application/json",
                     "{\"error\":\"unknown game\"}");
        return;
    }
    _server.send(200, "application/json",
                 _displayService->getArcadeGameStateJson());
}

void WebService::handleArcadeAction() {
    if (!_displayService->isGameActive()) {
        _server.send(409, "application/json",
                     "{\"error\":\"game is not active\"}");
        return;
    }
    if (!_server.hasArg("action")) {
        _server.send(400, "application/json",
                     "{\"error\":\"missing action\"}");
        return;
    }
    const int value = _server.hasArg("value")
        ? _server.arg("value").toInt() : 0;
    const bool changed =
        _displayService->handleArcadeAction(_server.arg("action"), value);
    String json = _displayService->getArcadeGameStateJson();
    if (!changed && json.indexOf("\"game_over\"") < 0) {
        // 无效移动不是协议错误，客户端仍需拿到最新状态。
        _server.send(200, "application/json", json);
        return;
    }
    _server.send(200, "application/json", json);
}

void WebService::handleArcadeExit() {
    _displayService->exitArcadeGame();
    _server.send(200, "application/json",
                 "{\"active\":false,\"state\":\"closed\"}");
}

void WebService::handleArcadeState() {
    const String id = _server.hasArg("id") ? _server.arg("id") : "";
    _server.send(200, "application/json",
                 _displayService->getArcadeGameStateJson(id));
}

void WebService::handleArcadeCatalog() {
    _server.send(200, "application/json",
        "{\"games\":["
        "{\"id\":\"dino\",\"name\":\"DINO.RUN\",\"controls\":\"jump\"},"
        "{\"id\":\"sokoban\",\"name\":\"BOX.PUSH\",\"controls\":\"dpad\"},"
        "{\"id\":\"tetris\",\"name\":\"TETRIS\",\"controls\":\"five-key\"},"
        "{\"id\":\"snake\",\"name\":\"SNAKE\",\"controls\":\"dpad\"},"
        "{\"id\":\"2048\",\"name\":\"2048\",\"controls\":\"swipe\"},"
        "{\"id\":\"breakout\",\"name\":\"BREAKOUT\",\"controls\":\"paddle\"}"
        "]}");
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

void WebService::sendSalaryStatus(int statusCode) {
    SalaryCounterService* counter = _displayService->salaryCounter();
    if (!counter) {
        _server.send(503, "application/json",
                     "{\"error\":\"salary module unavailable\"}");
        return;
    }

    String json = "{\"state\":\"";
    json += counter->getStateName();
    json += "\",\"configured\":";
    json += counter->isConfigured() ? "true" : "false";
    json += ",\"activeSeconds\":";
    json += counter->getActiveSeconds();
    json += ",\"earnedTenThousandths\":";
    appendUInt64(json, counter->getLiveEarnedTenThousandths());
    json += ",\"dailyTargetTenThousandths\":";
    appendUInt64(json, counter->getDailyTargetTenThousandths());
    json += ",\"rateTenThousandths\":";
    json += counter->getRateTenThousandthsPerSecond();
    json += ",\"progressPermille\":";
    json += _preferenceService->getSalaryScheduleProgressPermille(
        _timeService);
    json += "}";
    _server.send(statusCode, "application/json", json);
}

void WebService::handleSalaryStatus() {
    sendSalaryStatus();
}

void WebService::handleSalaryConfig() {
    SalaryCounterService* counter = _displayService->salaryCounter();
    if (!counter) {
        _server.send(503, "application/json",
                     "{\"error\":\"salary module unavailable\"}");
        return;
    }

    if (_server.method() == HTTP_GET) {
        String json = "{\"monthlyCents\":";
        json += counter->getMonthlyCents();
        json += ",\"workDaysX100\":";
        json += counter->getWorkDaysX100();
        json += ",\"workMinutesPerDay\":";
        json += counter->getWorkMinutesPerDay();
        json += ",\"autoEnabled\":";
        json += _preferenceService->getSalaryAutoEnabled()
            ? "true" : "false";
        json += ",\"startMinutes\":";
        json += _preferenceService->getSalaryStartMinutes();
        json += ",\"endMinutes\":";
        json += _preferenceService->getSalaryEndMinutes();
        json += ",\"locked\":";
        json += counter->isSessionActive() ? "true" : "false";
        json += "}";
        _server.send(200, "application/json", json);
        return;
    }

    JsonDocument doc;
    const DeserializationError error =
        deserializeJson(doc, _server.arg("plain"));
    if (error) {
        _server.send(400, "application/json",
                     "{\"error\":\"invalid JSON\"}");
        return;
    }
    const uint32_t monthlyCents = doc["monthlyCents"] | 0UL;
    const uint16_t workDaysX100 = doc["workDaysX100"] | 0;
    const uint16_t workMinutes = doc["workMinutesPerDay"] | 0;
    const bool autoEnabled = doc["autoEnabled"].is<bool>()
        ? doc["autoEnabled"].as<bool>()
        : _preferenceService->getSalaryAutoEnabled();
    const uint16_t startMinutes = doc["startMinutes"] |
        _preferenceService->getSalaryStartMinutes();
    const uint16_t endMinutes = doc["endMinutes"] |
        _preferenceService->getSalaryEndMinutes();
    if (startMinutes >= 1440 || endMinutes > 1440 ||
        startMinutes >= endMinutes) {
        _server.send(400, "application/json",
                     "{\"error\":\"invalid work schedule\"}");
        return;
    }
    if (!counter->configure(monthlyCents, workDaysX100, workMinutes)) {
        _server.send(409, "application/json",
                     "{\"error\":\"invalid or locked salary config\"}");
        return;
    }
    _preferenceService->setSalarySchedule(
        autoEnabled, startMinutes, endMinutes);
    _displayService->showSalaryCounter();
    _preferenceService->setStartupView(VIEW_SALARY);
    sendSalaryStatus();
}

void WebService::handleSalaryStart() {
    SalaryCounterService* counter = _displayService->salaryCounter();
    if (!counter) {
        _server.send(503, "application/json",
                     "{\"error\":\"salary module unavailable\"}");
        return;
    }
    uint32_t epoch = 0;
    JsonDocument doc;
    if (!_server.arg("plain").isEmpty() &&
        !deserializeJson(doc, _server.arg("plain"))) {
        epoch = doc["epoch"] | 0UL;
    }
    if (!counter->start(epoch)) {
        _server.send(409, "application/json",
                     "{\"error\":\"salary counter cannot start\"}");
        return;
    }
    _preferenceService->setStartupView(VIEW_SALARY);
    _displayService->showSalaryCounter();
    sendSalaryStatus();
}

void WebService::handleSalaryPause() {
    SalaryCounterService* counter = _displayService->salaryCounter();
    if (!counter || !counter->pause()) {
        _server.send(409, "application/json",
                     "{\"error\":\"salary counter is not running\"}");
        return;
    }
    _displayService->refreshSalaryCounter();
    sendSalaryStatus();
}

void WebService::handleSalaryResume() {
    SalaryCounterService* counter = _displayService->salaryCounter();
    if (!counter) {
        _server.send(503, "application/json",
                     "{\"error\":\"salary module unavailable\"}");
        return;
    }
    uint32_t epoch = 0;
    JsonDocument doc;
    if (!_server.arg("plain").isEmpty() &&
        !deserializeJson(doc, _server.arg("plain"))) {
        epoch = doc["epoch"] | 0UL;
    }
    if (!counter->resume(epoch)) {
        _server.send(409, "application/json",
                     "{\"error\":\"salary counter is not paused\"}");
        return;
    }
    _displayService->showSalaryCounter();
    sendSalaryStatus();
}

void WebService::handleSalaryFinish() {
    SalaryCounterService* counter = _displayService->salaryCounter();
    if (!counter || !counter->finish()) {
        _server.send(409, "application/json",
                     "{\"error\":\"salary counter is not active\"}");
        return;
    }
    _displayService->refreshSalaryCounter();
    sendSalaryStatus();
}

void WebService::handleSalaryReset() {
    SalaryCounterService* counter = _displayService->salaryCounter();
    if (!counter) {
        _server.send(503, "application/json",
                     "{\"error\":\"salary module unavailable\"}");
        return;
    }
    counter->reset();
    _displayService->refreshSalaryCounter();
    sendSalaryStatus();
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
        _preferenceService->setStartupView(
            constrain(_server.arg("startup").toInt(),
                      VIEW_EYES_NORMAL, VIEW_SALARY));
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
        if (theme < THEME_ORANGE_WHITE || theme >= THEME_COUNT) {
            _server.send(400, "application/json", "{\"error\":\"unknown theme\"}");
            return;
        }
        _preferenceService->setDisplayTheme(theme);
        const char* background = "#da1100";
        uint8_t brightness = 100;
        uint8_t nightBrightness = 25;
        ExpressionId expression = ExpressionId::NORMAL;
        if (theme == THEME_DARK_ORANGE) {
            background = "#050505";
            brightness = 80;
            nightBrightness = 15;
            expression = ExpressionId::GRUMPY;
        } else if (theme == THEME_MINT) {
            background = "#10251f";
            brightness = 85;
            nightBrightness = 20;
            expression = ExpressionId::CURIOUS;
        } else if (theme == THEME_PINK) {
            background = "#2a111d";
            brightness = 80;
            nightBrightness = 18;
            expression = ExpressionId::LOVE;
        }
        _preferenceService->setDefaultBgHex(background);
        _preferenceService->setBrightnessPercent(brightness);
        _preferenceService->setNightBrightnessPercent(nightBrightness);
        _preferenceService->setDefaultExpression(expression);
        _preferenceService->setExpressionMode(ExpressionMode::MANUAL);
        const uint16_t bg = _displayService->hexToRgb565(background);
        _displayService->setAnimBgColor(bg);
        _displayService->setDrawBgColor(bg);
        _displayService->setDisplayTheme(theme);
        _displayService->setExpression(expression);
        _displayService->setBrightnessPercent(brightness);
        LOG_INFO("Web", "Theme applied: %u", theme);
    }
    if (_server.hasArg("fontStyle")) {
        FontStyle style;
        if (!fontStyleFromName(_server.arg("fontStyle"), style)) {
            _server.send(400, "application/json", "{\"error\":\"unknown font style\"}");
            return;
        }
        _preferenceService->setFontStyle(style);
        _displayService->setFontStyle(style);
        LOG_INFO("Web", "Font style applied: %s", fontStyleToName(style));
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
        const uint8_t fixedView =
            static_cast<uint8_t>(_server.arg("carouselFixed").toInt());
        _preferenceService->setCarouselFixedView(fixedView);
        // 关闭轮播时，“固定页”就是用户期望下次上电恢复的页面。
        if (fixedView == VIEW_CLOCK || fixedView == VIEW_POMODORO ||
            fixedView == VIEW_WEATHER ||
            fixedView == VIEW_CRYPTO || fixedView == VIEW_MARKET ||
            fixedView == VIEW_SALARY || fixedView == VIEW_TIMETABLE) {
            _preferenceService->setStartupView(fixedView);
        }
    }
    if (_server.hasArg("carouselOrder")) {
        const String value = _server.arg("carouselOrder");
        uint8_t order[CAROUSEL_MAX_VIEW_COUNT] = {};
        uint8_t index = 0;
        int start = 0;
        while (index < CAROUSEL_MAX_VIEW_COUNT && start >= 0) {
            const int comma = value.indexOf(',', start);
            const String part = value.substring(start,
                comma < 0 ? value.length() : comma);
            order[index++] = static_cast<uint8_t>(part.toInt());
            start = comma < 0 ? -1 : comma + 1;
        }
        if (index == 0 ||
            !_preferenceService->setCarouselOrder(order, index)) {
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
    String j = "{\"version\":\""; j += APP_VERSION;
    j += "\",\"view\":"; j += _displayService->getInteractiveView();
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

void WebService::handleConfigExport() {
    if (!_preferenceService) {
        _server.send(503, "application/json", "{\"error\":\"configuration unavailable\"}");
        return;
    }
    String json = "{\"version\":1,\"profile\":";
    json += _preferenceService->getProfileJson();
    json += ",\"preferences\":";
    json += _preferenceService->getJson();
    json += "}";
    _server.sendHeader("Content-Disposition",
                       "attachment; filename=\"clawd-mochi-config.json\"");
    _server.send(200, "application/json", json);
}

void WebService::handleConfigImport() {
    if (!_preferenceService) {
        _server.send(503, "application/json", "{\"error\":\"configuration unavailable\"}");
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, _server.arg("plain"))) {
        _server.send(400, "application/json", "{\"error\":\"invalid json\"}");
        return;
    }
    JsonObject profile = doc["profile"];
    JsonObject prefs = doc["preferences"];
    if ((doc["version"] | 0) != 1 || profile.isNull() || prefs.isNull()) {
        _server.send(400, "application/json", "{\"error\":\"unsupported configuration\"}");
        return;
    }

    const String deviceName = profile["deviceName"] | "";
    const String bootLine1 = profile["bootLine1"] | "";
    const String bootLine2 = profile["bootLine2"] | "";
    const String expressionName = profile["defaultExpression"] | "";
    const String modeName = profile["expressionMode"] | "";
    const String bg = prefs["bg"] | "";
    ExpressionId expression;
    const uint8_t startup = prefs["startup"] | 255;
    const uint8_t theme = prefs["theme"] | 255;
    FontStyle fontStyle;
    const String fontStyleName = prefs["fontStyle"] | "pixel";
    const uint8_t speed = prefs["speed"] | 0;
    const uint8_t brightness = prefs["brightness"] | 255;
    const uint8_t nightStart = prefs["nightStart"] | 255;
    const uint8_t nightEnd = prefs["nightEnd"] | 255;
    const uint8_t nightBrightness = prefs["nightBrightness"] | 255;
    bool validBg = bg.length() == 7 && bg[0] == '#';
    for (size_t i = 1; validBg && i < bg.length(); i++) {
        validBg = isxdigit(static_cast<unsigned char>(bg[i]));
    }
    if (!PreferenceService::isValidProfileText(deviceName, 12, false) ||
        !PreferenceService::isValidProfileText(bootLine1, 16, true) ||
        !PreferenceService::isValidProfileText(bootLine2, 16, true) ||
        !expressionIdFromName(expressionName, expression) ||
        (modeName != "auto" && modeName != "manual") ||
        (startup != VIEW_EYES_NORMAL && startup != VIEW_EYES_SQUISH &&
         startup != VIEW_CLOCK && startup != VIEW_POMODORO &&
         startup != VIEW_WEATHER && startup != VIEW_CRYPTO &&
         startup != VIEW_MARKET && startup != VIEW_SALARY) ||
        !validBg || theme >= THEME_COUNT || speed < 1 || speed > 3 ||
        !fontStyleFromName(fontStyleName, fontStyle) ||
        brightness > 100 || nightStart > 23 || nightEnd > 23 ||
        nightBrightness > 100 ||
        !prefs["nightDim"].is<bool>() ||
        !prefs["claudeStatus"].is<bool>() ||
        !prefs["carousel"].is<bool>()) {
        _server.send(400, "application/json", "{\"error\":\"invalid configuration values\"}");
        return;
    }
    JsonArray orderJson = prefs["carouselOrder"];
    if (orderJson.size() == 0 || orderJson.size() > CAROUSEL_MAX_VIEW_COUNT) {
        _server.send(400, "application/json", "{\"error\":\"invalid carousel order\"}");
        return;
    }
    const uint8_t orderCount = static_cast<uint8_t>(orderJson.size());
    uint8_t order[CAROUSEL_MAX_VIEW_COUNT] = {};
    for (uint8_t i = 0; i < orderCount; i++) order[i] = orderJson[i] | 255;
    if (!_preferenceService->setCarouselOrder(order, orderCount)) {
        _server.send(400, "application/json", "{\"error\":\"invalid carousel order\"}");
        return;
    }

    _preferenceService->setDeviceName(deviceName);
    _preferenceService->setBootLine1(bootLine1);
    _preferenceService->setBootLine2(bootLine2);
    _preferenceService->setDefaultExpression(expression);
    const ExpressionMode mode = modeName == "auto"
        ? ExpressionMode::AUTO : ExpressionMode::MANUAL;
    _preferenceService->setExpressionMode(mode);
    _preferenceService->setDefaultBgHex(bg);
    _preferenceService->setAnimSpeed(speed);
    _preferenceService->setBrightnessPercent(brightness);
    _preferenceService->setClaudeStatusEnabled(prefs["claudeStatus"]);
    _preferenceService->setDisplayTheme(theme);
    _preferenceService->setFontStyle(fontStyle);
    _preferenceService->setCarouselEnabled(prefs["carousel"]);
    _preferenceService->setCarouselSpeedSeconds(prefs["carouselSpeed"] | 12);
    _preferenceService->setCarouselFixedView(prefs["carouselFixed"] | VIEW_WEATHER);
    _preferenceService->setStartupView(startup);
    _preferenceService->setNightDimEnabled(prefs["nightDim"]);
    _preferenceService->setNightHours(nightStart, nightEnd);
    _preferenceService->setNightBrightnessPercent(nightBrightness);

    const uint16_t bgColor = _displayService->hexToRgb565(bg);
    _displayService->setAnimBgColor(bgColor);
    _displayService->setDrawBgColor(bgColor);
    _displayService->setAnimSpeed(speed);
    _displayService->setBrightnessPercent(brightness);
    _displayService->setClaudeStatusEnabled(prefs["claudeStatus"]);
    _displayService->setDisplayTheme(theme);
    _displayService->setFontStyle(fontStyle);
    _displayService->setExpression(expression);
    _displayService->setExpressionMode(mode);
    _displayService->reloadIdleDisplayPreferences();
    LOG_INFO("Web", "Configuration imported: device=%s theme=%u",
             deviceName.c_str(), theme);
    handleConfigExport();
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

void WebService::handleTimetableGet() {
    if (!_timetableService) {
        _server.send(503, "application/json", "{\"error\":\"service unavailable\"}");
        return;
    }
    String payload;
    if (!_timetableService->loadJson(payload)) {
        _server.send(200, "application/json",
            "{\"schemaVersion\":1,\"school\":\"GDUFS\",\"termStart\":\"\","
            "\"courses\":[]}");
        return;
    }
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", payload);
}

void WebService::handleTimetableSave() {
    if (!_timetableService || !_server.hasArg("plain")) {
        _server.send(400, "application/json", "{\"error\":\"JSON body required\"}");
        return;
    }
    String error;
    if (!_timetableService->saveJson(_server.arg("plain"), error)) {
        JsonDocument response;
        response["error"] = error;
        String payload;
        serializeJson(response, payload);
        _server.send(400, "application/json", payload);
        return;
    }
    if (_displayService &&
        _displayService->getInteractiveView() == VIEW_TIMETABLE) {
        _displayService->redrawCurrentView();
    }
    _server.send(200, "application/json", "{\"ok\":true}");
}

void WebService::handleTimetableStatus() {
    TimetableSnapshot snapshot = {};
    const bool ready = _timetableService &&
        _timetableService->getSnapshot(_timeService, snapshot);
    JsonDocument doc;
    doc["configured"] = _timetableService && _timetableService->isConfigured();
    doc["ready"] = ready;
    const char* state = "not_configured";
    switch (snapshot.state) {
        case TimetableState::NO_CLASS_TODAY: state = "no_class_today"; break;
        case TimetableState::NEXT_CLASS: state = "next_class"; break;
        case TimetableState::IN_CLASS: state = "in_class"; break;
        case TimetableState::ALL_DONE: state = "all_done"; break;
        default: break;
    }
    doc["state"] = state;
    doc["week"] = snapshot.academicWeek;
    doc["weekday"] = snapshot.weekday;
    doc["todayTotal"] = snapshot.todayTotal;
    doc["todayCompleted"] = snapshot.todayCompleted;
    doc["remainingToday"] = snapshot.remainingToday;
    doc["minutesRemaining"] = snapshot.minutesRemaining;
    if (snapshot.course.name[0]) {
        JsonObject course = doc["course"].to<JsonObject>();
        course["name"] = snapshot.course.name;
        course["shortName"] = snapshot.course.shortName;
        course["room"] = snapshot.course.room;
        course["teacher"] = snapshot.course.teacher;
        course["start"] = snapshot.course.start;
        course["end"] = snapshot.course.end;
        course["day"] = snapshot.course.weekday;
    }
    doc["hasNextCourse"] = snapshot.hasNextCourse;
    if (snapshot.hasNextCourse) {
        JsonObject nextCourse = doc["nextCourse"].to<JsonObject>();
        nextCourse["name"] = snapshot.nextCourse.name;
        nextCourse["shortName"] = snapshot.nextCourse.shortName;
        nextCourse["room"] = snapshot.nextCourse.room;
        nextCourse["teacher"] = snapshot.nextCourse.teacher;
        nextCourse["start"] = snapshot.nextCourse.start;
        nextCourse["end"] = snapshot.nextCourse.end;
        nextCourse["day"] = snapshot.nextCourse.weekday;
    }
    String payload;
    serializeJson(doc, payload);
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", payload);
}

void WebService::handleWakeUpProxy() {
    // 浏览器负责签名、解密和课表解析；设备只允许转发到这两个固定端点，
    // 避免把它变成任意 URL 代理。请求和响应均不写入 LittleFS/NVS。
    const String stage = _server.arg("stage");
    const char* path = nullptr;
    if (stage == "antispam") path = "/pluto/app/antispam";
    else if (stage == "share") path = "/share_schedule/getv2";
    else {
        _server.send(400, "application/json", "{\"error\":\"invalid WakeUp stage\"}");
        return;
    }
    if (!_wifiService || !_wifiService->isConnected()) {
        _server.send(503, "application/json", "{\"error\":\"internet connection required\"}");
        return;
    }
    const String body = _server.arg("plain");
    if (body.isEmpty() || body.length() > 12288) {
        _server.send(413, "application/json", "{\"error\":\"invalid WakeUp request\"}");
        return;
    }
    auto validId = [](const String& value, size_t maxLen) {
        if (value.length() > maxLen) return false;
        for (size_t i = 0; i < value.length(); i++) {
            const char c = value[i];
            if (!isalnum(static_cast<unsigned char>(c)) && c != '|' &&
                c != '-' && c != '_') return false;
        }
        return true;
    };
    const String cuid = _server.arg("cuid");
    const String adid = _server.arg("adid");
    const String did = _server.arg("did");
    if (!validId(cuid, 48) || !validId(adid, 48) || !validId(did, 64)) {
        _server.send(400, "application/json", "{\"error\":\"invalid temporary device id\"}");
        return;
    }
    if (!MemoryMonitor::hasTlsHeadroom("WakeUpImport") ||
        !NetworkRequestGate::tryAcquire()) {
        _server.send(503, "application/json", "{\"error\":\"network busy, try again\"}");
        return;
    }

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setConnectTimeout(8000);
    http.setTimeout(15000);
    const String url = String("https://api.wakeup.fun") + path;
    if (!http.begin(client, url)) {
        NetworkRequestGate::release();
        _server.send(502, "application/json", "{\"error\":\"WakeUp connection failed\"}");
        return;
    }
    http.useHTTP10(true);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");
    http.addHeader("Accept-Encoding", "identity");
    http.addHeader("na__zyb_source__", "wakeup");
    if (!cuid.isEmpty()) http.addHeader("zyb-cuid", cuid);
    if (!adid.isEmpty()) http.addHeader("zyb-adid", adid);
    if (!did.isEmpty()) http.addHeader("zyb-did", did);
    const int status = http.POST(body);
    String response;
    if (status > 0) response = http.getString();
    http.end();
    NetworkRequestGate::release();
    if (status <= 0) {
        _server.send(502, "application/json", "{\"error\":\"WakeUp request failed\"}");
        return;
    }
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(status, "application/json", response);
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

void WebService::handleOtaStatus() {
    if (!_otaService) { _server.send(503, "application/json", "{\"error\":\"ota unavailable\"}"); return; }
    _server.sendHeader("Access-Control-Allow-Origin", "*");
    bool bootPending = false;
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t imageState;
    if (running && esp_ota_get_state_partition(running, &imageState) == ESP_OK) {
        bootPending = imageState == ESP_OTA_IMG_PENDING_VERIFY;
    }
    String json = "{\"version\":\"" + _otaService->currentVersion() +
        "\",\"latestVersion\":\"" + _otaService->latestVersion() +
        "\",\"channel\":\"" + _otaService->channel() +
        "\",\"state\":\"" + String(_otaService->stateText()) +
        "\",\"available\":" + String(_otaService->updateAvailable() ? "true" : "false") +
        ",\"bootPending\":" + String(bootPending ? "true" : "false") +
        ",\"progress\":" + String(_otaService->progressBytes()) +
        ",\"total\":" + String(_otaService->totalBytes()) +
        ",\"lastCheck\":\"" + _otaService->lastCheck() +
        "\",\"error\":\"" + _otaService->lastError() +
        "\",\"releaseNotes\":\"" + _otaService->releaseNotes() + "\"}";
    _server.send(200, "application/json", json);
}

void WebService::handleOtaCheck() {
    _server.sendHeader("Access-Control-Allow-Origin", "*");
    if (!_otaService || !_otaService->checkNow()) {
        handleOtaStatus();
        return;
    }
    handleOtaStatus();
}

void WebService::handleOtaInstall() {
    _server.sendHeader("Access-Control-Allow-Origin", "*");
    if (!_otaService || !_otaService->updateAvailable()) {
        _server.send(409, "application/json", "{\"error\":\"no update available\"}");
        return;
    }
    if (!_otaService->installRemote()) handleOtaStatus();
}

void WebService::handleOtaUploadData() {
    if (!_otaService) return;
    HTTPUpload& upload = _server.upload();
    switch (upload.status) {
        case UPLOAD_FILE_START:
            if (!_otaService->beginUpload(upload.filename, upload.totalSize)) {
                _otaService->cancel();
            }
            break;
        case UPLOAD_FILE_WRITE:
            if (!_otaService->writeUpload(upload.buf, upload.currentSize)) {
                _otaService->cancel();
            }
            break;
        case UPLOAD_FILE_END:
            if (!_otaService->finishUpload()) _otaService->cancel();
            break;
        case UPLOAD_FILE_ABORTED:
            _otaService->abortUpload();
            break;
    }
}

void WebService::handleOtaUpload() {
    if (!_otaService) { _server.send(503, "application/json", "{\"error\":\"ota unavailable\"}"); return; }
    _server.sendHeader("Access-Control-Allow-Origin", "*");
    if (_otaService->state() == OtaService::State::REBOOTING) return;
    const bool failed = _otaService->state() == OtaService::State::FAILED;
    _server.send(failed ? 400 : 200, "application/json",
                 failed ? "{\"ok\":false,\"error\":\"ota upload failed\"}" : "{\"ok\":true}");
}

void WebService::handleOtaCancel() {
    if (_otaService) _otaService->cancel();
    _server.sendHeader("Access-Control-Allow-Origin", "*");
    _server.send(200, "application/json", "{\"ok\":true}");
}

void WebService::handleOtaOptions() {
    _server.sendHeader("Access-Control-Allow-Origin", "*");
    _server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
    _server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    _server.send(204, "text/plain", "");
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

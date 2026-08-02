#include "ota_service.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <LittleFS.h>
#include <mbedtls/sha256.h>
#include <esp_ota_ops.h>
#include <Preferences.h>

#include "wifi_config_service.h"
#include "time_service.h"
#include "../config/app_config.h"
#include "../config/cfg_ota.h"
#include "../utils/logger.h"
#include "../utils/memory_monitor.h"
#include "../utils/network_request_gate.h"

namespace {
uint32_t dateKey(TimeService* time) {
    return static_cast<uint32_t>(time->getYear() * 10000 +
                                 time->getMonth() * 100 + time->getDay());
}

String sha256Hex(const uint8_t digest[32]) {
    static const char* hex = "0123456789abcdef";
    String result;
    result.reserve(64);
    for (uint8_t i = 0; i < 32; i++) {
        const uint8_t byte = digest[i];
        result += hex[byte >> 4];
        result += hex[byte & 0x0f];
    }
    return result;
}
}

OtaService::OtaService(WifiConfigService* wifi, TimeService* time)
    : _wifi(wifi), _time(time), _state(State::IDLE), _currentVersion(APP_VERSION),
      _channel("stable"), _uploadType(U_FLASH), _uploadActive(false),
      _dailyCheckDone(false), _lastCheckDate(0), _progressBytes(0),
      _totalBytes(0), _updater(&Update) {}

void OtaService::init() {
    Preferences prefs;
    if (prefs.begin("clawd-ota", true)) {
        _lastCheckDate = prefs.getUInt("checkDate", 0);
        _lastCheck = prefs.getString("lastCheck", "");
        prefs.end();
    }
    LOG_INFO("OTA", "OTA 服务初始化,当前版本: %s", _currentVersion.c_str());
}

void OtaService::update() {
    if (!_wifi->isConnected() || !_time->isSynced() || _uploadActive ||
        _state == State::DOWNLOADING || _state == State::CHECKING) return;
    const uint32_t today = dateKey(_time);
    if (today == _lastCheckDate) return;
    if (_time->getHour() != CFG_OTA_CHECK_HOUR ||
        _time->getMinute() != CFG_OTA_CHECK_MINUTE) return;
    checkNow();
}

bool OtaService::shouldCheckToday() const {
    return _time->isSynced() && dateKey(_time) != _lastCheckDate;
}

bool OtaService::isNewerVersion(const String& candidate) const {
    int a[3] = {}, b[3] = {};
    sscanf(_currentVersion.c_str(), "%d.%d.%d", &a[0], &a[1], &a[2]);
    sscanf(candidate.c_str(), "%d.%d.%d", &b[0], &b[1], &b[2]);
    for (uint8_t i = 0; i < 3; i++) {
        if (b[i] != a[i]) return b[i] > a[i];
    }
    return candidate != _currentVersion && candidate > _currentVersion;
}

bool OtaService::parseManifest(const String& payload) {
    JsonDocument doc;
    if (deserializeJson(doc, payload)) {
        setFailure("invalid manifest");
        return false;
    }
    const String product = doc["product"] | "";
    if ((doc["schema"] | 0) != 1 || product != "clawd-mochi") {
        setFailure("unsupported manifest");
        return false;
    }
    const String board = doc["board"] | "";
    if (board.length() && board != "esp32-c3-devkitc-02") {
        setFailure("wrong board");
        return false;
    }
    const String version = doc["version"] | "";
    const JsonObject firmware = doc["firmware"];
    if (version.length() == 0 || firmware.isNull() ||
        String(firmware["url"] | "").length() == 0) {
        setFailure("incomplete manifest");
        return false;
    }
    _latestVersion = version;
    _firmwareUrl = firmware["url"] | "";
    _firmwareSha256 = firmware["sha256"] | "";
    _filesystemUrl = doc["filesystem"]["url"] | "";
    _filesystemSha256 = doc["filesystem"]["sha256"] | "";
    _totalBytes = firmware["size"] | 0U;
    _channel = doc["channel"] | "stable";
    _releaseNotes = "";
    for (JsonVariant note : doc["releaseNotes"].as<JsonArray>()) {
        if (_releaseNotes.length()) _releaseNotes += "\n";
        _releaseNotes += note.as<String>();
        if (_releaseNotes.length() > 512) break;
    }
    return true;
}

bool OtaService::checkNow() {
    if (!_wifi->isConnected() || !MemoryMonitor::hasTlsHeadroom("OTA check")) {
        setFailure("network unavailable");
        return false;
    }
    if (!NetworkRequestGate::tryAcquire()) {
        setFailure("network busy");
        return false;
    }
    _state = State::CHECKING;
    WiFiClient plainClient;
    WiFiClientSecure secureClient;
    WiFiClient* client = &plainClient;
    if (String(CFG_OTA_MANIFEST_URL).startsWith("https://")) {
        if (strlen(CFG_OTA_ROOT_CA)) secureClient.setCACert(CFG_OTA_ROOT_CA);
        else secureClient.setInsecure(); // Development default; set CA for releases.
        client = &secureClient;
    }
    HTTPClient http;
    http.setTimeout(CFG_OTA_HTTP_TIMEOUT_MS);
    bool ok = http.begin(*client, CFG_OTA_MANIFEST_URL);
    if (ok) {
        const int code = http.GET();
        if (code == HTTP_CODE_OK && http.getSize() <= CFG_OTA_MAX_MANIFEST_BYTES) {
            ok = parseManifest(http.getString());
        } else {
            setFailure(String("manifest HTTP ") + code);
            ok = false;
        }
    }
    http.end();
    NetworkRequestGate::release();
    if (!ok) return false;
    _lastCheck = _time->getTimestamp();
    if (shouldCheckToday()) {
        _lastCheckDate = dateKey(_time);
        Preferences prefs;
        if (prefs.begin("clawd-ota", false)) {
            prefs.putUInt("checkDate", _lastCheckDate);
            prefs.putString("lastCheck", _lastCheck);
            prefs.end();
        }
    }
    _state = isNewerVersion(_latestVersion) ? State::AVAILABLE : State::UP_TO_DATE;
    _lastError = "";
    return true;
}

bool OtaService::downloadToUpdate(const String& url, size_t expectedSize,
                                  const String& expectedSha256, uint8_t command) {
    if (!NetworkRequestGate::tryAcquire() ||
        !MemoryMonitor::hasTlsHeadroom("OTA download")) {
        setFailure("insufficient network memory");
        return false;
    }
    WiFiClient plainClient;
    WiFiClientSecure secureClient;
    WiFiClient* client = &plainClient;
    if (url.startsWith("https://")) {
        if (strlen(CFG_OTA_ROOT_CA)) secureClient.setCACert(CFG_OTA_ROOT_CA);
        else secureClient.setInsecure(); // Development default; set CA for releases.
        client = &secureClient;
    }
    HTTPClient http;
    http.setTimeout(CFG_OTA_HTTP_TIMEOUT_MS);
    bool ok = http.begin(*client, url);
    if (!ok || http.GET() != HTTP_CODE_OK) {
        setFailure("firmware download failed");
        if (ok) http.end();
        NetworkRequestGate::release();
        return false;
    }
    const int length = http.getSize();
    if (expectedSize && length > 0 && static_cast<size_t>(length) != expectedSize) {
        setFailure("firmware size mismatch");
        http.end();
        NetworkRequestGate::release();
        return false;
    }
    if (command == U_SPIFFS) LittleFS.end();
    if (!Update.begin(expectedSize ? expectedSize : UPDATE_SIZE_UNKNOWN, command)) {
        setFailure("update partition unavailable");
        if (command == U_SPIFFS) LittleFS.begin(false);
        http.end();
        NetworkRequestGate::release();
        return false;
    }
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts_ret(&sha, 0);
    WiFiClient* stream = http.getStreamPtr();
    uint8_t buffer[1024];
    _progressBytes = 0;
    while (http.connected() && (_progressBytes < static_cast<size_t>(length) || length < 0)) {
        const size_t available = stream->available();
        if (!available) { delay(1); continue; }
        const size_t count = stream->readBytes(buffer, min(available, sizeof(buffer)));
        if (Update.write(buffer, count) != count) { Update.abort(); ok = false; break; }
        mbedtls_sha256_update_ret(&sha, buffer, count);
        _progressBytes += count;
    }
    uint8_t digest[32];
    mbedtls_sha256_finish_ret(&sha, digest);
    mbedtls_sha256_free(&sha);
    const String actual = sha256Hex(digest);
    if (ok && expectedSha256.length() && !actual.equalsIgnoreCase(expectedSha256)) ok = false;
    if (ok) ok = Update.end(true);
    else Update.abort();
    http.end();
    if (command == U_SPIFFS) LittleFS.begin(false);
    NetworkRequestGate::release();
    if (!ok) setFailure("OTA verification failed");
    return ok;
}

bool OtaService::installRemote() {
    if (_state != State::AVAILABLE || _firmwareUrl.length() == 0) return false;
    _state = State::DOWNLOADING;
    if (!downloadToUpdate(_firmwareUrl, _totalBytes, _firmwareSha256, U_FLASH)) return false;
    if (_filesystemUrl.length()) {
        _state = State::DOWNLOADING;
        if (!downloadToUpdate(_filesystemUrl, 0, _filesystemSha256, U_SPIFFS)) {
            const esp_partition_t* running = esp_ota_get_running_partition();
            if (running) esp_ota_set_boot_partition(running);
            return false;
        }
    }
    _state = State::REBOOTING;
    delay(250);
    ESP.restart();
    return true;
}

bool OtaService::beginUpload(const String& filename, size_t totalSize) {
    if (_uploadActive || filename.length() == 0) return false;
    _uploadFilename = filename;
    _uploadType = filename.endsWith(".littlefs.bin") || filename.endsWith(".spiffs.bin")
        ? U_SPIFFS : U_FLASH;
    if (_uploadType == U_FLASH && totalSize > ESP.getFreeSketchSpace()) return false;
    if (_uploadType == U_SPIFFS) LittleFS.end();
    if (!Update.begin(totalSize ? totalSize : UPDATE_SIZE_UNKNOWN, _uploadType)) {
        if (_uploadType == U_SPIFFS) LittleFS.begin(false);
        return false;
    }
    _uploadActive = true;
    _state = State::UPLOADING;
    _progressBytes = 0;
    _totalBytes = totalSize;
    _lastError = "";
    return true;
}

bool OtaService::writeUpload(const uint8_t* data, size_t length) {
    if (!_uploadActive || Update.write(const_cast<uint8_t*>(data), length) != length) {
        abortUpload();
        return false;
    }
    _progressBytes += length;
    return true;
}

bool OtaService::finishUpload() {
    if (!_uploadActive) return false;
    _state = State::VERIFYING;
    const bool ok = Update.end(true);
    _uploadActive = false;
    if (_uploadType == U_SPIFFS) LittleFS.begin(false);
    if (!ok) { setFailure("local OTA verification failed"); return false; }
    _state = State::REBOOTING;
    delay(250);
    ESP.restart();
    return true;
}

void OtaService::abortUpload() {
    if (_uploadActive) Update.abort();
    _uploadActive = false;
    if (_uploadType == U_SPIFFS) LittleFS.begin(false);
    setFailure("upload aborted");
}

void OtaService::cancel() { if (_uploadActive) abortUpload(); else _state = State::IDLE; }

void OtaService::setFailure(const String& error) { _state = State::FAILED; _lastError = error; LOG_WARN("OTA", "%s", error.c_str()); }

const char* OtaService::stateText() const {
    switch (_state) {
        case State::CHECKING: return "checking"; case State::AVAILABLE: return "available";
        case State::UP_TO_DATE: return "up_to_date"; case State::DOWNLOADING: return "downloading";
        case State::UPLOADING: return "uploading"; case State::VERIFYING: return "verifying";
        case State::REBOOTING: return "rebooting"; case State::FAILED: return "failed";
        default: return "idle";
    }
}

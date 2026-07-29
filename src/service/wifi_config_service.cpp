#include "wifi_config_service.h"
#include "operation_mode_service.h"
#include "../utils/logger.h"
#include <ESPmDNS.h>

WifiConfigService* WifiConfigService::_instance = nullptr;

WifiConfigService::WifiConfigService()
    : _configured(false), _connected(false), _connecting(false)
    , _apStarted(false), _mdnsStarted(false), _connectStartTime(0)
    , _lastAttemptEndMs(0), _connectedSinceMs(0), _lastPowerAdjustMs(0)
    , _filteredRssi(0), _currentTxPower(WIFI_POWER_19_5dBm)
    , _wifiSleepEnabled(true), _radioProfileInitialized(false)
    , _connectPhase(ConnectPhase::ASSOCIATING), _lastDisconnectReason(0)
    , _lastError("Connection failed"), _retryCount(0), _retryExhausted(false)
    , _provMode(ProvisioningMode::NONE), _provModeStartMs(0)
{
}

void WifiConfigService::init() {
    ensureAccessPoint();
    WiFi.onEvent(onWifiEvent);
    loadCredentials();
    if (_configured) {
        LOG_INFO("WiFi", "已有凭据,连接: %s", _ssid.c_str());
        connectToWifi(_ssid.c_str(), _password.c_str());
    } else {
        startAPMode();
    }
}

// WiFi 事件回调:推进连接阶段,记录断开原因码
void WifiConfigService::onWifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
    WifiConfigService* self = _instance;
    if (!self) return;
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            // 认证已通过,进入 DHCP 阶段
            self->_connectPhase = ConnectPhase::OBTAINING_IP;
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            self->_lastDisconnectReason = info.wifi_sta_disconnected.reason;
            break;
        default:
            break;
    }
}

// 断开原因码 → 用户可读的失败提示(与手机一致的分类)
static const char* mapDisconnectReason(uint8_t reason) {
    switch (reason) {
        case WIFI_REASON_NO_AP_FOUND:
            return "Network not found";
        case WIFI_REASON_AUTH_EXPIRE:
        case WIFI_REASON_ASSOC_EXPIRE:
        case WIFI_REASON_BEACON_TIMEOUT:
            return "Weak signal / AP timeout";
        case WIFI_REASON_AUTH_FAIL:
            return "Wrong password";
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
            return "Authentication timeout";
        default:
            return "Connection failed";
    }
}

const char* WifiConfigService::getConnectPhaseText() const {
    if (_connected) return "Connected";
    return _connectPhase == ConnectPhase::OBTAINING_IP ? "Obtaining IP" : "Connecting";
}

void WifiConfigService::setProvisioningMode(ProvisioningMode mode) {
    if (_provMode == mode) return;
    _provMode = mode;
    _provModeStartMs = millis();
    LOG_INFO("WiFi", "配网状态: %s", getProvisioningMessage());
}

const char* WifiConfigService::getProvisioningMessage() const {
    switch (_provMode) {
        case ProvisioningMode::AP_FALLBACK: return "AP: 192.168.4.1";
        case ProvisioningMode::CONNECTING:  return "Connecting...";
        case ProvisioningMode::RETRY_WAIT:  return "Retry soon";
        case ProvisioningMode::CONNECTED:   return "Connected";
        default:                            return "";
    }
}

void WifiConfigService::ensureAccessPoint() {
    if (WiFi.getMode() != WIFI_AP_STA) {
        WiFi.mode(WIFI_AP_STA);
    }
    if (_apStarted) return;

    _apStarted = WiFi.softAP(CFG_WIFI_AP_SSID, CFG_WIFI_AP_PASSWORD, CFG_WIFI_AP_CHANNEL);
    if (_apStarted) {
        LOG_INFO("WiFi", "AP 模式: %s IP: %s", CFG_WIFI_AP_SSID, WiFi.softAPIP().toString().c_str());
    } else {
        LOG_WARN("WiFi", "AP 启动失败: %s", CFG_WIFI_AP_SSID);
    }
}

void WifiConfigService::startMDNS() {
    if (!_connected || _mdnsStarted) return;

    if (MDNS.begin(CFG_WIFI_MDNS_HOSTNAME)) {
        MDNS.addService("http", "tcp", CFG_WIFI_WEB_PORT);
        _mdnsStarted = true;
        LOG_INFO("WiFi", "mDNS 启动: %s.local", CFG_WIFI_MDNS_HOSTNAME);
    } else {
        LOG_WARN("WiFi", "mDNS 启动失败");
    }
}

void WifiConfigService::stopMDNS() {
    if (!_mdnsStarted) return;
    MDNS.end();
    _mdnsStarted = false;
}

void WifiConfigService::update() {
    ensureAccessPoint();

    if (_connected && WiFi.status() != WL_CONNECTED) {
        _connected = false;
        _lastAttemptEndMs = millis();
        stopMDNS();
        applyConnectingRadioProfile();
        LOG_WARN("WiFi", "STA disconnected, keeping AP control");
    }

    // 连接中
    if (_connecting) {
        if (WiFi.status() == WL_CONNECTED) {
            _connected = true;
            _connecting = false;
            _lastError = "";
            _retryCount = 0;
            _retryExhausted = false;
            _connectedSinceMs = millis();
            _lastPowerAdjustMs = 0;
            _filteredRssi = 0;
            startMDNS();
            setProvisioningMode(ProvisioningMode::CONNECTED);
            LOG_INFO("WiFi", "已连接: %s IP: %s RSSI: %d dBm CH: %d",
                     WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(),
                     WiFi.RSSI(), WiFi.channel());
        } else if (millis() - _connectStartTime > CFG_WIFI_CONNECT_TIMEOUT_MS) {
            _connecting = false;
            _lastAttemptEndMs = millis();
            _lastError = mapDisconnectReason(_lastDisconnectReason);
            LOG_WARN("WiFi", "连接超时: %s (reason=%d)", _lastError, _lastDisconnectReason);
            if (!_configured) {
                startAPMode();
            } else {
                if (_retryCount < 255) _retryCount++;
                if (_retryCount >= CFG_WIFI_MAX_RETRIES) {
                    // 打满最大重试次数:回 AP 配网模式,等用户重新配置
                    _retryExhausted = true;
                    LOG_WARN("WiFi", "已达最大重试次数 %d,回到配网模式", CFG_WIFI_MAX_RETRIES);
                    startAPMode();
                } else {
                    // 进入重试等待态,屏幕显示倒计时,避免空白
                    setProvisioningMode(ProvisioningMode::RETRY_WAIT);
                }
            }
        }
        return;
    }

    if (_connected) {
        updateConnectedRadioProfile();
    }

    // 已连接后:3 秒展示,然后回归 NONE
    if (_provMode == ProvisioningMode::CONNECTED) {
        if (millis() - _provModeStartMs > 3000) {
            setProvisioningMode(ProvisioningMode::NONE);
        }
        return;
    }

    // 常态:已配置但掉线,按连续失败次数渐进重连
    if (_configured && !_connected && !_connecting
        && _provMode != ProvisioningMode::AP_FALLBACK) {
        if (millis() - _lastAttemptEndMs >= getReconnectDelayMs()) {
            connectToWifi(_ssid.c_str(), _password.c_str());
        }
    }

    // 重试打满后:保留 5 分钟一次的慢速重试,路由器恢复后可自愈
    if (_retryExhausted && _configured && !_connected && !_connecting
        && millis() - _lastAttemptEndMs >= CFG_WIFI_SLOW_RETRY_INTERVAL_MS) {
        LOG_INFO("WiFi", "慢速重试连接...");
        connectToWifi(_ssid.c_str(), _password.c_str());
    }
}

void WifiConfigService::loadCredentials() {
    if (!LittleFS.exists(CFG_WIFI_CRED_PATH)) { _configured = false; return; }
    File file = LittleFS.open(CFG_WIFI_CRED_PATH, "r");
    if (!file) { _configured = false; return; }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    if (err) { _configured = false; return; }

    _ssid = doc["ssid"].as<String>();
    _password = doc["password"].as<String>();
    _configured = !_ssid.isEmpty();
}

void WifiConfigService::startAPMode() {
    ensureAccessPoint();
    LOG_INFO("WiFi", "AP 模式: %s IP: %s", CFG_WIFI_AP_SSID, WiFi.softAPIP().toString().c_str());
    _connected = false;
    setProvisioningMode(ProvisioningMode::AP_FALLBACK);
}

void WifiConfigService::skipProvisioning() {
    // 切换到串口模式:Claude 状态走 USB,但保留 WiFi/Web 本地遥控
    auto* opMode = OperationModeService::current();
    if (opMode) opMode->setMode(OperationModeService::Mode::SERIAL);
    _connecting = false;
    setProvisioningMode(ProvisioningMode::NONE);
    LOG_INFO("WiFi", "已切换到串口模式,保留 WiFi/Web 遥控");
}

bool WifiConfigService::connectToWifi(const char* ssid, const char* password) {
    stopMDNS();
    _connected = false;
    ensureAccessPoint();
    applyConnectingRadioProfile();
    WiFi.begin(ssid, password);
    _connecting = true;
    _connectStartTime = millis();
    _connectPhase = ConnectPhase::ASSOCIATING;
    _lastDisconnectReason = 0;
    setProvisioningMode(ProvisioningMode::CONNECTING);
    return true;
}

void WifiConfigService::saveCredentials(const char* ssid, const char* password) {
    JsonDocument doc;
    doc["ssid"] = ssid;
    doc["password"] = password;
    File file = LittleFS.open(CFG_WIFI_CRED_PATH, "w");
    if (file) { serializeJson(doc, file); file.close(); }
    _ssid = ssid;
    _password = password;
    _configured = true;
}

void WifiConfigService::clearCredentials() {
    LittleFS.remove(CFG_WIFI_CRED_PATH);
    _ssid = ""; _password = ""; _configured = false;
}

void WifiConfigService::reset() {
    clearCredentials();
    ESP.restart();
}

bool WifiConfigService::isConfigured() { return _configured; }
bool WifiConfigService::isConnected() { return _connected; }
unsigned long WifiConfigService::getRetryRemainingMs() const {
    unsigned long delayMs = getReconnectDelayMs();
    unsigned long elapsed = millis() - _lastAttemptEndMs;
    if (elapsed >= delayMs) return 0;
    return delayMs - elapsed;
}

unsigned long WifiConfigService::getReconnectDelayMs() const {
    switch (_retryCount) {
        case 0:
        case 1:  return CFG_WIFI_RETRY_DELAY_1_MS;
        case 2:  return CFG_WIFI_RETRY_DELAY_2_MS;
        case 3:  return CFG_WIFI_RETRY_DELAY_3_MS;
        case 4:  return CFG_WIFI_RETRY_DELAY_4_MS;
        default: return CFG_WIFI_RETRY_DELAY_5_MS;
    }
}

void WifiConfigService::setRadioProfile(wifi_power_t txPower, bool sleepEnabled,
                                        const char* profileName, int16_t rssi) {
    bool changed = !_radioProfileInitialized
        || txPower != _currentTxPower
        || sleepEnabled != _wifiSleepEnabled;
    if (!changed) return;

    if (!_radioProfileInitialized || txPower != _currentTxPower) {
        if (WiFi.setTxPower(txPower)) {
            _currentTxPower = txPower;
        } else {
            LOG_WARN("WiFi", "设置发射功率失败: %s", profileName);
        }
    }
    if (!_radioProfileInitialized || sleepEnabled != _wifiSleepEnabled) {
        WiFi.setSleep(sleepEnabled);
        _wifiSleepEnabled = sleepEnabled;
    }
    _radioProfileInitialized = true;

    if (rssi == 0) {
        LOG_INFO("WiFi", "射频策略: %s TX=%.1f dBm sleep=%s",
                 profileName, static_cast<int>(_currentTxPower) / 4.0f,
                 _wifiSleepEnabled ? "on" : "off");
    } else {
        LOG_INFO("WiFi", "射频策略: %s RSSI=%d dBm TX=%.1f dBm sleep=%s",
                 profileName, rssi, static_cast<int>(_currentTxPower) / 4.0f,
                 _wifiSleepEnabled ? "on" : "off");
    }
}

void WifiConfigService::applyConnectingRadioProfile() {
    // 扫描/认证/弱信号恢复阶段优先保证成功率
    setRadioProfile(WIFI_POWER_19_5dBm, false, "connecting", 0);
}

void WifiConfigService::updateConnectedRadioProfile() {
    unsigned long now = millis();
    if (now - _connectedSinceMs < CFG_WIFI_POWER_SETTLE_MS) return;
    if (_lastPowerAdjustMs != 0
        && now - _lastPowerAdjustMs < CFG_WIFI_POWER_ADJUST_INTERVAL_MS) {
        return;
    }
    _lastPowerAdjustMs = now;

    int16_t rssi = WiFi.RSSI();
    if (_filteredRssi == 0) {
        _filteredRssi = rssi;
    } else {
        // 简单低通滤波,避免瞬时波动导致功率档位频繁切换
        _filteredRssi = (_filteredRssi * 3 + rssi) / 4;
    }

    if (_filteredRssi >= CFG_WIFI_RSSI_EXCELLENT_DBM) {
        setRadioProfile(WIFI_POWER_8_5dBm, true, "excellent", _filteredRssi);
    } else if (_filteredRssi >= CFG_WIFI_RSSI_GOOD_DBM) {
        setRadioProfile(WIFI_POWER_13dBm, true, "good", _filteredRssi);
    } else if (_filteredRssi >= CFG_WIFI_RSSI_FAIR_DBM) {
        setRadioProfile(WIFI_POWER_17dBm, false, "fair", _filteredRssi);
    } else {
        // 弱信号时保持最高功率和常醒,避免为了省电牺牲稳定性
        setRadioProfile(WIFI_POWER_19_5dBm, false, "weak", _filteredRssi);
    }
}
bool WifiConfigService::isSerialMode() const {
    auto* opMode = OperationModeService::current();
    return opMode && opMode->isSerial();
}
String WifiConfigService::getIP() {
    return _connected ? getLanIP() : getAPIP();
}
String WifiConfigService::getLanIP() {
    return _connected ? WiFi.localIP().toString() : "";
}
String WifiConfigService::getAPIP() {
    return WiFi.softAPIP().toString();
}
String WifiConfigService::getMDNSUrl() const {
    return String("http://") + CFG_WIFI_MDNS_HOSTNAME + ".local";
}
String WifiConfigService::getSSID() { return _connected ? WiFi.SSID() : CFG_WIFI_AP_SSID; }

void WifiConfigService::handleScanRequest(WebServer& server) {
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n; i++) {
        if (i > 0) json += ",";
        json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i));
        json += ",\"encrypted\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false") + "}";
    }
    json += "]";
    server.send(200, "application/json", json);
    WiFi.scanDelete();
}

void WifiConfigService::handleConnectRequest(WebServer& server) {
    if (server.hasArg("ssid") && server.hasArg("password")) {
        String ssid = server.arg("ssid");
        String password = server.arg("password");
        saveCredentials(ssid.c_str(), password.c_str());
        // 用户手动提交新凭据:重置重试计数,重新进入快速重试循环
        _retryCount = 0;
        _retryExhausted = false;
        connectToWifi(ssid.c_str(), password.c_str());
        server.send(200, "application/json", "{\"status\":\"connecting\"}");
    } else {
        server.send(400, "application/json", "{\"error\":\"missing parameters\"}");
    }
}

void WifiConfigService::handleStatusRequest(WebServer& server) {
    String json = "{\"connected\":" + String(_connected ? "true" : "false");
    json += ",\"ssid\":\"" + getSSID() + "\",\"ip\":\"" + getIP() + "\"";
    json += ",\"lanIp\":\"" + getLanIP() + "\"";
    json += ",\"apIp\":\"" + getAPIP() + "\"";
    json += ",\"apSsid\":\"" + String(CFG_WIFI_AP_SSID) + "\"";
    json += ",\"mdns\":\"" + getMDNSUrl() + "\"";
    json += ",\"mdnsActive\":" + String(_mdnsStarted ? "true" : "false");
    json += ",\"configured\":" + String(_configured ? "true" : "false");
    json += ",\"savedSsid\":\"" + _ssid + "\"";
    json += ",\"lastError\":\"" + String(_lastError) + "\"";
    json += ",\"retryCount\":" + String(_retryCount);
    json += ",\"retryExhausted\":" + String(_retryExhausted ? "true" : "false");
    json += ",\"phase\":\"" + String(getConnectPhaseText()) + "\"";
    json += ",\"rssi\":" + String(WiFi.RSSI()) + "}";
    server.send(200, "application/json", json);
}

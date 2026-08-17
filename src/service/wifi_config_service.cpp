#include "wifi_config_service.h"
#include "operation_mode_service.h"
#include "../utils/logger.h"
#include <ESPmDNS.h>

WifiConfigService* WifiConfigService::_instance = nullptr;

WifiConfigService::WifiConfigService()
    : _configured(false), _connected(false), _connecting(false)
    , _apStarted(false), _dnsStarted(false), _mdnsStarted(false), _connectStartTime(0)
    , _lastAttemptEndMs(0), _connectedSinceMs(0), _lastPowerAdjustMs(0)
    , _filteredRssi(0), _currentTxPower(WIFI_POWER_19_5dBm)
    , _wifiSleepEnabled(true), _radioProfileInitialized(false)
    , _highThroughputMode(false)
    , _credentialPrefsReady(false), _offlineMode(false), _credentialChangePending(false)
    , _connectPhase(ConnectPhase::ASSOCIATING), _lastDisconnectReason(0)
    , _lastError(""), _retryCount(0), _retryExhausted(false)
    , _provMode(ProvisioningMode::NONE), _provModeStartMs(0)
{
}

void WifiConfigService::init() {
    ensureAccessPoint();
    WiFi.onEvent(onWifiEvent);
    _credentialPrefsReady = _credentialPrefs.begin(
        CFG_WIFI_NVS_NAMESPACE, false);
    if (!_credentialPrefsReady) {
        LOG_ERROR("WiFi", "WiFi NVS 初始化失败");
    }
    loadCredentials();
    _offlineMode = _credentialPrefsReady && _credentialPrefs.getBool(
        CFG_WIFI_NVS_OFFLINE_KEY, false);
    if (_offlineMode) {
        ensureAccessPoint();
        setProvisioningMode(ProvisioningMode::NONE);
        LOG_INFO("WiFi", "离线模式,仅启动本地 AP");
    } else if (_configured) {
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

static String escapeJsonString(const String& value) {
    String escaped;
    escaped.reserve(value.length() + 4);
    for (size_t i = 0; i < value.length(); i++) {
        const char c = value[i];
        if (c == '"' || c == '\\') escaped += '\\';
        if (static_cast<uint8_t>(c) >= 0x20) escaped += c;
    }
    return escaped;
}

const char* WifiConfigService::getConnectPhaseText() const {
    if (_connected) return "Connected";
    return _connectPhase == ConnectPhase::OBTAINING_IP ? "Obtaining IP" : "Connecting";
}

void WifiConfigService::setProvisioningMode(ProvisioningMode mode) {
    if (_provMode == mode) return;
    _provMode = mode;
    _provModeStartMs = millis();
    if (_provMode == ProvisioningMode::AP_FALLBACK ||
        (_provMode == ProvisioningMode::CONNECTING && !_configured)) {
        startCaptiveDns();
    } else {
        stopCaptiveDns();
    }
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
        if (_provMode == ProvisioningMode::AP_FALLBACK ||
            (_provMode == ProvisioningMode::CONNECTING && !_configured)) {
            startCaptiveDns();
        }
    } else {
        LOG_WARN("WiFi", "AP 启动失败: %s", CFG_WIFI_AP_SSID);
    }
}

void WifiConfigService::startCaptiveDns() {
    if (_dnsStarted || !_apStarted) return;

    const IPAddress apIp = WiFi.softAPIP();
    if (apIp == IPAddress(0, 0, 0, 0)) {
        LOG_WARN("WiFi", "Captive Portal DNS 启动失败: AP IP 无效");
        return;
    }
    _dnsServer.start(53, "*", apIp);
    _dnsStarted = true;
    LOG_INFO("WiFi", "Captive Portal DNS 启动: * -> %s", apIp.toString().c_str());
}

void WifiConfigService::stopCaptiveDns() {
    if (!_dnsStarted) return;
    _dnsServer.stop();
    _dnsStarted = false;
    LOG_INFO("WiFi", "Captive Portal DNS 停止");
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
    if (_dnsStarted) _dnsServer.processNextRequest();

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
            if (_credentialChangePending) {
                if (persistCredentials(_pendingSsid, _pendingPassword)) {
                    _ssid = _pendingSsid;
                    _password = _pendingPassword;
                    _configured = true;
                    _offlineMode = false;
                    if (_credentialPrefsReady) {
                        _credentialPrefs.putBool(CFG_WIFI_NVS_OFFLINE_KEY, false);
                    }
                    LOG_INFO("WiFi", "新网络验证成功并保存到 NVS: %s",
                             _ssid.c_str());
                } else {
                    LOG_ERROR("WiFi", "新网络已连接,但凭据写入 NVS 失败");
                }
                _pendingSsid = "";
                _pendingPassword = "";
                _credentialChangePending = false;
            }
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
            if (_credentialChangePending) {
                LOG_WARN("WiFi", "候选网络验证失败,保留原凭据: %s",
                         _ssid.c_str());
                _pendingSsid = "";
                _pendingPassword = "";
                _credentialChangePending = false;
                _retryCount = 0;
                _retryExhausted = false;
                if (_configured) {
                    setProvisioningMode(ProvisioningMode::RETRY_WAIT);
                } else {
                    startAPMode();
                }
                return;
            }
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
    _configured = false;
    if (_credentialPrefsReady) {
        _ssid = _credentialPrefs.getString(CFG_WIFI_NVS_SSID_KEY, "");
        _password = _credentialPrefs.getString(CFG_WIFI_NVS_PASS_KEY, "");
        _configured = isValidCredentialInput(_ssid, _password);
    }
    if (_configured) return;

    _ssid = "";
    _password = "";
    migrateLegacyCredentials();
}

void WifiConfigService::migrateLegacyCredentials() {
    // 遍历根目录而不是对不存在的文件调用 LittleFS.exists()，
    // 避免 vfs 在全新文件系统上打印误导性的 open() 错误。
    File root = LittleFS.open("/");
    if (!root || !root.isDirectory()) return;

    File legacy;
    for (File entry = root.openNextFile(); entry; entry = root.openNextFile()) {
        String name = entry.name();
        if (name == CFG_WIFI_LEGACY_CRED_PATH ||
            name == String(CFG_WIFI_LEGACY_CRED_PATH).substring(1)) {
            legacy = entry;
            break;
        }
    }
    if (!legacy) return;

    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, legacy);
    legacy.close();
    if (err) {
        LOG_WARN("WiFi", "旧 LittleFS WiFi 凭据无效,忽略迁移");
        return;
    }

    const String ssid = doc["ssid"].as<String>();
    const String password = doc["password"].as<String>();
    if (!isValidCredentialInput(ssid, password) ||
        !persistCredentials(ssid, password)) {
        LOG_WARN("WiFi", "旧 WiFi 凭据迁移到 NVS 失败");
        return;
    }

    _ssid = ssid;
    _password = password;
    _configured = true;
    LittleFS.remove(CFG_WIFI_LEGACY_CRED_PATH);
    LOG_INFO("WiFi", "旧 WiFi 凭据已迁移到 NVS: %s", _ssid.c_str());
}

bool WifiConfigService::persistCredentials(const String& ssid,
                                           const String& password) {
    if (!_credentialPrefsReady || !isValidCredentialInput(ssid, password)) {
        return false;
    }
    const size_t ssidBytes = _credentialPrefs.putString(
        CFG_WIFI_NVS_SSID_KEY, ssid);
    const size_t passwordBytes = _credentialPrefs.putString(
        CFG_WIFI_NVS_PASS_KEY, password);
    return ssidBytes > 0 && passwordBytes > 0;
}

bool WifiConfigService::isValidCredentialInput(
    const String& ssid, const String& password) const {
    if (ssid.isEmpty() || ssid.length() > CFG_WIFI_CRED_SSID_MAX_LEN) {
        return false;
    }
    return password.isEmpty() ||
           (password.length() >= 8 &&
            password.length() <= CFG_WIFI_CRED_PASS_MAX_LEN);
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
    if (!ssid || !password) return false;
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

bool WifiConfigService::configureAndConnect(const char* ssid,
                                            const char* password) {
    const String candidateSsid = ssid ? String(ssid) : String();
    const String candidatePassword = password ? String(password) : String();
    if (!isValidCredentialInput(candidateSsid, candidatePassword)) return false;

    _pendingSsid = candidateSsid;
    _pendingPassword = candidatePassword;
    _credentialChangePending = true;
    _lastError = "";
    _retryCount = 0;
    _retryExhausted = false;
    _offlineMode = false;
    return connectToWifi(_pendingSsid.c_str(), _pendingPassword.c_str());
}

void WifiConfigService::saveCredentials(const char* ssid, const char* password) {
    const String valueSsid = ssid ? String(ssid) : String();
    const String valuePassword = password ? String(password) : String();
    if (!persistCredentials(valueSsid, valuePassword)) return;
    _ssid = valueSsid;
    _password = valuePassword;
    _configured = true;
}

void WifiConfigService::clearCredentials() {
    if (_credentialPrefsReady) _credentialPrefs.clear();
    LittleFS.remove(CFG_WIFI_LEGACY_CRED_PATH);
    _ssid = "";
    _password = "";
    _pendingSsid = "";
    _pendingPassword = "";
    _credentialChangePending = false;
    _configured = false;
    _offlineMode = false;
}

void WifiConfigService::setOfflineMode(bool enabled) {
    _offlineMode = enabled;
    _connecting = false;
    _credentialChangePending = false;
    _pendingSsid = "";
    _pendingPassword = "";
    if (_credentialPrefsReady) {
        _credentialPrefs.putBool(CFG_WIFI_NVS_OFFLINE_KEY, enabled);
    }
    if (enabled) {
        stopMDNS();
        ensureAccessPoint();
        setProvisioningMode(ProvisioningMode::NONE);
        LOG_INFO("WiFi", "已启用离线模式");
    }
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
    if (_highThroughputMode) {
        setRadioProfile(WIFI_POWER_19_5dBm, false, "stream", WiFi.RSSI());
        return;
    }
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

void WifiConfigService::setHighThroughputMode(bool enabled) {
    if (_highThroughputMode == enabled) return;
    _highThroughputMode = enabled;
    _lastPowerAdjustMs = 0;
    if (!_connected) return;

    if (enabled) {
        setRadioProfile(WIFI_POWER_19_5dBm, false, "stream", WiFi.RSSI());
    } else {
        updateConnectedRadioProfile();
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
        json += "{\"ssid\":\"" + escapeJsonString(WiFi.SSID(i)) +
                "\",\"rssi\":" + String(WiFi.RSSI(i));
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
        if (!configureAndConnect(ssid.c_str(), password.c_str())) {
            server.send(400, "application/json",
                        "{\"error\":\"SSID must be 1-32 characters; password must be empty or 8-64 characters\"}");
            return;
        }
        server.send(202, "application/json",
                    "{\"status\":\"connecting\",\"apIp\":\"192.168.4.1\",\"credentialsPending\":true}");
    } else {
        server.send(400, "application/json", "{\"error\":\"missing parameters\"}");
    }
}

void WifiConfigService::handleStatusRequest(WebServer& server) {
    String json = "{\"connected\":" + String(_connected ? "true" : "false");
    json += ",\"ssid\":\"" + escapeJsonString(getSSID()) +
            "\",\"ip\":\"" + escapeJsonString(getIP()) + "\"";
    json += ",\"lanIp\":\"" + escapeJsonString(getLanIP()) + "\"";
    json += ",\"apIp\":\"" + escapeJsonString(getAPIP()) + "\"";
    json += ",\"apSsid\":\"" + String(CFG_WIFI_AP_SSID) + "\"";
    json += ",\"mdns\":\"" + getMDNSUrl() + "\"";
    json += ",\"mdnsActive\":" + String(_mdnsStarted ? "true" : "false");
    json += ",\"configured\":" + String(_configured ? "true" : "false");
    json += ",\"savedSsid\":\"" + escapeJsonString(_ssid) + "\"";
    json += ",\"lastError\":\"" + String(_lastError) + "\"";
    json += ",\"retryCount\":" + String(_retryCount);
    json += ",\"retryExhausted\":" + String(_retryExhausted ? "true" : "false");
    json += ",\"changingNetwork\":" +
            String(_credentialChangePending ? "true" : "false");
    json += ",\"credentialStorage\":\"nvs\"";
    json += ",\"phase\":\"" + String(getConnectPhaseText()) + "\"";
    json += ",\"rssi\":" + String(WiFi.RSSI()) + "}";
    server.send(200, "application/json", json);
}

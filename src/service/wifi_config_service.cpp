#include "wifi_config_service.h"
#include "../utils/logger.h"

WifiConfigService::WifiConfigService()
    : _configured(false), _connected(false), _connecting(false), _connectStartTime(0)
    , _provMode(ProvisioningMode::NONE), _provModeStartMs(0)
    , _serialMode(false)
{
}

void WifiConfigService::init() {
    loadCredentials();
    if (_configured) {
        LOG_INFO("WiFi", "已有凭据,连接: %s", _ssid.c_str());
        connectToWifi(_ssid.c_str(), _password.c_str());
    } else {
        startAPMode();
    }
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
        case ProvisioningMode::CONNECTING:  return "连接中...";
        case ProvisioningMode::CONNECTED:   return "已连接";
        default:                            return "";
    }
}

void WifiConfigService::update() {
    // 连接中
    if (_connecting) {
        if (WiFi.status() == WL_CONNECTED) {
            _connected = true;
            _connecting = false;
            setProvisioningMode(ProvisioningMode::CONNECTED);
            LOG_INFO("WiFi", "已连接: %s IP: %s", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        } else if (millis() - _connectStartTime > CFG_WIFI_CONNECT_TIMEOUT_MS) {
            LOG_WARN("WiFi", "连接超时");
            _connecting = false;
            if (_configured) {
                setProvisioningMode(ProvisioningMode::NONE);
            } else {
                startAPMode();
            }
        }
        return;
    }

    // 已连接后:3 秒展示,然后回归 NONE
    if (_provMode == ProvisioningMode::CONNECTED) {
        if (millis() - _provModeStartMs > 3000) {
            setProvisioningMode(ProvisioningMode::NONE);
        }
        return;
    }

    // 常态:已配置但掉线,定时重连
    if (_configured && !_connected && !_connecting
        && _provMode != ProvisioningMode::AP_FALLBACK) {
        static unsigned long lastReconnect = 0;
        if (millis() - lastReconnect > CFG_WIFI_RECONNECT_INTERVAL_MS) {
            lastReconnect = millis();
            connectToWifi(_ssid.c_str(), _password.c_str());
        }
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
    WiFi.mode(WIFI_AP);
    WiFi.softAP(CFG_WIFI_AP_SSID, CFG_WIFI_AP_PASSWORD, CFG_WIFI_AP_CHANNEL);
    LOG_INFO("WiFi", "AP 模式: %s IP: %s", CFG_WIFI_AP_SSID, WiFi.softAPIP().toString().c_str());
    _connected = false;
    setProvisioningMode(ProvisioningMode::AP_FALLBACK);
}

void WifiConfigService::skipProvisioning() {
    // 串口模式:跳过 UDP 状态同步,但保留 WiFi+Web 遥控
    _serialMode = true;
    if (_configured && !_connected && !_connecting) {
        LOG_INFO("WiFi", "串口模式,继续连接 WiFi: %s", _ssid.c_str());
        connectToWifi(_ssid.c_str(), _password.c_str());
    } else if (!_configured && WiFi.getMode() != WIFI_AP) {
        startAPMode();
        LOG_INFO("WiFi", "串口模式,无凭据,AP 模式 Web 可用");
    }
    setProvisioningMode(ProvisioningMode::NONE);
    LOG_INFO("WiFi", "跳过 UDP,进入串口模式");
}

bool WifiConfigService::connectToWifi(const char* ssid, const char* password) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    _connecting = true;
    _connectStartTime = millis();
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
String WifiConfigService::getIP() {
    return _connected ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
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
        connectToWifi(ssid.c_str(), password.c_str());
        server.send(200, "application/json", "{\"status\":\"connecting\"}");
    } else {
        server.send(400, "application/json", "{\"error\":\"missing parameters\"}");
    }
}

void WifiConfigService::handleStatusRequest(WebServer& server) {
    String json = "{\"connected\":" + String(_connected ? "true" : "false");
    json += ",\"ssid\":\"" + getSSID() + "\",\"ip\":\"" + getIP() + "\"";
    json += ",\"rssi\":" + String(WiFi.RSSI()) + "}";
    server.send(200, "application/json", json);
}

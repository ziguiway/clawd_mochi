#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "../config/cfg_wifi.h"

class WifiConfigService {
public:
    enum class ProvisioningMode {
        NONE,           // 已配置且连接,或不需要配网
        AP_FALLBACK,    // AP+Web 配网模式(无凭据时进入)
        CONNECTING,     // 已收到凭据,正在连接
        CONNECTED       // 刚连接成功(短暂时态,用于显示)
    };

    WifiConfigService();
    void init();
    void update();

    bool isConfigured();
    bool isConnected();
    String getIP();
    String getSSID();

    ProvisioningMode getProvisioningMode() const { return _provMode; }
    const char* getProvisioningMessage() const;

    void skipProvisioning();      // 切换到串口模式
    bool isSerialMode() const;

    void startAPMode();
    bool connectToWifi(const char* ssid, const char* password);
    void saveCredentials(const char* ssid, const char* password);
    void clearCredentials();
    void reset();

    void handleScanRequest(WebServer& server);
    void handleConnectRequest(WebServer& server);
    void handleStatusRequest(WebServer& server);

    // 全局单例绑定/访问(供各服务查询连接状态)
    static void bind(WifiConfigService* inst) { _instance = inst; }
    static WifiConfigService* current() { return _instance; }

private:
    String _ssid;
    String _password;
    bool _configured;
    bool _connected;
    bool _connecting;
    unsigned long _connectStartTime;

    ProvisioningMode _provMode;
    unsigned long _provModeStartMs;

    static WifiConfigService* _instance;

    void setProvisioningMode(ProvisioningMode mode);
    void loadCredentials();
};

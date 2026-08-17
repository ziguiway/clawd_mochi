#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "../config/cfg_wifi.h"

class WifiConfigService {
public:
    enum class ProvisioningMode {
        NONE,           // 已配置且连接,或不需要配网
        AP_FALLBACK,    // AP+Web 配网模式(无凭据时进入)
        CONNECTING,     // 已收到凭据,正在连接
        RETRY_WAIT,     // 连接超时,等待自动重试(屏幕显示倒计时)
        CONNECTED       // 刚连接成功(短暂时态,用于显示)
    };

    WifiConfigService();
    void init();
    void update();

    bool isConfigured();
    bool isOfflineMode() const { return _offlineMode; }
    bool isConnected();
    String getIP();
    String getLanIP();
    String getAPIP();
    String getMDNSUrl() const;
    String getSSID();
    String getSavedSSID() const { return _ssid; }

    // 投屏等持续低延迟传输期间关闭 WiFi 省电并提高发射功率。
    void setHighThroughputMode(bool enabled);

    ProvisioningMode getProvisioningMode() const { return _provMode; }
    const char* getProvisioningMessage() const;
    unsigned long getRetryRemainingMs() const;  // RETRY_WAIT 时距下次自动重连的毫秒数

    // 连接过程细节(供屏幕显示)
    const char* getConnectPhaseText() const;  // 连接阶段: "Connecting" / "Obtaining IP"
    const char* getLastError() const { return _lastError; }  // 最近一次失败原因
    uint8_t getRetryCount() const { return _retryCount; }    // 已连续失败次数
    bool isRetryExhausted() const { return _retryExhausted; } // 已打满最大重试次数

    void skipProvisioning();      // 切换到串口模式
    bool isSerialMode() const;

    void startAPMode();
    bool connectToWifi(const char* ssid, const char* password);
    bool configureAndConnect(const char* ssid, const char* password);
    void saveCredentials(const char* ssid, const char* password);
    void clearCredentials();
    void reset();
    void setOfflineMode(bool enabled);

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
    bool _apStarted;
    bool _dnsStarted;
    bool _mdnsStarted;
    DNSServer _dnsServer;
    unsigned long _connectStartTime;
    unsigned long _lastAttemptEndMs; // 上次连接失败/掉线的时间戳,用于渐进重试
    unsigned long _connectedSinceMs;
    unsigned long _lastPowerAdjustMs;
    int16_t _filteredRssi;
    wifi_power_t _currentTxPower;
    bool _wifiSleepEnabled;
    bool _radioProfileInitialized;
    bool _highThroughputMode;
    Preferences _credentialPrefs;
    bool _credentialPrefsReady;
    bool _offlineMode;
    String _pendingSsid;
    String _pendingPassword;
    bool _credentialChangePending;

    // 连接阶段(由 WiFi 事件推进): 关联+认证 → 获取 IP
    enum class ConnectPhase : uint8_t { ASSOCIATING, OBTAINING_IP };
    ConnectPhase _connectPhase;
    uint8_t _lastDisconnectReason;  // 最近一次 STA_DISCONNECTED 事件的原因码
    const char* _lastError;         // 映射后的失败提示文案
    uint8_t _retryCount;            // 连续失败次数
    bool _retryExhausted;           // 连续失败达到 CFG_WIFI_MAX_RETRIES

    ProvisioningMode _provMode;
    unsigned long _provModeStartMs;

    static WifiConfigService* _instance;

    static void onWifiEvent(arduino_event_id_t event, arduino_event_info_t info);
    void setProvisioningMode(ProvisioningMode mode);
    void loadCredentials();
    void migrateLegacyCredentials();
    bool persistCredentials(const String& ssid, const String& password);
    bool isValidCredentialInput(const String& ssid, const String& password) const;
    void ensureAccessPoint();
    void startCaptiveDns();
    void stopCaptiveDns();
    void startMDNS();
    void stopMDNS();
    unsigned long getReconnectDelayMs() const;
    void applyConnectingRadioProfile();
    void updateConnectedRadioProfile();
    void setRadioProfile(wifi_power_t txPower, bool sleepEnabled,
                         const char* profileName, int16_t rssi);
};

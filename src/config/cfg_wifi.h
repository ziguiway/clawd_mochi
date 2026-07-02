#pragma once

// ============================================================
// WiFi 配置
// ============================================================

// AP 模式配置
#define CFG_WIFI_AP_SSID                  "ClaWD-Mochi"
#define CFG_WIFI_AP_PASSWORD              "clawd1234"
#define CFG_WIFI_AP_CHANNEL               1

// STA 连接超时 (ms)
#define CFG_WIFI_CONNECT_TIMEOUT_MS       15000

// WiFi 扫描超时 (ms)
#define CFG_WIFI_SCAN_TIMEOUT_MS          10000

// 凭据存储路径
#define CFG_WIFI_CRED_PATH                "/wifi.json"

// 凭据最大长度
#define CFG_WIFI_CRED_SSID_MAX_LEN        32
#define CFG_WIFI_CRED_PASS_MAX_LEN        64

// 重连间隔 (ms)
#define CFG_WIFI_RECONNECT_INTERVAL_MS    30000

// Web 服务器端口
#define CFG_WIFI_WEB_PORT                 80

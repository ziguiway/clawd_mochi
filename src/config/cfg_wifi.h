#pragma once

// ============================================================
// WiFi 配置
// ============================================================

// AP 模式配置
#define CFG_WIFI_AP_SSID                  "ClaWD-Mochi"
#define CFG_WIFI_AP_PASSWORD              "clawd1234"
#define CFG_WIFI_AP_CHANNEL               1

#define CFG_WIFI_MDNS_HOSTNAME            "clawd-mochi"

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

// 最大连续重试次数,超过后回到 AP 配网模式等待用户重新配置
#define CFG_WIFI_MAX_RETRIES              5

// 重试打满后的慢速重试间隔 (ms),路由器恢复后可自愈
#define CFG_WIFI_SLOW_RETRY_INTERVAL_MS   300000

// Web 服务器端口
#define CFG_WIFI_WEB_PORT                 80

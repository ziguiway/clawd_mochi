#pragma once

// ============================================================
// WiFi 配置
// ============================================================

// AP 模式配置
#define CFG_WIFI_AP_SSID                  "ClaWD-Mochi"
#define CFG_WIFI_AP_PASSWORD              "clawd1234"
#define CFG_WIFI_AP_CHANNEL               1

#define CFG_WIFI_MDNS_HOSTNAME            "clawd-mochi"

// STA 连接超时 (ms):弱信号下给扫描、认证和 DHCP 留足时间
#define CFG_WIFI_CONNECT_TIMEOUT_MS       45000

// WiFi 扫描超时 (ms)
#define CFG_WIFI_SCAN_TIMEOUT_MS          10000

// WiFi 凭据存入独立 NVS 命名空间，uploadfs 重写 LittleFS 时不会丢失。
#define CFG_WIFI_NVS_NAMESPACE            "clawd-wifi"
#define CFG_WIFI_NVS_SSID_KEY             "ssid"
#define CFG_WIFI_NVS_PASS_KEY             "password"

// 仅用于从旧固件迁移，迁移成功后删除。
#define CFG_WIFI_LEGACY_CRED_PATH          "/wifi.json"

// 凭据最大长度
#define CFG_WIFI_CRED_SSID_MAX_LEN        32
#define CFG_WIFI_CRED_PASS_MAX_LEN        64

// 渐进重连间隔 (ms):连续失败后依次等待 5/10/20/30/60 秒
#define CFG_WIFI_RETRY_DELAY_1_MS          5000
#define CFG_WIFI_RETRY_DELAY_2_MS         10000
#define CFG_WIFI_RETRY_DELAY_3_MS         20000
#define CFG_WIFI_RETRY_DELAY_4_MS         30000
#define CFG_WIFI_RETRY_DELAY_5_MS         60000

// 首次尝试后最多再重试 5 次,超过后回到 AP 配网模式
#define CFG_WIFI_MAX_RETRIES              6

// 重试打满后的慢速重试间隔 (ms),路由器恢复后可自愈
#define CFG_WIFI_SLOW_RETRY_INTERVAL_MS   300000

// 连接稳定后每 30 秒按 RSSI 调整发射功率
#define CFG_WIFI_POWER_SETTLE_MS          10000
#define CFG_WIFI_POWER_ADJUST_INTERVAL_MS 30000

// 自适应功率 RSSI 分档 (dBm)
#define CFG_WIFI_RSSI_EXCELLENT_DBM       -55
#define CFG_WIFI_RSSI_GOOD_DBM            -67
#define CFG_WIFI_RSSI_FAIR_DBM            -75

// Web 服务器端口
#define CFG_WIFI_WEB_PORT                 80

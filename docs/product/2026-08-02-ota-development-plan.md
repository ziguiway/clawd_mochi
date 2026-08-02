# Clawd Mochi OTA 开发计划

日期：2026-08-02  
状态：真机验收通过  
分支：`codex/ota-support`

## 目标

增加固件和 LittleFS 的在线远程 OTA、局域网/恢复 AP 离线上传、每日固定时间检查、控制页面升级状态，以及 ESP32 双分区启动确认和失败回滚。

首版使用 Arduino-ESP32 内置 `Update.h` 与 `HTTPUpdate`，远程发布采用静态 `manifest.json` + GitHub Release/CDN。发现新版本后只提示，用户确认后安装。首版仅限制在局域网/AP 可达范围内，不提供公网端口映射或账号认证。

## 分区迁移

现有 `huge_app.csv` 只有 `app0`，不能进行可回滚 OTA。`ota_4mb.csv` 保持 LittleFS 起始地址不变，并提供两个 1.5MB 应用槽：

```text
nvs      0x9000    0x5000
otadata  0xe000    0x2000
app0     0x10000   0x180000
app1     0x190000  0x180000
spiffs   0x310000  0x0e0000
coredump 0x3f0000  0x10000
```

第一次支持 OTA 的版本需要 USB 刷入新的分区表和固件，不擦除整片 Flash，以保留 LittleFS 数据。后续构建必须拒绝超过单槽位的固件。

## 服务和协议

新增 `OtaService`，负责版本比较、Manifest 下载、检查调度、远程下载、本地上传状态、SHA-256 校验、NVS 最近检查记录、安装协调和启动确认。

Manifest 包含 `schema`、`product`、`board`、`channel`、`version`、`publishedAt`、固件 URL/大小/SHA-256、可选 LittleFS URL/大小/SHA-256 和 `releaseNotes`。设备只请求固定 Manifest 地址，不调用 GitHub API；固件下载使用 HTTPS。

发布构建必须在 `cfg_ota.h` 配置 `CFG_OTA_ROOT_CA`；空值仅用于本地开发，会退回不验证证书的 HTTPS 连接，不能用于正式发布。

设备提供：

```text
GET  /ota/status
POST /ota/check
POST /ota/install
POST /ota/upload
POST /ota/cancel
```

本地上传按分块写入 `Update.h`。固件写入备用 OTA 槽；LittleFS 更新前卸载文件系统，完成后重新挂载。LittleFS 没有冗余分区，失败时保留当前固件并报告明确错误；页面提示可能影响用户媒体文件。

## 检查调度

- WiFi 首次连接并完成 NTP 同步后立即检查一次。
- 默认每天本地时间 03:30 检查一次，同一自然日只执行一次。
- 控制页面支持手动 `Check now`。
- 检查使用 `MemoryMonitor` 和 `NetworkRequestGate`，TLS 堆不足、游戏/媒体运行或 Claude 工作时延后。
- 默认通道为 `stable`，默认不自动安装。

## Web UI

设置区域新增 Firmware 面板，显示当前/最新版本、通道、最近检查时间、状态、发布说明和文件大小，提供 `Check now`、`Install update`、`Upload firmware`、`Upload filesystem` 和取消操作。安装需二次确认，显示进度，重启后自动恢复状态。

## 回滚和生命周期

新应用启动后保持待确认状态；WiFi、显示、Web 和状态机自检完成后标记 valid。启动崩溃或看门狗复位由 ESP32 回滚到旧槽位。安装前停止游戏、媒体和非必要 HTTPS 请求，并记录内存快照；安装后恢复正常调度。

## 验收

执行 `pio run`、`pio run --target buildfs` 和现有 Web 回归。新增覆盖 Manifest 无更新/有更新、手动检查、固件和 LittleFS 上传、超大/错误/中断上传、重启状态恢复、每日去重、断网重试和回滚路径的测试。实机验证 USB 分区迁移、恢复 AP 离线升级、HTTPS 远程升级、LittleFS 更新及升级失败恢复。

## 后续增强

首版之后再增加 BOOT 按键授权、一次性 OTA token、Web 密码和固件签名（Ed25519/RSA）。

## 2026-08-02 真机验收

测试设备：ESP32-C3，USB 序列号 `14:63:93:6E:9F:B4`。测试入口为 `scripts/ota_test.py`，case 实现在独立的 `scripts/ota_test/` 包内。

测试运行器采用幂等设计：每次在系统临时目录复制最小 PlatformIO 项目，测试版本、Manifest 地址和启动失败注入只修改临时副本；不向受版本控制的 `platformio.ini` 或生产配置头写入测试环境和主机地址。无论 case 成功或失败，`finally` 路径都会重新烧录生产固件和生产 LittleFS，并删除全部临时项目和构建产物。验收结束时设备必须重新报告生产版本，临时目录必须不存在。

单次端到端运行已通过：

- `1.0.0-test` 通过远程 Manifest 升级到 `1.0.1-test`；
- 固件与 LittleFS 均完成 SHA-256 校验，升级后控制页面可从 LittleFS 正常加载；
- 错误 SHA-256 被拒绝，运行分区未切换；
- 浏览器上传中途断开后设备保持运行；
- `1.0.2-test` 启动失败注入触发 bootloader 回滚到 `1.0.1-test`；
- 离线浏览器上传成功并完成重启；
- 测试完成后恢复正式固件 `1.0.0-rc1` 和正式 LittleFS；
- 临时 OTA 项目和构建产物已自动清除，仓库未残留测试版本、测试 Manifest URL 或失败注入宏。

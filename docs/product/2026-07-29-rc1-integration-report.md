# RC1 集成记录

日期：2026-07-29  
版本：`1.0.0-rc1`

## 已完成

- `pio run` 通过：RAM 45,356 / 327,680 bytes（13.8%），Flash 1,167,990 / 1,310,720 bytes（89.1%）。
- 主机侧 Web 正向回归通过：Mock 目录与真实 CoinLore 目录各一轮。
- RC 产物通过 `scripts/build_rc_artifacts.sh` 生成；固件、LittleFS 及 `SHA256SUMS` 位于未纳入版本库的 `dist/rc1/`。
- 当前接入样机（USB 序列号 `14:63:93:6E:9F:B4`）已刷入同一 RC1 固件和 LittleFS。
- 样机状态接口与串口 `status` 均报告 `1.0.0-rc1`。
- 实机 Web 回归以 3 秒最小请求间隔运行；HTTP 持久日志和 USB 串口日志均断言到表情、AUTO、Profile、主题与配置导入动作。

## 尚未完成

- RC1 出口要求三台样机使用相同产物。当前只接入一台，另两台的刷写、表达式、Web、WiFi、重启、恢复默认检查待设备到位后执行。
- 因此 RC 状态为 `VERIFY`，不是 `DONE`；没有对稳定性或性能作出结论。

## 复现命令

```bash
bash scripts/build_rc_artifacts.sh
uv run scripts/test_web_ui.py
uv run scripts/test_web_ui.py --live-directory
uv run scripts/test_web_ui.py --device-url http://<device-ip>/ --request-interval 3
uv run scripts/test_web_ui.py --serial-log-only --serial-port <serial-port>
```

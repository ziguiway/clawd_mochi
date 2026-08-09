# 阶段 3：个性化配置执行报告

> 日期：2026-07-29
> 状态：`VERIFY`
> 分支：`codex/desk-companion-v1`
> 对应任务：P-01 至 P-09

## 1. 已实现

- `PreferenceService` 新增设备昵称、两行开机短句、默认表情和启动表情模式。
- 使用 `devname`、`boot1`、`boot2`、`defexpr`、`exprmode` 五个独立 NVS key。
- 启动读取时校验长度、可打印 ASCII、表情枚举和模式，损坏值回退到品牌默认值。
- `BootAnimation` 由调用方传入已校验的两行文本，并按字符宽度居中。
- 新增 `GET /profile`、`POST /profile` 和 `POST /profile/reset`。
- Profile 保存后立即应用默认表情和模式；新的开机短句在下次重启显示。
- Web 增加昵称、开机短句、默认表情、启动模式、保存和恢复默认控件。
- 恢复默认只影响个性化字段，不清除 WiFi、行情或其他显示偏好。

## 2. 默认值

| 字段 | 默认值 |
|---|---|
| `deviceName` | `MOCHI` |
| `bootLine1` | `HELLO` |
| `bootLine2` | `MOCHI` |
| `defaultExpression` | `normal` |
| `expressionMode` | `manual` |

默认模式按最新 PRD 使用固定 Normal 表情；AUTO 仍由用户主动开启。

## 3. 自动化结果

固件构建：

```text
RAM:   13.8% (45,356 / 327,680 bytes)
Flash: 93.2% (1,222,246 / 1,310,720 bytes)
Result: SUCCESS
```

相对阶段 2 报告：

```text
RAM:   +64 bytes
Flash: +8,532 bytes
```

Mocked Web UI 正向回归通过，新增覆盖：

- 修改设备昵称和两行开机短句。
- 选择 Love 作为默认表情并保存。
- 保存后页面标题和当前表情同步更新。
- 恢复品牌默认 Profile。
- 原有表情、Crypto、Market、WiFi、主题和轮播流程无回归。

局域网实机回归通过：

```bash
uv run scripts/testing/test_web_ui.py \
  --device-url http://<device-ip>/ \
  --serial-port /dev/cu.usbmodem1101
```

- 通过串口命令配置 WiFi 后，从同一局域网访问设备。
- Profile 保存、恢复默认、表情、主题、轮播、Crypto 和 Market 功能路径通过。
- HTTP 持久日志与 USB 串口实时日志均记录到表情和 Profile 关键动作。
- 测试结束后自动恢复设备原有行情、轮播和个性化配置。

`git diff --check` 和 Python 语法检查通过。

## 4. 待完成验证

- 烧录后修改 Profile，重启并确认 NVS 持久化。
- 检查 0、12、16 字符边界和包含引号、反斜杠的可打印 ASCII 文本。
- 确认两行 16 字符文本在 240×240 屏幕上不溢出。
- 恢复默认后确认 WiFi 凭据和其他偏好保持不变。
- 制作一份完整“定制订单”并完成开机画面和默认表情验收。

以上完成前，阶段 3 保持 `VERIFY`。

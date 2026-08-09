# 阶段 1：离线摆件模式执行报告

> 日期：2026-07-28
> 状态：`VERIFY`
> 分支：`codex/desk-companion-v1`
> 对应任务：O-01 至 O-08

## 1. 已实现

- `PROVISIONING` 完成服务初始化后立即进入 `LAN_IDLE`，不等待联网成功。
- `LAN_IDLE` 在未联网时进入普通眼睛显示，不再反复跳回配网页面。
- `LAN_WORKING` 持续断网后回到空闲显示，后台继续重连。
- AP、Web、时间、串口和网络服务在 `LAN_IDLE` 中继续更新。
- 重新接入现有 `EyesView` 的逐帧更新，普通眼睛固定在中央并以自然频率持续眨眼。
- 眼睛动画背景色跟随当前显示背景色，不再写死为橙色。
- 眼睛本体统一为黑色，不受主题和背景配色影响。

## 2. 代码改动

| 文件 | 改动 |
|---|---|
| `src/states/provisioning_state.cpp` | 配网改为后台行为，初始化后进入空闲态 |
| `src/states/lan_idle_state.cpp` | 未联网时显示眼睛，取消断网回配网 |
| `src/states/lan_working_state.cpp` | 断网超时后回空闲态 |
| `src/service/display_service.*` | 在 EXPRESSION 模式更新 EyesView |
| `src/view/eyes_view.*` | 支持配置化背景色、固定眼位和毫秒级自然眨眼 |

## 3. 自动化结果

### 固件

阶段 1 干净构建：

```text
RAM:   13.8% (45,276 / 327,680 bytes)
Flash: 92.1% (1,206,630 / 1,310,720 bytes)
Result: SUCCESS
```

相对基线：

```text
RAM:   -16 bytes
Flash: -36 bytes
```

### Web

```text
uv run scripts/testing/test_web_ui.py
Result: PASS
```

现有 Crypto、Market、WiFi 错误提示、主题和轮播正向流程没有回归。

## 4. 实机验证

设备：

```text
/dev/cu.usbmodem1101
ESP32-C3
MAC 14:63:93:6e:9f:b4
```

固件烧录成功。通过串口执行 `reset` 清除 WiFi 凭据后，启动日志为：

```text
BOOT
MODE_SELECT
PROVISIONING
LAN_IDLE
```

结果：

- `PROVISIONING` 没有等待网络连接。
- AP 成功启动为 `ClaWD-Mochi`。
- AP 地址为 `192.168.4.1`。
- 进入 `LAN_IDLE` 后短时观察没有再次跳回 `PROVISIONING`。
- 串口状态显示 WiFi 未连接、Claude Code IDLE，设备保持运行。

## 5. 任务状态

| 任务 | 状态 | 说明 |
|---|---|---|
| O-01 | `DONE` | 无凭据、连接中、失败场景已建立 |
| O-02 | `DONE` | ProvisioningState 已修改 |
| O-03 | `DONE` | LANIdleState 已修改 |
| O-04 | `DONE` | 后台服务持续更新 |
| O-05 | `DONE` | WiFi 状态不再强制覆盖 TFT |
| O-06 | `DONE` | 构建和现有 Web 回归通过 |
| O-07 | `VERIFY` | 首次启动通过，其余实机网络场景待测 |
| O-08 | `TODO` | O-07 完成后执行最终修复和 M1 验收 |

## 6. 待完成验证

- 未配网连续运行 24 小时。
- 提交错误 WiFi 密码后保持摆件画面。
- 正确路由器不可用时保持摆件画面。
- 路由器恢复后后台自动连接。
- 运行中断网不覆盖用户当前显示。
- 在设备屏幕上确认眼睛无左右移动、始终为黑色、自然眨眼且背景擦除无残影。

这些项目完成前，阶段 1 保持 `VERIFY`，阶段 2 不开始。

## 7. 当前结论

阶段 1 的软件实现和首次启动主路径成立。当前没有发现需要回退的 P0/P1
问题，样机已烧录阶段 1 固件并清除 WiFi 凭据，可用于持续运行验证。

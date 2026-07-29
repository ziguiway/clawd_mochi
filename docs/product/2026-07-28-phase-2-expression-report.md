# 阶段 2：统一表情与可选 AUTO 执行报告

> 日期：2026-07-28
> 状态：`VERIFY`
> 分支：`codex/desk-companion-v1`
> 对应任务：E-01 至 E-13

## 1. 已实现

- 新增独立的 `ExpressionId`、`ExpressionMode`、名称和标签映射。
- `EyesView` 建立 `setExpression/update/redraw` 契约。
- 实现 Normal、Happy、Sleepy、Sleeping、Curious、Surprised、Grumpy、Love。
- 所有眼睛本体固定使用黑色，主题只影响文字和非眼睛装饰。
- Normal 固定眼位，以 2.5–6 秒随机间隔自然眨眼；Sleepy 使用更慢的眨眼。
- 手动选择表情后保持该表情，不自动恢复 AUTO。
- AUTO 默认关闭，用户主动开启后才运行。
- AUTO 以 Normal 为基础，每 12–35 秒随机插入短表情，同一事件不连续重复。
- 增加表情列表、当前状态和设置 API。
- Web 首屏增加 8 种表情和 AUTO 控制，旧 `/cmd` 入口继续兼容。

## 2. API

```http
GET /expressions
GET /expression/current
POST /expression
Content-Type: application/json
```

手动选择：

```json
{"id": "happy"}
```

或：

```json
{"mode": "manual", "id": "happy"}
```

开启 AUTO：

```json
{"mode": "auto"}
```

未知表情、无效模式、无效 JSON 和冲突参数返回 HTTP 400。

## 3. 自动化结果

固件构建：

```text
RAM:   13.8% (45,292 / 327,680 bytes)
Flash: 92.6% (1,213,714 / 1,310,720 bytes)
Result: SUCCESS
```

相对阶段 1 最终构建：

```text
RAM:   +16 bytes
Flash: +7,084 bytes
```

Mocked Web UI 正向回归通过，新增覆盖：

- 首屏完整显示 8 种表情。
- 手动选择 Love 后进入 MANUAL 并保持。
- 用户主动开启 AUTO。
- 原有 Crypto、Market、WiFi、主题和轮播流程无回归。

## 4. 设计取舍

AUTO 调度当前保留在 `DisplayService`，没有立即新增 `IdleBehaviorService`。
目前调度仅包含下一事件、返回时间和去重状态，单独服务会增加对象和接口成本。
当阶段 3 引入持久化性格、昼夜权重或可配置行为后，再提取独立服务。

旧 `VIEW_EYES_SQUISH` 映射为 Happy，旧 `VIEW_EYES_NORMAL` 映射为 Normal，
避免已有 Web 命令或启动配置失效。

## 5. 待完成验证

- 在实机逐个确认 8 种表情的辨识度、居中、边界和黑色一致性。
- 检查 Normal 和 Sleepy 的眨眼擦除是否无残影。
- AUTO 连续观察 10 分钟，确认没有明显固定循环。
- AUTO 连续运行 24 小时无卡死。
- 连续切换表情 100 次，无残影或状态错乱。
- 从时钟、画板和 Codex 状态返回后恢复正确的 MANUAL/AUTO 状态。

以上完成前，阶段 2 保持 `VERIFY`。

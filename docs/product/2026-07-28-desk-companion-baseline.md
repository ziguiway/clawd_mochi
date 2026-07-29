# Desk Companion v1 开发基线记录

> 记录日期：2026-07-28
> 分支：`codex/desk-companion-v1`
> 基线提交：`7d8ff30`
> 对应任务：B-01 至 B-07

## 1. 构建基线

执行一次 PlatformIO 干净构建：

```text
RAM:   13.8% (45,292 / 327,680 bytes)
Flash: 92.1% (1,206,666 / 1,310,720 bytes)
Result: SUCCESS
```

构建生成的 `firmware.bin` 为 1,266,576 bytes。

## 2. 分区与 LittleFS 基线

当前 4 MB Flash 分区：

| 分区 | 大小 |
|---|---:|
| NVS | 20 KB |
| OTA data | 8 KB |
| app0 | 1,280 KB |
| app1 | 1,280 KB |
| LittleFS | 1,408 KB |
| coredump | 64 KB |

`data/` 当前包含 7 个文件，有效内容共 17,869 bytes。生成的
`littlefs.bin` 会填充到完整的 1,408 KB 分区大小；按源文件内容估算，
静态资源只使用约 1.2% 的分区容量，LittleFS 空间不是 v1 的当前瓶颈。

应用程序分区已使用 92.1%，是后续表情、字体和 Web 功能扩展的主要资源风险。

## 3. 自动化测试基线

### Mocked Web UI

命令：

```bash
uv run scripts/test_web_ui.py
```

结果：通过。覆盖 Crypto 搜索与保存、WiFi 错误提示、主题、信息轮播和
Market 搜索与保存的正向流程。

### Live directory Web UI

命令：

```bash
uv run scripts/test_web_ui.py --live-directory
```

结果：通过。实时 CoinLore 资产目录下的同一组正向流程全部通过。

## 4. 设备与实机条件

检测到一台 ESP32-C3：

```text
/dev/cu.usbmodem1101
USB VID:PID=303A:1001
```

阶段 1 自动化通过后使用该设备完成烧录、串口状态和屏幕检查。24 小时运行、
错误密码、路由器断开与恢复属于实机持续验证项，不由主机侧测试替代。

## 5. 当前产品行为

| 场景 | 当前代码行为 | Desk Companion v1 目标 |
|---|---|---|
| 无 WiFi 凭据启动 | 停留在配网页面 | 直接显示动态眼睛 |
| 有凭据但连接中 | 停留在连接页面 | 显示眼睛并后台连接 |
| WiFi 失败 | 显示失败和重试页面 | 保持当前摆件画面 |
| LAN_IDLE 断网 | 返回 `PROVISIONING` | 保持空闲显示 |
| LAN_WORKING 断网 | 8 秒后返回 `PROVISIONING` | 8 秒后返回空闲显示 |
| 普通空闲眼睛 | 已有动画类，但空闲更新未调用 | 固定黑色眼睛，以自然频率眨眼 |
| AP Web | 始终启动 | 保持 |
| STA Web | 联网后可访问 | 保持 |

## 6. 已有显示和控制能力

显示视图：

- Normal eyes
- Squish eyes
- Code / terminal
- Canvas
- Thinking
- Working
- Clock
- Pomodoro
- Weather
- Crypto
- Market

持久化设置：

- 背景颜色
- 动画速度
- 启动视图
- 亮度
- Claude status 开关
- 橙白 / 橙黑主题
- 信息轮播和固定信息页
- 夜间亮度

## 7. v1 默认产品参数

| 参数 | 决定 |
|---|---|
| 默认模式 | `Normal` |
| 阶段 1 离线默认 | 固定黑色 Normal 眼睛，仅自然眨眼 |
| 默认主题 | Classic Orange |
| 默认昵称 | `MOCHI` |
| 开机第一行 | `HELLO` |
| 开机第二行 | `MOCHI` |
| 手动表情恢复 | 默认保持，不自动回 AUTO |
| 夜间自动行为 | 默认关闭 |

## 8. 阶段 0 结论

- B-01：完成。
- B-02：完成。
- B-03：完成。
- B-04：完成。
- B-05：状态流代码基线完成，烧录后的屏幕验证进入阶段 1。
- B-06：完成。
- B-07：完成。

阶段 0 软件出口满足，可以进入阶段 1。

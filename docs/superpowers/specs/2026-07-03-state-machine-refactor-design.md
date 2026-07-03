# Clawd Mochi 状态机重构设计

> 日期:2026-07-03
> 状态:设计完成,未实现

## 背景

当前架构问题:
- `main.cpp` 仍含模式判断逻辑(`canStartCC` 等)
- 服务(`DisplayService`/`ClaudeCodeService`)内部混杂状态分发逻辑
- 没有清晰的状态边界,显示/配网/CC 状态切换散落各处

目标:`src/states/` 下每个状态独立成类,`main` 只持有 `AppStateMachine` 并转发 `update()`。

## 已有基础(当前会话已创建)

```
src/states/
├── app_context.h         # IAppContext 接口(纯虚,返回各服务指针)
├── state.h               # State 基类(onEnter/onUpdate/onExit + _ctx)
├── app_state_machine.h   # AppStateMachine 壳(持有服务实例 + 状态指针数组)
├── boot_state.h          # BootState 声明(未实现)
```

## 待创建文件

### 状态类(8 个,每个 .h + .cpp)

| 文件 | 状态 | 职责 |
|------|------|------|
| `boot_state.*` | BOOT | 启动动画 + LittleFS/Logger 初始化,完成后切到 MODE_SELECT |
| `mode_select_state.*` | MODE_SELECT | 3 秒检测串口输入决定 LAN/SERIAL,显示选择界面,完成后切 PROVISIONING 或 SERIAL_IDLE |
| `provisioning_state.*` | PROVISIONING | AP 配网 + 二维码显示,连接成功切 LAN_IDLE |
| `lan_idle_state.*` | LAN_IDLE | WiFi 已连,CC 空闲,显示眼睛动画。CC 变 WORKING 切 LAN_WORKING |
| `lan_working_state.*` | LAN_WORKING | CC 工作中,显示工作动画 + 工具信息。CC 回 IDLE 切 LAN_IDLE |
| `serial_idle_state.*` | SERIAL_IDLE | 串口模式,CC 空闲,显示眼睛。CC 变 WORKING 切 SERIAL_WORKING |
| `serial_working_state.*` | SERIAL_WORKING | 串口模式,CC 工作中。CC 回 IDLE 切 SERIAL_IDLE |
| `reset_state.*` | RESET | 显示红屏 RESET,调 wifiService.reset() 重启 |

### 管理器实现

| 文件 | 职责 |
|------|------|
| `app_state_machine.cpp` | 构造所有服务 + 状态实例,实现 `IAppContext`,管理 `transitionTo()` |

## 状态转移图

```
        BOOT
         │
         ▼
    MODE_SELECT ──(串口有输入)──► SERIAL_IDLE ◄──► SERIAL_WORKING
         │
         │(无输入,LAN 模式)
         ▼
    PROVISIONING ──(连上 WiFi)──► LAN_IDLE ◄──► LAN_WORKING

    任意状态 ──(BOOT 长按 5s)──► RESET ──(重启)──► BOOT
```

## 各状态实现要点

### BootState
- `onEnter`: `LittleFS.begin(true)` + `Logger.init()` + `tft.init()` + `BootAnimation::run()`
- `onUpdate`: 立即 `requestNextState()` → 切到 MODE_SELECT
- 服务实例化时机:在 `AppStateMachine` 构造时完成,`BootState` 只调用 `init()`

### ModeSelectState
- `onEnter`: 显示 "Select Mode" 界面 + 启动 3 秒计时
- `onUpdate`:
  - 检测 `Serial.available()` > 0 → `opMode.setMode(SERIAL)` → 切 SERIAL_IDLE
  - 超时 3 秒 → `opMode.setMode(LAN)` → 切 PROVISIONING
- 显示:两行文字 "Serial: send any key" / "LAN: wait 3s"

### ProvisioningState
- `onEnter`: `wifi.init()` + `web.init()` + `display.init()` + `time.init()`
- `onUpdate`:
  - 调 `wifi.update()` / `web.update()` / `time.update()` / `cc.update()` / `serial.update()`
  - `wifi.isConnected()` → 切 LAN_IDLE
  - `opMode.isSerial()` (用户点跳过) → 切 SERIAL_IDLE
- 显示:二维码 + AP 凭据(现有 `updateProvisioning` 逻辑搬来)

### LANIdleState
- `onEnter`: `display.switchToExpressionMode()` (眼睛动画)
- `onUpdate`:
  - 调 `wifi.update()` / `web.update()` / `time.update()` / `cc.update()` / `serial.update()` / `display.update()`
  - `cc.getStatus() == THINKING || WORKING` → 切 LAN_WORKING
  - `!wifi.isConnected()` → 切 PROVISIONING

### LANWorkingState
- `onEnter`: 显示工作动画
- `onUpdate`:
  - 同 LANIdleState 的服务更新
  - `cc.getStatus() == IDLE || SLEEPING` → 切 LAN_IDLE
  - 显示 CC 信息视图(hook/tool/model)

### SerialIdleState
- `onEnter`: `display.switchToExpressionMode()`,不启动 WiFi/Web
- `onUpdate`:
  - 只调 `cc.update()` / `serial.update()` / `display.update()`
  - `cc.getStatus() == THINKING || WORKING` → 切 SERIAL_WORKING

### SerialWorkingState
- `onEnter`: 显示工作动画
- `onUpdate`:
  - 同 SerialIdleState
  - `cc.getStatus() == IDLE` → 切 SERIAL_IDLE

### ResetState
- `onEnter`: `tft.clear(RED)` + 显示 "RESET" + `delay(1000)` + `wifi.reset()`
- `onUpdate`: 无(设备会重启)

## AppStateMachine 实现

### 构造顺序(重要:依赖顺序)
```
_tft()                          // 无依赖
_time()                         // 无依赖
_sm()                           // 无依赖
_cc(&_sm)                       // 依赖 _sm
_wifi()                         // 无依赖
_display(&_tft, &_cc, &_wifi)   // 依赖 _tft, _cc, _wifi
_web(&_cc, &_wifi, &_time, &_display)  // 依赖 _cc, _wifi, _time, _display
_serial(&_wifi, &_cc, &_time)   // 依赖 _wifi, _cc, _time
_opMode()                       // 无依赖
_bootButton(&_tft, &_wifi)      // 依赖 _tft, _wifi
```

### init()
```cpp
void AppStateMachine::init() {
    OperationModeService::bind(&_opMode);
    WifiConfigService::bind(&_wifi);

    registerState(BOOT, &_boot);
    registerState(MODE_SELECT, &_modeSelect);
    registerState(PROVISIONING, &_provisioning);
    registerState(LAN_IDLE, &_lanIdle);
    registerState(LAN_WORKING, &_lanWorking);
    registerState(SERIAL_IDLE, &_serialIdle);
    registerState(SERIAL_WORKING, &_serialWorking);
    registerState(RESET, &_reset);

    transitionTo(BOOT);
}
```

### update()
```cpp
void AppStateMachine::update() {
    if (_current) _current->onUpdate();
    bootButton->update();  // 全局,任何状态都能触发 RESET
}
```

### transitionTo()
```cpp
void AppStateMachine::transitionTo(StateId id) {
    if (id == _currentId) return;
    if (_current) _current->onExit();
    _currentId = id;
    _current = _states[id];
    if (_current) {
        _current->_ctx = this;
        _current->onEnter();
    }
}
```

### IAppContext 实现
所有 `tft()`/`time()`/`cc()` 等直接返回 `&_tft` 等成员指针。

## main.cpp 改写

```cpp
#include "states/app_state_machine.h"

AppStateMachine appState;

void setup() {
    Serial.begin(115200);
    appState.init();
}

void loop() {
    appState.update();
    delay(APP_LOOP_INTERVAL_MS);
}
```

## 从现有代码迁移逻辑

| 现有位置 | 迁移到 | 说明 |
|---------|-------|------|
| `DisplayService::updateProvisioning()` | `ProvisioningState::onEnter/onUpdate` | 二维码 + AP 凭据显示 |
| `DisplayService::updateDisplayMode()` | 各状态的 `onUpdate` | 模式判断逻辑由状态转移体现 |
| `DisplayService::switchToExpressionMode()` | `*IdleState::onEnter` | 眼睛动画 |
| `DisplayService::switchToInfoMode()` | `*WorkingState::onEnter` | CC 信息视图 |
| `main.cpp` 的 `canStartCC` 逻辑 | `AppStateMachine::update()` 或各状态 | 由当前状态决定调哪些服务 |
| `main.cpp` 的 BOOT 按钮逻辑 | `BootButtonService`(已迁移) | 全局,不受状态影响 |

## 编译验证清单

- [ ] `app_state_machine.cpp` 实现 IAppContext 所有方法
- [ ] 8 个状态 `.cpp` 实现 onEnter/onUpdate
- [ ] 状态转移逻辑正确(无死循环)
- [ ] 服务 update() 调用不在多个状态重复(每个状态只调自己需要的)
- [ ] `main.cpp` 只调 `appState.init()` / `update()`
- [ ] 删除 `display_service.cpp` 里已迁移的逻辑(避免双重显示)
- [ ] 删除 `main.cpp` 里旧的全局实例(已移到 AppStateMachine)

## 风险

1. **服务重复调用**:多个状态都调 `cc.update()` / `display.update()`,需确保每次只在一个状态调
2. **显示冲突**:`DisplayService` 内部还有模式判断,需清理掉,只保留绘制函数
3. **构造顺序**:`AppStateMachine` 成员初始化顺序必须正确(见上)
4. **循环依赖**:已通过 `IAppContext` 接口解决

## 下次会话执行步骤

1. 先编译当前 25 个文件改动,确认基线能跑(状态机壳子不影响现有功能)
2. 按"待创建文件"顺序实现 8 个状态
3. 实现 `app_state_machine.cpp`
4. 改写 `main.cpp`
5. 清理 `display_service.cpp` / `claude_code_service.cpp` 里已迁移的逻辑
6. 编译 + 烧录测试
7. 串口 mock 命令测试:`cc thinking,PreToolUse,Read,,claude-sonnet-4-6` 等

# 首次上电与配网流程修复实施计划

> **致执行代理:** 必备子技能:使用 superpowers:subagent-driven-development(推荐)或 superpowers:executing-plans 按任务逐条执行本计划。步骤使用复选框(`- [ ]`)语法跟踪进度。

**目标:** 让首次上电与 WiFi 配网流程形成闭环:设备真正显示引导/二维码界面;"已连接 + IP" 确认屏不被抢占;手机通过 captive portal 直达配网页;网页在密码错误恢复期间正确提示。

**架构:** 显示仲裁上移到状态层:当配网活跃时,`LanIdleState`/`LanWorkingState` 让位于 `WifiConfigService::ProvisioningMode`(调用 `DisplayService::updateProvisioning()`);在短暂的 CONNECTED 窗口内,`DisplayService` 的 idle/INFO 切换主动退让。Web 侧改动仅限 `WebService::onNotFound`(captive 重定向)与 `wifi.js`(失败状态轮询)。不新增服务;无 RAM 影响(渲染全部复用 `updateProvisioning()` 已有的 static 缓存)。

**技术栈:** PlatformIO(Arduino/ESP32-C3),固件 C++ 位于 `src/`,Web 资源为 `data/` 下的原生 JS。

**本仓库必须遵守的约定:**
- 注释与日志用中文,UI 文案用英文。
- 防闪烁:`updateProvisioning()` 已采用"模式切换整屏重绘 + 动态元素脏检查"模式,新增内容沿用该模式。
- 每个任务完成后 `pio run` 必须通过;涉及内存的改动需报告改动前后静态 RAM。

**设备验证的现实约束:** 这些是状态机/显示时序改动,没有宿主机单元测试框架。每个任务的验证 = `pio run` 编译 + 串口/UDP 驱动的实体机检查(可用现成的 `scripts/tools/preview_expressions_udp.py` 注入 CC 状态)。最后一个任务是完整的真机走查清单。禁止仅凭编译通过就宣布完成。

---

### 任务 1:把配网引导屏接入状态机(修复死代码断点)

**文件:**
- 修改: `src/states/lan_idle_state.cpp`
- 修改: `src/states/lan_working_state.cpp`
- 修改: `src/service/display_service.h`(新增一个公开查询)

**背景:** `DisplayService::updateProvisioning()`(display_service.cpp:2887)已实现全部配网界面,但没有任何调用方。`LanIdleState::onUpdate` 当前完全不查询 `_ctx->wifi()->getProvisioningMode()` 就驱动显示/CC 切换。

- [ ] **步骤 1:新增显示查询,让状态机能判断配网屏是否在显示**

在 `src/service/display_service.h` 中,`isInteractive()` 附近(约第 74 行)添加:

```cpp
    // 配网引导屏正在显示(状态机据此让出屏幕仲裁权)
    bool isProvisioningActive() const { return _currentMode == DisplayMode::PROVISIONING; }
```

- [ ] **步骤 2:在 `LanIdleState` 中驱动配网屏**

在 `src/states/lan_idle_state.cpp` 中:

`onEnter()` —— 仅当配网不需要占用屏幕时才切换 idle 视图:

```cpp
void LANIdleState::onEnter() {
    // 配网引导(AP/连接中/重试/成功确认)优先占用屏幕
    if (_ctx->wifi()->getProvisioningMode() != WifiConfigService::ProvisioningMode::NONE) {
        return;
    }
    if (_ctx->wifi()->isConnected()) {
        _ctx->display()->switchToIdleDisplay();
    } else {
        _ctx->display()->switchToExpressionMode();
    }
}
```

`onUpdate()` —— 在各服务 `->update()` 调用之后、独占视图提前返回之前,添加:

```cpp
    // 配网流程活跃时,屏幕交给配网引导屏,并抑制一切视图切换
    if (_ctx->wifi()->getProvisioningMode() != WifiConfigService::ProvisioningMode::NONE) {
        _ctx->display()->updateProvisioning();
        return;
    }
```

- [ ] **步骤 3:`LanWorkingState` 同样让位**

在 `src/states/lan_working_state.cpp` 的 `onUpdate()` 中,紧跟各服务 `->update()` 调用之后(独占视图检查之前),添加同样的代码块:

```cpp
    // 配网引导屏优先(例如 WiFi 重试打满回 AP,或换网验证中)
    if (_ctx->wifi()->getProvisioningMode() != WifiConfigService::ProvisioningMode::NONE) {
        _ctx->display()->updateProvisioning();
        return;
    }
```

说明:`LanWorkingState::onEnter()` 仍会调用 `switchToInfoMode()`,这没问题——下一个 `onUpdate()` tick 会在 provMode 出现时立即切回配网屏。INFO 屏唯一可能停留的窗口是配网期间 CC 突然激活;上面的代码块会在一个 tick 内收回屏幕,且 `transitionTo` 对相同 id 提前返回,不会反复重进 `switchToInfoMode`。

- [ ] **步骤 4:编译**

运行: `pio run`
预期: `SUCCESS`。报告改动前后静态 RAM(预期不变;无新增分配)。

- [ ] **步骤 5:提交**

```bash
git add src/states/lan_idle_state.cpp src/states/lan_working_state.cpp src/service/display_service.h
git commit -m "fix(provisioning): 由 idle/working 状态驱动配网引导屏"
```

---

### 任务 2:保护 CONNECTED 成功确认屏不被 idle 视图抢占

**文件:**
- 修改: `src/service/display_service.cpp`(`switchToExpressionMode`、`switchToIdleDisplay`)

**背景:** 任务 1 之后状态机不再抢屏,但 `switchToIdleDisplay()` 还会被 `DisplayService::update()` 内部的 CC 状态转换路径(约 1440 行,以及 `switchToExpressionMode` → `applyIdleDefaultView`,3085/3091 行)调用,它把 `_currentMode` 置为 INTERACTIVE/EXPRESSION,会踩掉 3 秒的 CONNECTED 确认屏。在 DisplayService 内部加一个统一守卫。

- [ ] **步骤 1:新增私有辅助函数与守卫**

在 `src/service/display_service.h` 私有区(`_carouselSuspended` 附近,约 293 行)声明:

```cpp
    // 配网成功确认屏(CONNECTED)展示期间,拒绝被 idle/INFO 视图覆盖
    bool provisioningScreenProtected() const;
```

在 `src/service/display_service.cpp` 中,`updateProvisioning()` 之后(约 3080 行)实现:

```cpp
bool DisplayService::provisioningScreenProtected() const {
    // 仅保护短暂的成功确认窗;AP/CONNECTING/RETRY_WAIT 由状态机驱动,不在此拦截
    return _wifiService &&
           _wifiService->getProvisioningMode() ==
               WifiConfigService::ProvisioningMode::CONNECTED;
}
```

- [ ] **步骤 2:守卫两个会踩屏的入口**

在 `switchToExpressionMode()`(display_service.cpp:3082)的第一行添加:

```cpp
    if (provisioningScreenProtected()) return;
```

在 `switchToIdleDisplay()`(display_service.cpp,约 2735 行)的第一行添加:

```cpp
    if (provisioningScreenProtected()) return;
```

(故意不守卫 `switchToInfoMode`:正在工作的 Claude 会话优先级高于 3 秒确认屏,且 WORKING 结束后任务 1 的状态代码块会在下一 tick 重新拉起 CONNECTED 屏。该取舍已写在头文件注释中。)

- [ ] **步骤 3:编译**

运行: `pio run`
预期: `SUCCESS`。

- [ ] **步骤 4:提交**

```bash
git add src/service/display_service.cpp src/service/display_service.h
git commit -m "fix(provisioning): 保护 CONNECTED 确认屏不被 idle 视图抢占"
```

---

### 任务 3:captive portal 重定向,让手机直达配网页

**文件:**
- 修改: `src/service/web_service.cpp`(`onNotFound` 处理器,约 192-199 行)

**背景:** 目前没有 DNS 服务器(可以接受——引入 DNSServer 要多花一个 task 和 RAM,而 iOS/Android 的 captive 检测走的是 HTTP 探测,我们直接应答即可)。最小且可靠的修法:当请求来自 AP 接口时(即 STA 尚未连接,或请求落在 AP 网段),对未知路径应答 302 到 `/wifi_setup`,触发手机的 captive portal 弹窗;来自 LAN 真实 IP 的请求保持 404,不干扰控制器的正常路由。

- [ ] **步骤 1:把 AP 侧的未知请求重定向到配网页**

将 `src/service/web_service.cpp` 中 `onNotFound` lambda 替换为:

```cpp
    _server.onNotFound([this]() {
        String path = _server.uri();
        if (LittleFS.exists(path)) {
            handleFile(path.c_str(), getContentType(path).c_str());
            return;
        }
        // captive portal:手机连上 ClaWD-Mochi AP 后访问任意网址都引导到配网页。
        // 仅在未连上路由器(或请求来自 AP 网段)时重定向,避免干扰 LAN 侧正常访问。
        const bool apSide = !_wifiService->isConnected()
            || _server.client().localIP() == WiFi.softAPIP();
        if (apSide) {
            _server.sendHeader("Location", "http://192.168.4.1/wifi_setup");
            _server.send(302, "text/plain", "");
            return;
        }
        _server.send(404, "text/plain", "Not Found");
    });
```

`WiFi.softAPIP()` 需要 `<WiFi.h>`——确认 `web_service.cpp` 顶部已包含(经 `wifi_config_service.h` 传递包含;若编译报错则显式添加 `#include <WiFi.h>`)。

- [ ] **步骤 2:编译**

运行: `pio run`
预期: `SUCCESS`。

- [ ] **步骤 3:提交**

```bash
git add src/service/web_service.cpp
git commit -m "feat(provisioning): AP 接口上的 captive portal 302 重定向到 wifi_setup"
```

---

### 任务 4:修复 `wifi.js` 在密码错误恢复期间的过早失败提示

**文件:**
- 修改: `data/wifi.js`(`connectWifi` 轮询循环)

**背景:** 设备已保存凭据、用户提交错误密码时,固件会进入 RETRY_WAIT(此时 `_lastError` 已设置),随后重连旧网络并恢复。当前 JS 把任何 `lastError` 视为最终失败并停止轮询,设备实际正在恢复时网页却显示失败。

- [ ] **步骤 1:只在设备彻底放弃或已回退时才停止轮询**

在 `data/wifi.js` 的 `connectWifi()` 轮询 `for` 循环中,把计算 `const failed = ...` 的 `if (msg) { ... }` 代码块替换为:

```js
        if (msg) {
            // 设备可能正在回退重连旧网络:只有彻底放弃(retryExhausted 或不再验证新凭据且未连接)
            // 才视为最终失败;否则继续轮询,给旧网络恢复留出时间。
            const finalFailure = status.retryExhausted
                || (status.lastError && !status.changingNetwork && !status.connected);
            msg.textContent = finalFailure
                ? `${status.lastError || 'Connection failed'}. The previous saved network was not overwritten.`
                : (status.lastError
                    ? `${status.lastError}. Restoring the previous network...`
                    : `${status.phase || 'Connecting'}...`);
            msg.className = finalFailure ? 'status-msg error' : 'status-msg info';
            if (finalFailure) return;
        }
```

同时把检测 `status.connected && status.ssid !== ssid` 的分支(当前显示 error)改为中性的恢复提示,因为走到这里意味着旧网络已恢复:

```js
        if (status.connected && status.ssid !== ssid) {
            if (msg) { msg.textContent = `Could not join ${ssid}. Reverted to ${status.ssid}.`; msg.className = 'status-msg info'; }
            return;
        }
```

- [ ] **步骤 2:跑正向 Web UI 回归(mock 版)**

运行: `uv run scripts/testing/test_web_ui.py`
预期: PASS(该用例覆盖 market/crypto 流程,但会走同一条 LittleFS 静态资源链路,能发现 JS 语法错误)。

- [ ] **步骤 3:提交**

```bash
git add data/wifi.js
git commit -m "fix(wifi-setup): 密码错误恢复期间继续轮询,不再过早报失败"
```

---

### 任务 5:配网页新增隐藏网络入口

**文件:**
- 修改: `data/wifi_setup.html`(连接面板区域)
- 修改: `data/wifi.js`(一个辅助函数)

**背景:** 连接表单只在点击扫描到的 SSID 后才出现(`selectNetwork` 设置 `display:block`),隐藏 SSID 永远无法配置。

- [ ] **步骤 1:添加"加入未列出的网络"链接**

在 `data/wifi_setup.html` 中,紧接包含 `wifi-list` 的 `<section class="card provision-card">` 结束标签 `</section>` 之后,添加:

```html
        <p class="hidden-net-link"><a href="#" onclick="showManualEntry(); return false;">Join a network that is not listed</a></p>
```

在 `data/wifi.js` 中添加:

```js
function showManualEntry() {
    const f = document.getElementById('connect-form');
    if (f) f.style.display = 'block';
    document.getElementById('ssid-input')?.focus();
}
```

- [ ] **步骤 2:编译 + 注意需要 uploadfs 才能下发到设备**

运行: `pio run`(仅编译检查;资源在真机验证时通过 `pio run --target uploadfs` 下发)。
预期: `SUCCESS`。

- [ ] **步骤 3:提交**

```bash
git add data/wifi_setup.html data/wifi.js
git commit -m "feat(wifi-setup): 支持加入隐藏/未列出的网络"
```

---

### 任务 6:真机端到端走查(强制,不允许仅凭编译通过验收)

**文件:**
- 无(纯验证);用 `scripts/tools/preview_expressions_udp.py` 注入 CC 状态。

- [ ] **步骤 1:全新设备路径**

清除凭据(长按 BOOT 键 5 秒 = 恢复出厂,或串口 `reset`),烧录固件 + LittleFS(`pio run --target upload && pio run --target uploadfs`),上电,在**实体 ST7789** 上验证:
1. 开机动画 → 出现带二维码的 "WiFi Setup" 屏(而不是眼睛)。
2. 屏幕显示 "Scan QR or open: http://192.168.4.1"。
3. 手机加入 `ClaWD-Mochi`(密码 `clawd1234`);captive 弹窗或访问任意网址都落到 `/wifi_setup`。
4. 选择网络、输入密码 → 设备显示 "Joining network / <SSID> / Connecting..." 三点动画,无闪烁。
5. 成功 → "Connected ✓" 屏带 LAN IP 停留约 3 秒(不会被眼睛瞬间顶替)。
6. 随后进入 idle 默认视图;控制器可通过 LAN IP 访问。

- [ ] **步骤 2:已有凭据时输错新密码路径**

在已保存可用网络的情况下,通过控制器的 WiFi Setup 对另一个 SSID 提交错误密码:
1. 网页显示 "Restoring the previous network..."(而不是立即失败)。
2. 设备显示 RETRY_WAIT 倒计时,随后重连旧网络;网页最终以 "Reverted to <old SSID>" 结束。
3. 重启后旧凭据仍然可用。

- [ ] **步骤 3:配网期间收到 CC 状态路径**

在 AP 兜底屏显示期间,注入 WORKING 状态:`uv run scripts/tools/preview_expressions_udp.py`(或单个 UDP 包)。验证配网屏不被拆除(或在一帧内恢复),INFO 面板无闪烁。

- [ ] **步骤 4:重试打满路径**

在**没有**已保存凭据的情况下,连续提交错误密码 6 次以上(或临时在一次性构建中调低 `CFG_WIFI_MAX_RETRIES`)→ 屏幕回到 AP 兜底页并显示 "Too many attempts, set up again"。

- [ ] **步骤 5:记录结果**

把每个场景的串口日志摘录贴进最终空提交的 commit message 或 PR 描述;禁止仅凭编译输出就把本任务标记为完成。

---

## 自查记录

- **需求覆盖:** P1 配网屏死代码 → 任务 1;P2 CONNECTED 被抢占 → 任务 2;P3 captive portal/AP 提示 → 任务 3(采用重定向方案;SSID/密码是固定值且已显示在网页上——受 240px 屏幕限制不额外上屏,二维码 + captive 弹窗已能完成引导);P4 wifi.js 过早失败 → 任务 4;P5 隐藏网络 → 任务 5;端到端闭环 → 任务 6。
- **占位符扫描:** 无 TBD/TODO 步骤;所有代码完整给出。
- **类型一致性:** `provisioningScreenProtected()`(任务 2)在两个被守卫函数中一致使用;`isProvisioningActive()`(任务 1)作为廉价公开查询保留,即便任务 1 后未直接使用,也可供任务 6 调试。引用的 `WifiConfigService::ProvisioningMode` 枚举值(NONE/CONNECTED/RETRY_WAIT/AP_FALLBACK)均存在于 `wifi_config_service.h`。
- **RAM 预算:** 无新增分配;`updateProvisioning` 的 static 缓存早已存在。预期静态 RAM 不变;任务 1 步骤 4 仍要求如实报告。

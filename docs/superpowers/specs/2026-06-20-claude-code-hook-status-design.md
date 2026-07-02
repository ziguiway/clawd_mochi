# Claude Code 实时 Hook 状态功能设计

## 1. 概述

### 1.1 项目背景

Clawd Mochi 是一个基于 ESP32-C3 的桌面伴侣，使用 ST7789 1.54" 240x240 彩色 TFT 显示屏。当前功能包括眼睛动画、画布模式等，通过 Web 控制器进行控制。

本设计旨在为 Clawd Mochi 添加 Claude Code 实时状态显示功能，参考 Cube-X 项目的实现方式。

### 1.2 核心功能

- **实时 Hook 状态显示**：接收并显示 Claude Code 的 hook 事件
- **自适应显示模式**：空闲时显示眼睛动画，工作时显示详细信息
- **Web 配网**：参考小智的配网流程，支持 WiFi 扫描和选择
- **串口命令**：支持恢复出厂设置、查看日志等命令
- **日志系统**：带时间戳的本地日志，支持 Web 查看
- **NTP 时间同步**：配网后自动同步时间

### 1.3 硬件规格

| 组件 | 规格 |
|------|------|
| MCU | ESP32-C3 Super Mini |
| 显示屏 | ST7789 1.54" 240x240 TFT |
| 通信 | WiFi (AP/STA) |
| 存储 | LittleFS |

---

## 2. 系统架构

### 2.1 项目结构

```
clawd_mochi/
├── data/                          # 静态文件（LittleFS）
│   ├── index.html                 # 主页面
│   ├── wifi_setup.html            # 配网页面
│   ├── logs.html                  # 日志查看页面
│   ├── style.css                  # 样式文件
│   ├── app.js                     # 主逻辑
│   ├── claude_code.js             # Claude Code 状态逻辑
│   └── wifi.js                    # 配网逻辑
├── src/
│   ├── main.cpp                   # 主程序入口
│   ├── config/
│   │   ├── app_config.h           # 全局配置
│   │   ├── cfg_claude_code.h      # Claude Code 配置
│   │   ├── cfg_display.h          # 显示配置
│   │   └── cfg_wifi.h             # WiFi 配置
│   ├── hardware/
│   │   └── tft_display.h/.cpp     # ST7789 硬件驱动
│   ├── service/
│   │   ├── claude_code_service.h/.cpp  # Claude Code UDP 服务
│   │   ├── display_service.h/.cpp      # 显示服务
│   │   ├── web_service.h/.cpp          # Web 服务器
│   │   ├── wifi_config_service.h/.cpp  # WiFi 配网服务
│   │   ├── time_service.h/.cpp         # NTP 时间服务
│   │   └── serial_command_service.h/.cpp # 串口命令服务
│   ├── states/
│   │   ├── claude_code_state.h/.cpp    # Claude Code 状态
│   │   ├── eyes_state.h/.cpp           # 眼睛动画状态
│   │   └── canvas_state.h/.cpp         # 画布状态
│   ├── view/
│   │   ├── claude_code_view.h/.cpp     # Claude Code 视图
│   │   └── eyes_view.h/.cpp            # 眼睛视图
│   └── utils/
│       ├── state_machine.h/.cpp        # 状态机框架
│       └── logger.h/.cpp               # 日志工具
├── platformio.ini                 # PlatformIO 配置
└── README.md
```

### 2.2 分层架构

```
┌─────────────────────────────────────────────────────────┐
│                    Web 控制器                            │
│              (HTML/CSS/JavaScript)                       │
├─────────────────────────────────────────────────────────┤
│                    Web 服务层                            │
│           (WebService, 路由处理)                         │
├─────────────────────────────────────────────────────────┤
│                    状态层                                │
│    (StateMachine, ClaudeCodeState, EyesState)           │
├─────────────────────────────────────────────────────────┤
│                    服务层                                │
│  (ClaudeCodeService, WifiConfigService, TimeService)    │
├─────────────────────────────────────────────────────────┤
│                    硬件层                                │
│            (TftDisplay, WiFi, LittleFS)                 │
└─────────────────────────────────────────────────────────┘
```

### 2.3 数据流

```
cc_hook.py (PC) ──UDP──> ClaudeCodeService ──> StateMachine ──> ClaudeCodeView ──> TFT
                              │
                              ├──> Serial Log
                              └──> Web API ──> Web Interface
```

---

## 3. Claude Code 服务设计

### 3.1 UDP 消息格式

**消息格式：**
```
CC:<event>[,<hook_name>[,<tool_name>[,<detail>[,<model>]]]]
```

**示例：**
```
CC:working,PreToolUse,Bash,git status,claude-sonnet-4-6
CC:thinking,UserPromptSubmit,,,claude-sonnet-4-6
CC:permission,PermissionRequest,,,
CC:done,Stop,,,
```

**字段说明：**

| 字段 | 说明 | 示例 |
|------|------|------|
| event | 状态事件 | working, thinking, done |
| hook_name | Hook 名称 | PreToolUse, PostToolUse |
| tool_name | 工具名称 | Bash, Read, Edit |
| detail | 详情 | git status, main.cpp |
| model | 模型名称 | claude-sonnet-4-6 |

### 3.2 状态机

**状态定义：**

| 状态 | 含义 | 触发条件 | 退出条件 |
|------|------|----------|----------|
| IDLE | 空闲 | SessionStart | 收到事件 |
| THINKING | 思考中 | UserPromptSubmit | 工具调用/完成 |
| WORKING | 执行工具 | PreToolUse/PostToolUse | 完成/失败 |
| ERROR | 出错 | PostToolUseFailure | 超时/新任务 |
| DONE | 完成 | Stop | 超时/新任务 |
| PERMISSION | 等待权限 | PermissionRequest | 权限操作 |
| SWEEPING | 上下文压缩 | PreCompact | PostCompact |
| SLEEPING | 休眠 | SessionEnd/超时 | 10秒后退出 |

**状态转换图：**
```
                    ┌──────────────────────────────────────┐
                    │                                      │
                    ▼                                      │
┌──────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐  │
│ IDLE │───>│ THINKING │───>│ WORKING  │───>│   DONE   │──┘
└──────┘    └──────────┘    └──────────┘    └──────────┘
                │               │                │
                │               ▼                │
                │          ┌──────────┐          │
                │          │  ERROR   │──────────┘
                │          └──────────┘
                │               │
                ▼               ▼
           ┌──────────┐   ┌──────────┐
           │PERMISSION│   │ SWEEPING │
           └──────────┘   └──────────┘
                │               │
                └───────────────┘
                        │
                        ▼
                   ┌──────────┐
                   │ SLEEPING │
                   └──────────┘
```

### 3.3 ClaudeCodeService 类

```cpp
class ClaudeCodeService {
public:
    enum class Status {
        IDLE, THINKING, WORKING, ERROR, DONE,
        PERMISSION, SWEEPING, SLEEPING
    };

    ClaudeCodeService(WiFiUDP* udp, StateMachine* sm);
    void init();
    void update();

    // Getters
    Status getStatus() const;
    const char* getStatusText() const;
    const char* getHookName() const;
    const char* getToolName() const;
    const char* getDetail() const;
    const char* getModel() const;
    unsigned long getElapsedMs() const;
    bool isActive() const;

private:
    WiFiUDP* _udp;
    StateMachine* _stateMachine;
    Status _status;
    
    char _hookName[32];
    char _toolName[32];
    char _detail[64];
    char _model[20];
    
    unsigned long _lastActivityMs;
    unsigned long _taskStartMs;
    bool _taskActive;
    unsigned long _sleepStartMs;
    unsigned long _wifiConnectedMs;

    void processPacket(const char* data, int len);
    void setStatus(Status status, const char* hookName = nullptr,
                   const char* toolName = nullptr, const char* detail = nullptr);
};
```

### 3.4 超时机制

- **无活动超时**：60 秒（可配置）
- **SLEEPING 持续时间**：10 秒后自动退出

---

## 4. 显示设计

### 4.1 显示模式

**模式 A：表情模式（空闲时）**

```
┌────────────────────────────────────────┐
│                                        │
│                                        │
│         ■                   ■          │  ← 眼睛动画
│         ■                   ■          │
│                                        │
│                                        │
│                                        │
│                                        │
│                                        │
└────────────────────────────────────────┘
```

**模式 B：信息模式（工作时）**

```
┌────────────────────────────────────────┐
│  Claude Code                           │
│  ────────────────────────────────────── │
│  Status: WORKING                       │
│  ────────────────────────────────────── │
│  Hook: PreToolUse                      │
│  Tool: Bash                            │
│  ────────────────────────────────────── │
│  Detail: git status --porcelain        │
│  ────────────────────────────────────── │
│  Model: claude-sonnet-4-6              │
│  Time: 00:12                           │
└────────────────────────────────────────┘
```

### 4.2 模式切换逻辑

```cpp
enum class DisplayMode {
    EXPRESSION,  // 表情模式
    INFO         // 信息模式
};

void updateDisplayMode(ClaudeCodeService::Status status) {
    if (status == Status::IDLE || status == Status::SLEEPING) {
        if (currentMode != DisplayMode::EXPRESSION) {
            currentMode = DisplayMode::EXPRESSION;
            switchToExpressionMode();
        }
    } else {
        if (currentMode != DisplayMode::INFO) {
            currentMode = DisplayMode::INFO;
            switchToInfoMode();
        }
    }
}
```

### 4.3 颜色方案

```cpp
// 主题色
#define COLOR_ORANGE    0xFC00  // 橙色（主题色）
#define COLOR_DARKBG    0x0821  // 深蓝黑（背景）
#define COLOR_WHITE     0xFFFF  // 白色
#define COLOR_GREEN     0x07E0  // 绿色（成功）
#define COLOR_RED       0xF800  // 红色（错误）
#define COLOR_YELLOW    0xFFE0  // 黄色（警告）
#define COLOR_GRAY      0x8410  // 灰色（次要信息）
#define COLOR_BLUE      0x001F  // 蓝色（思考）

// 状态颜色映射
IDLE/SLEEPING → 橙色眼睛
THINKING → 蓝色
WORKING → 绿色
ERROR → 红色
DONE → 绿色
PERMISSION → 黄色
SWEEPING → 橙色
```

### 4.4 ClaudeCodeView 类

```cpp
class ClaudeCodeView {
public:
    explicit ClaudeCodeView(TftDisplay* tft);
    
    void render(Status status, const char* hookName,
                const char* toolName, const char* detail,
                const char* model, unsigned long elapsedMs);
    
    void renderEyesMode();  // 眼睛动画模式

private:
    TftDisplay* _tft;
    unsigned long _lastFrameMs;
    
    void drawHeader(Status status);
    void drawHookInfo(const char* hookName, const char* toolName);
    void drawDetail(const char* detail);
    void drawFooter(const char* model, unsigned long elapsedMs);
    void drawStatusIcon(Status status, int x, int y);
    void formatElapsed(unsigned long ms, char* buf, size_t bufSize);
};
```

### 4.5 新增表情动画

| 状态 | 表情动画 | 颜色 |
|------|----------|------|
| IDLE | 正常眨眼 | 橙色 |
| THINKING | 眼睛快速左右移动 | 蓝色 |
| WORKING | 眼睛聚焦，脉冲效果 | 绿色 |
| ERROR | 眼睛变红，X 形闪烁 | 红色 |
| DONE | 眼睛变绿，笑脸 | 绿色 |
| PERMISSION | 眼睛变黄，问号 | 黄色 |
| SWEEPING | 波浪效果 | 橙色 |
| SLEEPING | 闭眼 + ZZZ | 灰色 |

---

## 5. Web 控制器设计

### 5.1 静态文件管理

使用 LittleFS 存储静态文件，支持热更新。

**文件列表：**

| 文件 | 说明 |
|------|------|
| index.html | 主控制页面 |
| wifi_setup.html | WiFi 配网页面 |
| logs.html | 日志查看页面 |
| style.css | 共享样式 |
| app.js | 主逻辑 |
| claude_code.js | Claude Code 状态 |
| wifi.js | 配网逻辑 |

### 5.2 Web API 设计

**Claude Code 相关：**

| 路由 | 方法 | 说明 |
|------|------|------|
| /cc/status | GET | 获取 Claude Code 状态 |
| /cc/test | GET | 测试 Claude Code 连接 |

**WiFi 配网相关：**

| 路由 | 方法 | 说明 |
|------|------|------|
| /wifi/scan | GET | 扫描 WiFi 网络 |
| /wifi/connect | POST | 连接 WiFi |
| /wifi/status | GET | 获取 WiFi 状态 |

**日志相关：**

| 路由 | 方法 | 说明 |
|------|------|------|
| /logs | GET | 获取日志 |
| /logs/clear | POST | 清除日志 |
| /logs/status | GET | 获取日志状态 |

**其他：**

| 路由 | 方法 | 说明 |
|------|------|------|
| /time | GET | 获取当前时间 |
| /state | GET | 获取设备状态 |
| /cmd | GET | 发送命令 |
| /backlight | GET | 控制背光 |

### 5.3 Web 界面设计

**主页面功能：**
- 眼睛动画控制
- Claude Code 状态显示
- 画布模式
- 背景颜色选择
- 速度控制

**配网页面功能：**
- WiFi 网络扫描
- 网络列表显示（信号强度、加密类型）
- 密码输入
- 连接状态显示

**日志页面功能：**
- 日志级别过滤
- 实时刷新
- 清除日志

---

## 6. WiFi 配网设计

### 6.1 配网流程

```
┌─────────────────────────────────────────────────────────────┐
│                      启动流程                                │
├─────────────────────────────────────────────────────────────┤
│  1. 读取 LittleFS 中的 wifi.json                           │
│  2. 配置存在？                                              │
│     ├─ 是 → WiFi.begin(ssid, password)                     │
│     │   ├─ 15秒内连接成功 → STA 模式，显示主界面           │
│     │   └─ 超时 → 启动 AP 模式                             │
│     └─ 否 → 启动 AP 模式                                   │
│  3. AP 模式 → 启动 Web 服务器                              │
│  4. 用户访问 192.168.4.1 → 显示配网页面                    │
│  5. 点击"扫描" → GET /wifi/scan → 返回 WiFi 列表          │
│  6. 选择网络 → 输入密码 → 点击"连接"                      │
│  7. POST /wifi/connect → ESP32 尝试连接                    │
│  8. 连接成功 → 保存凭据 → 返回成功 + IP                    │
│  9. 前端显示"已连接" → 3秒后跳转主界面                     │
│ 10. ESP32 重启 → 回到步骤 1                                │
└─────────────────────────────────────────────────────────────┘
```

### 6.2 WifiConfigService 类

```cpp
class WifiConfigService {
public:
    WifiConfigService();
    void init();
    void update();
    
    bool isConfigured();
    bool isConnected();
    String getIP();
    String getSSID();
    
    void startAPMode();
    bool connectToWifi(const char* ssid, const char* password);
    void saveCredentials(const char* ssid, const char* password);
    void clearCredentials();
    void reset();
    
    void handleScanRequest(WebServer& server);
    void handleConnectRequest(WebServer& server);
    void handleStatusRequest(WebServer& server);

private:
    String _ssid;
    String _password;
    bool _configured;
    bool _connected;
    bool _connecting;
    unsigned long _connectStartTime;
    
    void loadCredentials();
};
```

### 6.3 凭据存储

**文件：** `/wifi.json`

**格式：**
```json
{
    "ssid": "MyWiFi",
    "password": "mypassword123"
}
```

### 6.4 硬件重置

长按 BOOT 按钮 5 秒恢复出厂设置。

---

## 7. 串口命令设计

### 7.1 命令列表

| 命令 | 功能 |
|------|------|
| `help` 或 `?` | 显示帮助信息 |
| `reset` | 恢复出厂设置（清除 WiFi 配置） |
| `restart` 或 `reboot` | 重启设备 |
| `status` | 显示当前状态 |
| `ip` | 显示 IP 地址 |
| `time` | 显示当前时间 |
| `time sync` | 手动同步时间 |
| `log` | 显示最近日志 |
| `log clear` | 清除日志 |
| `log level <n>` | 设置日志级别 (debug/info/warn/error) |

### 7.2 SerialCommandService 类

```cpp
class SerialCommandService {
public:
    SerialCommandService(WifiConfigService* wifiService, 
                        ClaudeCodeService* ccService,
                        TimeService* timeService);
    void init();
    void update();

private:
    WifiConfigService* _wifiService;
    ClaudeCodeService* _ccService;
    TimeService* _timeService;
    String _inputBuffer;
    
    void processCommand(const String& cmd);
    void printHelp();
    void printStatus();
    void printIP();
    void showTime();
    void syncTime();
    void showLogs();
    void clearLogs();
    void setLogLevel(const String& level);
    void resetFactory();
    void restart();
};
```

---

## 8. 日志系统设计

### 8.1 功能特性

- **时间戳**：使用 NTP 同步的真实时间
- **日志级别**：DEBUG, INFO, WARN, ERROR
- **本地保存**：存储到 LittleFS
- **自动轮转**：超过最大大小自动轮转
- **串口输出**：同时输出到串口
- **Web 查看**：通过浏览器查看日志

### 8.2 日志大小

**推荐：20KB**（约 200-300 条日志）

| 场景 | 建议大小 | 说明 |
|------|----------|------|
| 开发调试 | 50KB | 保留更多日志 |
| 正常使用 | 20KB | 平衡存储和历史 |
| 存储紧张 | 10KB | 最小配置 |

### 8.3 Logger 类

```cpp
class Logger {
public:
    static Logger& getInstance();
    
    void init(TimeService* timeService, const char* logPath = "/logs/system.log", 
              size_t maxSize = 20480);
    void setLevel(LogLevel level);
    
    void debug(const char* tag, const char* format, ...);
    void info(const char* tag, const char* format, ...);
    void warn(const char* tag, const char* format, ...);
    void error(const char* tag, const char* format, ...);
    
    String getLogs(size_t maxLines = 100);
    String getLogsByLevel(LogLevel level, size_t maxLines = 100);
    void clearLogs();
    size_t getLogSize();
    
    void setSerialOutput(bool enabled);
    void setFileOutput(bool enabled);

private:
    Logger();
    
    TimeService* _timeService;
    String _logPath;
    size_t _maxSize;
    LogLevel _currentLevel;
    bool _serialOutput;
    bool _fileOutput;
    
    void writeToFile(const String& entry);
    void rotateLog();
    String getTimestamp();
    String getLevelStr(LogLevel level);
};

// 便捷宏
#define LOG_DEBUG(tag, ...) Logger::getInstance().debug(tag, __VA_ARGS__)
#define LOG_INFO(tag, ...) Logger::getInstance().info(tag, __VA_ARGS__)
#define LOG_WARN(tag, ...) Logger::getInstance().warn(tag, __VA_ARGS__)
#define LOG_ERROR(tag, ...) Logger::getInstance().error(tag, __VA_ARGS__)
```

### 8.4 日志格式

```
2026-06-20 14:30:45.123 [INFO ] [ClaudeCode] 收到: CC:working,PreToolUse,Bash,git status
2026-06-20 14:30:45.456 [DEBUG] [ClaudeCode] 状态变更: THINKING -> WORKING
2026-06-20 14:30:46.789 [WARN ] [WiFi] 信号较弱: -75 dBm
2026-06-20 14:31:00.123 [ERROR] [ClaudeCode] UDP 接收超时
```

---

## 9. 时间同步设计

### 9.1 NTP 配置

**NTP 服务器（阿里云）：**
- `ntp.aliyun.com`
- `ntp1.aliyun.com`
- `ntp2.aliyun.com`

**时区：** GMT+8（东八区）

**同步间隔：** 1 小时

### 9.2 TimeService 类

```cpp
class TimeService {
public:
    TimeService();
    void init();
    void update();
    
    bool isSynced();
    String getTime();
    String getDate();
    String getDateTime();
    String getTimestamp();
    
    unsigned long getEpoch();
    int getHour();
    int getMinute();
    int getSecond();
    int getYear();
    int getMonth();
    int getDay();
    
    void syncNow();

private:
    bool _synced;
    unsigned long _lastSyncTime;
    unsigned long _syncInterval;
    
    void configTime();
    bool waitForSync(unsigned long timeoutMs = 10000);
};
```

### 9.3 同步时机

1. **开机时**：WiFi 连接后自动同步
2. **定期同步**：每小时同步一次
3. **手动同步**：通过串口命令 `time sync`

---

## 10. cc_hook.py 修改

### 10.1 消息格式修改

**新格式：**
```python
# CC:<event>,<hook>,<tool>,<detail>,<model>
msg = f"CC:{event},{hook},{tool_name},{tool_input[:30]}"
if model:
    msg += f",{model}"
```

### 10.2 完整修改

```python
def main() -> None:
    hook = sys.argv[1] if len(sys.argv) > 1 else ""
    event = HOOK_MAP.get(hook)
    if not event:
        sys.exit(0)

    data: dict = {}
    if not sys.stdin.isatty():
        try:
            data = json.load(sys.stdin)
        except json.JSONDecodeError:
            pass

    # 提取信息
    tool_name = data.get("tool_name", "")
    tool_input = data.get("tool_input", "")
    model = data.get("model", "") or os.environ.get("CLAUDE_MODEL", "") or os.environ.get("ANTHROPIC_MODEL", "")
    
    # 构建消息
    msg = f"CC:{event}"
    msg += f",{hook}"                    # hook 名称
    msg += f",{tool_name}"               # 工具名
    msg += f",{tool_input[:30]}"         # 详情（截断到30字符）
    if model:
        msg += f",{model}"

    ip = find_esp32()
    if not ip:
        sys.exit(0)

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.sendto(msg.encode(), (ip, ESP32_PORT))
        sock.close()
    except OSError:
        pass
```

---

## 11. 实现阶段

### 11.1 阶段划分

| 阶段 | 内容 | 预计时间 |
|------|------|----------|
| 1 | 基础架构（目录结构、配置、状态机） | 1-2 天 |
| 2 | Claude Code 服务（UDP 接收、状态机） | 2-3 天 |
| 3 | 显示层（TFT 驱动、视图、动画） | 2-3 天 |
| 4 | Web 控制器（静态文件、API） | 2-3 天 |
| 5 | WiFi 配网（扫描、连接、存储） | 1-2 天 |
| 6 | 日志和时间服务 | 1 天 |
| 7 | 串口命令 | 0.5 天 |
| 8 | cc_hook.py 修改 | 0.5 天 |
| 9 | 集成测试和优化 | 1-2 天 |

**总计：约 11-16 天**

### 11.2 关键文件

| 文件 | 说明 |
|------|------|
| `platformio.ini` | PlatformIO 配置 |
| `src/main.cpp` | 主程序入口 |
| `src/service/claude_code_service.cpp` | Claude Code 核心服务 |
| `src/service/wifi_config_service.cpp` | WiFi 配网服务 |
| `src/service/web_service.cpp` | Web 服务器 |
| `data/index.html` | 主页面 |
| `data/wifi_setup.html` | 配网页面 |
| `scripts/cc_hook.py` | PC 端 hook 脚本 |

---

## 12. 验证方法

### 12.1 单元测试

```cpp
// 测试 UDP 消息解析
void test_process_packet() {
    ClaudeCodeService service(nullptr, nullptr);
    
    // 测试新格式
    service.processPacket("CC:working,PreToolUse,Bash,git status", 35);
    assert(service.getStatus() == ClaudeCodeService::Status::WORKING);
    assert(strcmp(service.getHookName(), "PreToolUse") == 0);
    assert(strcmp(service.getToolName(), "Bash") == 0);
    assert(strcmp(service.getDetail(), "git status") == 0);
}
```

### 12.2 集成测试

```bash
# 发送测试 UDP 消息
echo "CC:working,PreToolUse,Bash,git status" | nc -u 192.168.4.1 4210

# 验证串口输出
# 验证 TFT 显示
# 验证 Web 界面更新
```

### 12.3 端到端测试

1. 启动 Claude Code
2. 执行命令（如 `git status`）
3. 观察 Clawd Mochi 显示
4. 检查 Web 界面状态
5. 验证日志记录

### 12.4 性能指标

| 指标 | 目标 |
|------|------|
| UDP 延迟 | < 100ms |
| 显示更新 | < 50ms |
| Web 响应 | < 200ms |
| 内存使用 | < 50KB |
| CPU 使用 | < 30% |

---

## 13. 风险和缓解

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| UDP 丢包 | 状态不同步 | 添加心跳机制 |
| 内存不足 | 崩溃 | 优化缓冲区大小 |
| Web 服务器过载 | 响应慢 | 添加缓存 |
| 显示刷新慢 | 卡顿 | 优化绘制逻辑 |
| WiFi 连接不稳定 | 配网失败 | 重试机制 + 错误提示 |
| NTP 同步失败 | 时间不准确 | 多 NTP 服务器 + 重试 |

---

## 14. 附录

### 14.1 配置文件示例

**cfg_claude_code.h：**
```cpp
#define CFG_CLAUDE_CODE_UDP_PORT          4210
#define CFG_CLAUDE_CODE_TIMEOUT_MS        60000
#define CFG_CLAUDE_CODE_RX_BUF_SIZE       128
#define CFG_CLAUDE_CODE_HOOK_MAX_LEN      32
#define CFG_CLAUDE_CODE_TOOL_MAX_LEN      32
#define CFG_CLAUDE_CODE_DETAIL_MAX_LEN    64
#define CFG_CLAUDE_CODE_MODEL_MAX_LEN     20
```

**cfg_wifi.h：**
```cpp
#define CFG_WIFI_AP_SSID                  "ClaWD-Mochi"
#define CFG_WIFI_AP_PASSWORD              "clawd1234"
#define CFG_WIFI_CONNECT_TIMEOUT_MS       15000
#define CFG_WIFI_CRED_SSID_MAX_LEN        32
#define CFG_WIFI_CRED_PASS_MAX_LEN        64
```

### 14.2 platformio.ini 配置

```ini
[env:esp32-c3-devkitc-02]
platform = espressif32
board = esp32-c3-devkitc-02
framework = arduino

; 启用 LittleFS
board_build.filesystem = littlefs

; 库依赖
lib_deps = 
    adafruit/Adafruit GFX Library
    adafruit/Adafruit ST7735 and ST7789 Library
    bblanchon/ArduinoJson

; 上传速度
upload_speed = 921600

; 监视速度
monitor_speed = 115200
```

---

## 设计总结

本设计为 Clawd Mochi 添加了完整的 Claude Code 实时状态显示功能，包括：

1. **UDP 通信**：接收 Claude Code hook 事件
2. **状态机**：管理 Claude Code 状态转换
3. **自适应显示**：空闲时显示眼睛，工作时显示信息
4. **Web 控制器**：完整的 Web 界面控制
5. **WiFi 配网**：参考小智的配网流程
6. **串口命令**：支持恢复出厂设置等命令
7. **日志系统**：带时间戳的本地日志
8. **时间同步**：NTP 自动同步时间

设计参考了 Cube-X 项目的架构，保持了代码的一致性和可维护性。

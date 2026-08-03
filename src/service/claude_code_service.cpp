#include "claude_code_service.h"
#include "time_service.h"
#include "operation_mode_service.h"
#include "wifi_config_service.h"
#include "../utils/logger.h"

ClaudeCodeService::ClaudeCodeService(StateMachine* sm, TimeService* timeService)
    : _stateMachine(sm)
    , _timeService(timeService)
    , _status(Status::IDLE)
    , _taskStartMs(0)
    , _taskElapsedMs(0)
    , _taskActive(false)
    , _sleepStartMs(0)
    , _lastStatusEventMs(0)
    , _initialized(false)
    , _statsDateKey(0)
    , _todayWorkingMs(0)
    , _sessionWorkingMs(0)
    , _longestWorkingMs(0)
    , _doneCount(0)
    , _errorCount(0)
    , _permissionCount(0)
    , _workingSegmentStartMs(0)
    , _statsDirty(false)
    , _lastStatsPersistMs(0)
    , _statsLoaded(false)
{
    _hookName[0] = '\0';
    _toolName[0] = '\0';
    _detail[0] = '\0';
    _model[0] = '\0';
}

bool ClaudeCodeService::canRun() const {
    auto* opMode = OperationModeService::current();
    if (!opMode) return false;
    if (opMode->isSerial()) return true;
    // LAN 模式需要 WiFi 连上
    auto* wifi = WifiConfigService::current();
    return wifi && wifi->isConnected();
}

void ClaudeCodeService::init() {
    // 懒启动:在 update() 里根据 canRun() 决定何时真正初始化
    // 统计持久化在启动时即可加载(NVS 不依赖 WiFi)
    if (!_statsLoaded) {
        _statsPrefs.begin("mochi-ccstats", false);
        loadStats();
        _statsLoaded = true;
    }
}

void ClaudeCodeService::update() {
    // 懒启动:第一次 canRun() 时初始化 UDP
    if (!_initialized && canRun()) {
        auto* opMode = OperationModeService::current();
        if (opMode && !opMode->isSerial()) {
            _udp.begin(CFG_CLAUDE_CODE_UDP_PORT);
            LOG_INFO("ClaudeCode", "UDP 监听端口: %d", CFG_CLAUDE_CODE_UDP_PORT);
        } else {
            LOG_INFO("ClaudeCode", "串口模式:跳过 UDP,等待 injectStatus");
        }
        _initialized = true;
    }
    if (!_initialized) return;

    // 仅 LAN 模式监听 UDP
    auto* opMode = OperationModeService::current();
    bool isSerial = opMode && opMode->isSerial();
    if (!isSerial) {
        int packetSize = _udp.parsePacket();
        if (packetSize > 0) {
            char buf[CFG_CLAUDE_CODE_RX_BUF_SIZE];
            int len = _udp.read(buf, sizeof(buf) - 1);
            if (len > 0) {
                buf[len] = '\0';
                processPacket(buf, len);
            }
        }
    }

    unsigned long now = millis();
    if (_status == Status::SLEEPING) {
        if (now - _sleepStartMs > CFG_CLAUDE_CODE_SLEEP_DURATION_MS) {
            LOG_INFO("ClaudeCode", "休眠结束，回到空闲");
            setStatus(Status::IDLE);
        }
    } else if (isActiveStatus(_status)
               && now - _lastStatusEventMs >= CFG_CLAUDE_CODE_ACTIVE_TIMEOUT_MS) {
        LOG_WARN("ClaudeCode", "活跃状态超时，回到空闲");
        setStatus(Status::IDLE);
    }

    static unsigned long lastDiscovery = 0;
    if (millis() - lastDiscovery > 5000) {
        lastDiscovery = millis();
        sendDiscovery();
    }

    // 统计:跨天归零 + 节流持久化
    checkDayRollover();
    if (_statsDirty && now - _lastStatsPersistMs > 15000UL) {
        persistStats();
        _statsDirty = false;
        _lastStatsPersistMs = now;
    }
}

void ClaudeCodeService::processPacket(const char* data, int len) {
    LOG_INFO("ClaudeCode", "收到: %s", data);

    if (strncmp(data, "CC:", 3) != 0) return;

    // ping 探测:回 pong:<mode>,供 hook 判断当前模式
    // 不改状态
    if (strcmp(data + 3, "ping") == 0) {
        const char* mode = "lan";
        auto* opMode = OperationModeService::current();
        if (opMode && opMode->isSerial()) mode = "serial";
        char reply[16];
        snprintf(reply, sizeof(reply), "CC:pong:%s", mode);
        _udp.beginPacket(_udp.remoteIP(), _udp.remotePort());
        _udp.write((const uint8_t*)reply, strlen(reply));
        _udp.endPacket();
        return;
    }

    const char* p = data + 3;
    char event[16] = {0};
    char hook[CFG_CLAUDE_CODE_HOOK_MAX_LEN] = {0};
    char tool[CFG_CLAUDE_CODE_TOOL_MAX_LEN] = {0};
    char detail[CFG_CLAUDE_CODE_DETAIL_MAX_LEN] = {0};
    char model[CFG_CLAUDE_CODE_MODEL_MAX_LEN] = {0};

    // 解析 event
    const char* comma = strchr(p, ',');
    if (comma) {
        size_t l = comma - p;
        if (l < sizeof(event)) { strncpy(event, p, l); event[l] = '\0'; }
        p = comma + 1;
    } else {
        strncpy(event, p, sizeof(event) - 1);
        // session_start:新会话开始,重置会话级统计(今日累计保留)
        if (strcmp(event, "session_start") == 0) resetSessionStats();
        setStatus(mapEventToStatus(event));
        return;
    }
    // session_start 即便携带字段也重置会话统计
    if (strcmp(event, "session_start") == 0) resetSessionStats();

    // 解析 hook
    comma = strchr(p, ',');
    if (comma) {
        size_t l = comma - p;
        if (l < sizeof(hook)) { strncpy(hook, p, l); hook[l] = '\0'; }
        p = comma + 1;
    } else {
        strncpy(hook, p, sizeof(hook) - 1);
        setStatus(mapEventToStatus(event), hook);
        return;
    }

    // 解析 tool
    comma = strchr(p, ',');
    if (comma) {
        size_t l = comma - p;
        if (l < sizeof(tool)) { strncpy(tool, p, l); tool[l] = '\0'; }
        p = comma + 1;
    } else {
        strncpy(tool, p, sizeof(tool) - 1);
        setStatus(mapEventToStatus(event), hook, tool);
        return;
    }

    // 解析 detail
    comma = strchr(p, ',');
    if (comma) {
        size_t l = comma - p;
        if (l < sizeof(detail)) { strncpy(detail, p, l); detail[l] = '\0'; }
        p = comma + 1;
        strncpy(model, p, sizeof(model) - 1);
    } else {
        strncpy(detail, p, sizeof(detail) - 1);
    }

    setStatus(mapEventToStatus(event), hook, tool, detail, model);
}

ClaudeCodeService::Status ClaudeCodeService::mapEventToStatus(const char* event) {
    if (strcmp(event, "session_start") == 0) return Status::IDLE;
    if (strcmp(event, "session_end") == 0)   return Status::SLEEPING;
    if (strcmp(event, "working") == 0)    return Status::WORKING;
    if (strcmp(event, "thinking") == 0)   return Status::THINKING;
    if (strcmp(event, "permission") == 0) return Status::PERMISSION;
    if (strcmp(event, "done") == 0)       return Status::DONE;
    if (strcmp(event, "error") == 0)      return Status::ERROR;
    if (strcmp(event, "sweeping") == 0)   return Status::SWEEPING;
    if (strcmp(event, "compacting") == 0) return Status::SWEEPING;
    if (strcmp(event, "Compacting") == 0) return Status::SWEEPING;
    if (strcmp(event, "COMPACTING") == 0) return Status::SWEEPING;
    if (strcmp(event, "sleeping") == 0)   return Status::SLEEPING;
    if (strcmp(event, "idle") == 0)       return Status::IDLE;
    return Status::WORKING;
}

void ClaudeCodeService::setStatus(Status status, const char* hookName,
                                   const char* toolName, const char* detail,
                                   const char* model) {
    _lastStatusEventMs = millis();
    updateTaskClock(status);
    // 统计累积:在 _status 被覆盖前用旧值检测转移。
    updateStats(_status, status);
    if (status == Status::SLEEPING && _status != Status::SLEEPING) {
        _sleepStartMs = millis();
    } else if (status == Status::IDLE) {
        _sleepStartMs = 0;
    }
    _status = status;
    // 空值守卫:hook 在多数事件(Stop/PostToolUse 等)里不携带 model,
    // stdin JSON 也只有 SessionStart 才有 model 字段。空串不覆盖,保留上次有效值,
    // 否则 working 后任意 done/error 事件都会把 model 清成 "-"。同理 hook/tool/detail。
    if (hookName && hookName[0]) strncpy(_hookName, hookName, CFG_CLAUDE_CODE_HOOK_MAX_LEN - 1);
    if (toolName && toolName[0]) strncpy(_toolName, toolName, CFG_CLAUDE_CODE_TOOL_MAX_LEN - 1);
    if (detail && detail[0])     strncpy(_detail, detail, CFG_CLAUDE_CODE_DETAIL_MAX_LEN - 1);
    if (model && model[0])       strncpy(_model, model, CFG_CLAUDE_CODE_MODEL_MAX_LEN - 1);
    LOG_INFO("ClaudeCode", "状态: %s hook=%s tool=%s", statusToText(status), _hookName, _toolName);
}

bool ClaudeCodeService::isActiveStatus(Status status) const {
    return status == Status::THINKING ||
           status == Status::WORKING ||
           status == Status::PERMISSION ||
           status == Status::SWEEPING;
}

void ClaudeCodeService::updateTaskClock(Status status) {
    unsigned long now = millis();
    switch (status) {
        case Status::THINKING:
            // Claude Code starts a new turn when the user prompt is submitted.
            _taskStartMs = now;
            _taskElapsedMs = 0;
            _taskActive = true;
            break;
        case Status::WORKING:
        case Status::PERMISSION:
        case Status::SWEEPING:
            if (!_taskActive) {
                _taskStartMs = now;
                _taskElapsedMs = 0;
                _taskActive = true;
            }
            break;
        case Status::DONE:
        case Status::ERROR:
            if (_taskStartMs) _taskElapsedMs = now - _taskStartMs;
            _taskActive = false;
            break;
        case Status::IDLE:
            _taskStartMs = 0;
            _taskElapsedMs = 0;
            _taskActive = false;
            break;
        case Status::SLEEPING:
            if (_taskActive && _taskStartMs) _taskElapsedMs = now - _taskStartMs;
            _taskActive = false;
            break;
    }
}

const char* ClaudeCodeService::statusToText(Status status) const {
    switch (status) {
        case Status::IDLE:       return "IDLE";
        case Status::THINKING:   return "THINKING";
        case Status::WORKING:    return "WORKING";
        case Status::ERROR:      return "ERROR";
        case Status::DONE:       return "DONE";
        case Status::PERMISSION: return "PERMISSION";
        case Status::SWEEPING:   return "COMPACTING";
        case Status::SLEEPING:   return "SLEEPING";
        default:                 return "UNKNOWN";
    }
}

ClaudeCodeService::Status ClaudeCodeService::getStatus() const { return _status; }
const char* ClaudeCodeService::getStatusText() const { return statusToText(_status); }
const char* ClaudeCodeService::getHookName() const { return _hookName; }
const char* ClaudeCodeService::getToolName() const { return _toolName; }
const char* ClaudeCodeService::getDetail() const { return _detail; }
const char* ClaudeCodeService::getModel() const { return _model; }

unsigned long ClaudeCodeService::getElapsedMs() const {
    if (_taskActive && _taskStartMs) return millis() - _taskStartMs;
    return _taskElapsedMs;
}

bool ClaudeCodeService::isActive() const {
    return _taskActive;
}

String ClaudeCodeService::getStatusJson() const {
    String json = "{";
    json += "\"status\":\"" + String(statusToText(_status)) + "\"";
    json += ",\"hook\":\"" + String(_hookName) + "\"";
    json += ",\"tool\":\"" + String(_toolName) + "\"";
    json += ",\"detail\":\"" + String(_detail) + "\"";
    json += ",\"model\":\"" + String(_model) + "\"";
    json += ",\"elapsed\":" + String(getElapsedMs());
    json += ",\"active\":" + String(isActive() ? "true" : "false");
    json += "}";
    return json;
}

void ClaudeCodeService::sendDiscovery() {
    if (WiFi.status() != WL_CONNECTED) return;
    IPAddress broadcastIP = WiFi.localIP();
    broadcastIP[3] = 255;
    _udp.beginPacket(broadcastIP, CFG_CLAUDE_CODE_DISCOVERY_PORT);
    _udp.print("CC_DISCOVER:ClawdMochi:");
    _udp.print(WiFi.localIP().toString());
    _udp.endPacket();
}

void ClaudeCodeService::injectStatus(Status status, const char* hookName,
                                     const char* toolName, const char* detail,
                                     const char* model) {
    setStatus(status, hookName, toolName, detail, model);
    LOG_INFO("ClaudeCode", "串口注入: %s", statusToText(status));
}

// ============================================================
// 会话/今日统计
// ============================================================

void ClaudeCodeService::updateStats(Status oldStatus, Status newStatus) {
    // WORKING 段闭合:离开 WORKING 时把这段时长累加进今日/会话
    if (oldStatus == Status::WORKING && newStatus != Status::WORKING &&
        _workingSegmentStartMs) {
        const uint32_t dur = static_cast<uint32_t>(millis() - _workingSegmentStartMs);
        _todayWorkingMs += dur;
        _sessionWorkingMs += dur;
        if (dur > _longestWorkingMs) _longestWorkingMs = dur;
        _workingSegmentStartMs = 0;
        _statsDirty = true;
    }
    // WORKING 段开启
    if (oldStatus != Status::WORKING && newStatus == Status::WORKING) {
        _workingSegmentStartMs = millis();
    }
    // 事件计数(仅在真正转入时 +1,避免同状态重复计数)
    if (newStatus == Status::DONE && oldStatus != Status::DONE) {
        _doneCount++;
        _statsDirty = true;
    } else if (newStatus == Status::ERROR && oldStatus != Status::ERROR) {
        _errorCount++;
        _statsDirty = true;
    } else if (newStatus == Status::PERMISSION && oldStatus != Status::PERMISSION) {
        _permissionCount++;
        _statsDirty = true;
    }
}

void ClaudeCodeService::resetSessionStats() {
    _sessionWorkingMs = 0;
    _statsDirty = true;
    LOG_INFO("ClaudeCode", "统计: 新会话,会话计时归零");
}

uint32_t ClaudeCodeService::currentDateKey() const {
    if (!_timeService || !_timeService->isSynced()) return 0;
    return static_cast<uint32_t>(_timeService->getYear()) * 10000UL +
           static_cast<uint32_t>(_timeService->getMonth()) * 100UL +
           static_cast<uint32_t>(_timeService->getDay());
}

void ClaudeCodeService::checkDayRollover() {
    const uint32_t dateKey = currentDateKey();
    if (dateKey == 0) return;  // 时间未同步,无法判定跨天
    if (_statsDateKey == 0) {
        // 首次同步:仅记录当天,不归零(可能是恢复的当日数据)
        _statsDateKey = dateKey;
        _statsDirty = true;
        return;
    }
    if (dateKey != _statsDateKey) {
        LOG_INFO("ClaudeCode", "统计: 跨天 %lu -> %lu,今日归零",
                 static_cast<unsigned long>(_statsDateKey),
                 static_cast<unsigned long>(dateKey));
        _todayWorkingMs = 0;
        _longestWorkingMs = 0;
        _doneCount = 0;
        _errorCount = 0;
        _permissionCount = 0;
        _statsDateKey = dateKey;
        _statsDirty = true;
    }
}

void ClaudeCodeService::loadStats() {
    _statsDateKey = _statsPrefs.getUInt("dateKey", 0);
    _todayWorkingMs = _statsPrefs.getUInt("todayMs", 0);
    _sessionWorkingMs = _statsPrefs.getUInt("sessionMs", 0);
    _longestWorkingMs = _statsPrefs.getUInt("longestMs", 0);
    _doneCount = _statsPrefs.getUShort("done", 0);
    _errorCount = _statsPrefs.getUShort("error", 0);
    _permissionCount = _statsPrefs.getUShort("perm", 0);
    LOG_INFO("ClaudeCode", "统计载入: 今日%lums 会话%lums done=%u err=%u perm=%u",
             static_cast<unsigned long>(_todayWorkingMs),
             static_cast<unsigned long>(_sessionWorkingMs),
             _doneCount, _errorCount, _permissionCount);
}

void ClaudeCodeService::persistStats() {
    _statsPrefs.putUInt("dateKey", _statsDateKey);
    _statsPrefs.putUInt("todayMs", _todayWorkingMs);
    _statsPrefs.putUInt("sessionMs", _sessionWorkingMs);
    _statsPrefs.putUInt("longestMs", _longestWorkingMs);
    _statsPrefs.putUShort("done", _doneCount);
    _statsPrefs.putUShort("error", _errorCount);
    _statsPrefs.putUShort("perm", _permissionCount);
}

uint32_t ClaudeCodeService::getTodayWorkingMs() const {
    if (_status == Status::WORKING && _workingSegmentStartMs) {
        return _todayWorkingMs + static_cast<uint32_t>(millis() - _workingSegmentStartMs);
    }
    return _todayWorkingMs;
}

uint32_t ClaudeCodeService::getSessionWorkingMs() const {
    if (_status == Status::WORKING && _workingSegmentStartMs) {
        return _sessionWorkingMs + static_cast<uint32_t>(millis() - _workingSegmentStartMs);
    }
    return _sessionWorkingMs;
}

uint32_t ClaudeCodeService::getLongestWorkingMs() const { return _longestWorkingMs; }
uint16_t ClaudeCodeService::getDoneCount() const { return _doneCount; }
uint16_t ClaudeCodeService::getErrorCount() const { return _errorCount; }
uint16_t ClaudeCodeService::getPermissionCount() const { return _permissionCount; }
uint32_t ClaudeCodeService::getStatsDateKey() const { return _statsDateKey; }

String ClaudeCodeService::getStatsJson() const {
    String json = "{";
    json += "\"todayMs\":" + String(getTodayWorkingMs());
    json += ",\"sessionMs\":" + String(getSessionWorkingMs());
    json += ",\"longestMs\":" + String(_longestWorkingMs);
    json += ",\"done\":" + String(_doneCount);
    json += ",\"error\":" + String(_errorCount);
    json += ",\"permission\":" + String(_permissionCount);
    json += ",\"dateKey\":" + String(_statsDateKey);
    json += ",\"working\":" + String(_status == Status::WORKING ? "true" : "false");
    json += "}";
    return json;
}

void ClaudeCodeService::resetStats() {
    _todayWorkingMs = 0;
    _sessionWorkingMs = 0;
    _longestWorkingMs = 0;
    _doneCount = 0;
    _errorCount = 0;
    _permissionCount = 0;
    _workingSegmentStartMs = (_status == Status::WORKING) ? millis() : 0;
    _statsDirty = true;
    persistStats();
    _statsDirty = false;
    _lastStatsPersistMs = millis();
    LOG_INFO("ClaudeCode", "统计: 已重置");
}


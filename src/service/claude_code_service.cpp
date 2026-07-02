#include "claude_code_service.h"
#include "../utils/logger.h"

ClaudeCodeService::ClaudeCodeService(StateMachine* sm)
    : _stateMachine(sm)
    , _status(Status::IDLE)
    , _lastActivityMs(0)
    , _taskStartMs(0)
    , _taskActive(false)
    , _sleepStartMs(0)
{
    _hookName[0] = '\0';
    _toolName[0] = '\0';
    _detail[0] = '\0';
    _model[0] = '\0';
}

void ClaudeCodeService::init() {
    _udp.begin(CFG_CLAUDE_CODE_UDP_PORT);
    LOG_INFO("ClaudeCode", "UDP 监听端口: %d", CFG_CLAUDE_CODE_UDP_PORT);
}

void ClaudeCodeService::update() {
    int packetSize = _udp.parsePacket();
    if (packetSize > 0) {
        char buf[CFG_CLAUDE_CODE_RX_BUF_SIZE];
        int len = _udp.read(buf, sizeof(buf) - 1);
        if (len > 0) {
            buf[len] = '\0';
            processPacket(buf, len);
        }
    }

    if (_taskActive) {
        unsigned long now = millis();
        if (now - _lastActivityMs > CFG_CLAUDE_CODE_TIMEOUT_MS) {
            if (_status != Status::SLEEPING) {
                LOG_WARN("ClaudeCode", "无活动超时，进入休眠");
                setStatus(Status::SLEEPING);
                _sleepStartMs = now;
            }
        }
        if (_status == Status::SLEEPING) {
            if (now - _sleepStartMs > CFG_CLAUDE_CODE_SLEEP_DURATION_MS) {
                LOG_INFO("ClaudeCode", "休眠结束，回到空闲");
                setStatus(Status::IDLE);
                _taskActive = false;
            }
        }
    }

    static unsigned long lastDiscovery = 0;
    if (millis() - lastDiscovery > 5000) {
        lastDiscovery = millis();
        sendDiscovery();
    }
}

void ClaudeCodeService::processPacket(const char* data, int len) {
    LOG_INFO("ClaudeCode", "收到: %s", data);

    if (strncmp(data, "CC:", 3) != 0) return;

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
        setStatus(mapEventToStatus(event));
        _lastActivityMs = millis();
        return;
    }

    // 解析 hook
    comma = strchr(p, ',');
    if (comma) {
        size_t l = comma - p;
        if (l < sizeof(hook)) { strncpy(hook, p, l); hook[l] = '\0'; }
        p = comma + 1;
    } else {
        strncpy(hook, p, sizeof(hook) - 1);
        setStatus(mapEventToStatus(event), hook);
        _lastActivityMs = millis();
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
        _lastActivityMs = millis();
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
    _lastActivityMs = millis();
    _taskActive = true;
    if (!_taskStartMs) _taskStartMs = millis();
}

ClaudeCodeService::Status ClaudeCodeService::mapEventToStatus(const char* event) {
    if (strcmp(event, "working") == 0)    return Status::WORKING;
    if (strcmp(event, "thinking") == 0)   return Status::THINKING;
    if (strcmp(event, "permission") == 0) return Status::PERMISSION;
    if (strcmp(event, "done") == 0)       return Status::DONE;
    if (strcmp(event, "error") == 0)      return Status::ERROR;
    if (strcmp(event, "sweeping") == 0)   return Status::SWEEPING;
    if (strcmp(event, "sleeping") == 0)   return Status::SLEEPING;
    if (strcmp(event, "idle") == 0)       return Status::IDLE;
    return Status::WORKING;
}

void ClaudeCodeService::setStatus(Status status, const char* hookName,
                                   const char* toolName, const char* detail,
                                   const char* model) {
    _status = status;
    if (hookName) strncpy(_hookName, hookName, CFG_CLAUDE_CODE_HOOK_MAX_LEN - 1);
    if (toolName) strncpy(_toolName, toolName, CFG_CLAUDE_CODE_TOOL_MAX_LEN - 1);
    if (detail)   strncpy(_detail, detail, CFG_CLAUDE_CODE_DETAIL_MAX_LEN - 1);
    if (model)    strncpy(_model, model, CFG_CLAUDE_CODE_MODEL_MAX_LEN - 1);
    LOG_INFO("ClaudeCode", "状态: %s hook=%s tool=%s", statusToText(status), _hookName, _toolName);
}

const char* ClaudeCodeService::statusToText(Status status) const {
    switch (status) {
        case Status::IDLE:       return "IDLE";
        case Status::THINKING:   return "THINKING";
        case Status::WORKING:    return "WORKING";
        case Status::ERROR:      return "ERROR";
        case Status::DONE:       return "DONE";
        case Status::PERMISSION: return "PERMISSION";
        case Status::SWEEPING:   return "SWEEPING";
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
    if (!_taskStartMs) return 0;
    return millis() - _taskStartMs;
}

bool ClaudeCodeService::isActive() const {
    return _status != Status::IDLE && _status != Status::SLEEPING;
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
    _lastActivityMs = millis();
    _taskActive = true;
    if (!_taskStartMs) _taskStartMs = millis();
    LOG_INFO("ClaudeCode", "串口注入: %s", statusToText(status));
}

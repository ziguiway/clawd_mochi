#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "../config/cfg_claude_code.h"
#include "../utils/state_machine.h"

class ClaudeCodeService {
public:
    enum class Status {
        IDLE, THINKING, WORKING, ERROR, DONE,
        PERMISSION, SWEEPING, SLEEPING
    };

    ClaudeCodeService(StateMachine* sm);
    void init();
    void update();

    Status getStatus() const;
    const char* getStatusText() const;
    const char* getHookName() const;
    const char* getToolName() const;
    const char* getDetail() const;
    const char* getModel() const;
    unsigned long getElapsedMs() const;
    bool isActive() const;
    String getStatusJson() const;

    void sendDiscovery();

    void injectStatus(Status status, const char* hookName = nullptr,
                      const char* toolName = nullptr, const char* detail = nullptr,
                      const char* model = nullptr);

private:
    WiFiUDP _udp;
    StateMachine* _stateMachine;
    Status _status;

    char _hookName[CFG_CLAUDE_CODE_HOOK_MAX_LEN];
    char _toolName[CFG_CLAUDE_CODE_TOOL_MAX_LEN];
    char _detail[CFG_CLAUDE_CODE_DETAIL_MAX_LEN];
    char _model[CFG_CLAUDE_CODE_MODEL_MAX_LEN];

    unsigned long _lastActivityMs;
    unsigned long _taskStartMs;
    bool _taskActive;
    unsigned long _sleepStartMs;

    void processPacket(const char* data, int len);
    void setStatus(Status status, const char* hookName = nullptr,
                   const char* toolName = nullptr, const char* detail = nullptr,
                   const char* model = nullptr);
    Status mapEventToStatus(const char* event);
    const char* statusToText(Status status) const;
};

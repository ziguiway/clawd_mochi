#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include "../config/cfg_claude_code.h"
#include "../utils/state_machine.h"

class TimeService;

class ClaudeCodeService {
public:
    enum class Status {
        IDLE, THINKING, WORKING, ERROR, DONE,
        PERMISSION, SWEEPING, SLEEPING
    };

    ClaudeCodeService(StateMachine* sm, TimeService* timeService = nullptr);
    void init();
    void update();

    // 是否已经可以开始运行(SERIAL 立即可以,LAN 需 WiFi 连上)
    bool canRun() const;

    Status getStatus() const;
    const char* getStatusText() const;
    const char* getHookName() const;
    const char* getToolName() const;
    const char* getDetail() const;
    const char* getModel() const;
    unsigned long getElapsedMs() const;
    bool isActive() const;
    String getStatusJson() const;

    // —— 会话/今日统计 ——
    // 今日 WORKING 累计毫秒(含当前进行中的段)
    uint32_t getTodayWorkingMs() const;
    // 当前会话 WORKING 累计毫秒(含当前进行中的段)
    uint32_t getSessionWorkingMs() const;
    // 最长一次连续 WORKING 毫秒
    uint32_t getLongestWorkingMs() const;
    uint16_t getDoneCount() const;
    uint16_t getErrorCount() const;
    uint16_t getPermissionCount() const;
    uint32_t getStatsDateKey() const;
    String getStatsJson() const;
    // 清零今日/会话/最长/计数(供 Web reset)
    void resetStats();

    void sendDiscovery();

    void processPacket(const char* data, int len);

    void injectStatus(Status status, const char* hookName = nullptr,
                      const char* toolName = nullptr,
                      const char* detail = nullptr,
                      const char* model = nullptr);

private:
    WiFiUDP _udp;
    StateMachine* _stateMachine;
    TimeService* _timeService;
    Status _status;

    char _hookName[CFG_CLAUDE_CODE_HOOK_MAX_LEN];
    char _toolName[CFG_CLAUDE_CODE_TOOL_MAX_LEN];
    char _detail[CFG_CLAUDE_CODE_DETAIL_MAX_LEN];
    char _model[CFG_CLAUDE_CODE_MODEL_MAX_LEN];

    unsigned long _taskStartMs;
    unsigned long _taskElapsedMs;
    bool _taskActive;
    unsigned long _sleepStartMs;
    unsigned long _lastStatusEventMs;
    bool _initialized;

    // —— 统计累积字段 ——
    Preferences _statsPrefs;
    uint32_t _statsDateKey;          // YYYYMMDD,0 表示未同步
    uint32_t _todayWorkingMs;
    uint32_t _sessionWorkingMs;
    uint32_t _longestWorkingMs;
    uint16_t _doneCount;
    uint16_t _errorCount;
    uint16_t _permissionCount;
    unsigned long _workingSegmentStartMs;  // 0 = 当前不在 WORKING
    bool _statsDirty;
    unsigned long _lastStatsPersistMs;
    bool _statsLoaded;

    void setStatus(Status status, const char* hookName = nullptr,
                   const char* toolName = nullptr,
                   const char* detail = nullptr,
                   const char* model = nullptr);
    void updateTaskClock(Status status);
    bool isActiveStatus(Status status) const;
    Status mapEventToStatus(const char* event);
    const char* statusToText(Status status) const;

    // 统计内部方法
    void updateStats(Status oldStatus, Status newStatus);
    void resetSessionStats();
    void checkDayRollover();
    void loadStats();
    void persistStats();
    uint32_t currentDateKey() const;
};

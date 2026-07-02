#pragma once

#include <Arduino.h>
#include "../hardware/tft_display.h"
#include "../service/claude_code_service.h"
#include "../config/cfg_display.h"

class ClaudeCodeView {
public:
    explicit ClaudeCodeView(TftDisplay* tft);

    void render(ClaudeCodeService::Status status, const char* hookName,
                const char* toolName, const char* detail,
                const char* model, unsigned long elapsedMs);

private:
    TftDisplay* _tft;

    void drawHeader(ClaudeCodeService::Status status);
    void drawHookInfo(const char* hookName, const char* toolName);
    void drawDetail(const char* detail);
    void drawFooter(const char* model, unsigned long elapsedMs);
    void drawStatusIcon(ClaudeCodeService::Status status, int x, int y);
    uint16_t getStatusColor(ClaudeCodeService::Status status);
    void formatElapsed(unsigned long ms, char* buf, size_t bufSize);
};

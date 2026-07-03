#pragma once

#include <Arduino.h>
#include "../hardware/tft_display.h"
#include "../service/claude_code_service.h"
#include "../config/cfg_display.h"

class ClaudeCodeView {
public:
    explicit ClaudeCodeView(TftDisplay* tft);

    void reset();
    void render(ClaudeCodeService::Status status, const char* hookName,
                const char* toolName, const char* detail,
                const char* model, unsigned long elapsedMs);

private:
    TftDisplay* _tft;
    bool _layoutDrawn;
    ClaudeCodeService::Status _lastStatus;
    char _lastHook[32];
    char _lastTool[32];
    char _lastDetail[96];
    char _lastModel[32];
    unsigned long _lastElapsedSec;

    void drawShell();
    void drawHeader(ClaudeCodeService::Status status);
    void drawStatusPanel(ClaudeCodeService::Status status);
    void drawHookInfo(const char* hookName, const char* toolName);
    void drawDetail(const char* detail);
    void drawFooter(const char* model, unsigned long elapsedSec);
    void drawStatusIcon(ClaudeCodeService::Status status, int x, int y);
    uint16_t getStatusColor(ClaudeCodeService::Status status);
    const char* getStatusVerb(ClaudeCodeService::Status status);
    const char* safeText(const char* text, const char* fallback);
    bool textChanged(const char* previous, const char* current);
    void rememberText(char* target, size_t targetSize, const char* source);
    void drawClippedText(int x, int y, int w, const char* text, uint16_t color,
                         uint16_t bgColor, uint8_t size);
    void drawWrappedText(int x, int y, int w, const char* text, uint16_t color,
                         uint16_t bgColor);
    void formatElapsed(unsigned long sec, char* buf, size_t bufSize);
};

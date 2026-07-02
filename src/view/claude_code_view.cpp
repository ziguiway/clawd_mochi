#include "claude_code_view.h"

static const char* statusToText(ClaudeCodeService::Status status) {
    switch (status) {
        case ClaudeCodeService::Status::IDLE:       return "IDLE";
        case ClaudeCodeService::Status::THINKING:   return "THINKING";
        case ClaudeCodeService::Status::WORKING:    return "WORKING";
        case ClaudeCodeService::Status::ERROR:      return "ERROR";
        case ClaudeCodeService::Status::DONE:       return "DONE";
        case ClaudeCodeService::Status::PERMISSION: return "PERMISSION";
        case ClaudeCodeService::Status::SWEEPING:   return "SWEEPING";
        case ClaudeCodeService::Status::SLEEPING:   return "SLEEPING";
        default:                                    return "UNKNOWN";
    }
}

ClaudeCodeView::ClaudeCodeView(TftDisplay* tft) : _tft(tft) {}

void ClaudeCodeView::render(ClaudeCodeService::Status status, const char* hookName,
                             const char* toolName, const char* detail,
                             const char* model, unsigned long elapsedMs) {
    uint16_t statusColor = getStatusColor(status);

    drawHeader(status);
    _tft->fillRect(10, 42, 220, 2, statusColor);
    drawHookInfo(hookName, toolName);
    _tft->fillRect(10, 100, 220, 2, COLOR_GRAY);
    drawDetail(detail);
    _tft->fillRect(10, 155, 220, 2, COLOR_GRAY);
    drawFooter(model, elapsedMs);
}

void ClaudeCodeView::drawHeader(ClaudeCodeService::Status status) {
    uint16_t color = getStatusColor(status);
    _tft->fillRect(0, 0, 240, 40, COLOR_DARKBG);
    drawStatusIcon(status, 15, 8);
    _tft->drawText(40, 8, "Claude Code", COLOR_WHITE, COLOR_DARKBG, FONT_MEDIUM);
    _tft->drawText(40, 24, statusToText(status), color, COLOR_DARKBG, FONT_SMALL);
}

void ClaudeCodeView::drawHookInfo(const char* hookName, const char* toolName) {
    _tft->fillRect(0, 45, 240, 55, COLOR_DARKBG);
    _tft->drawText(15, 50, "Hook:", COLOR_GRAY, COLOR_DARKBG, FONT_SMALL);
    _tft->drawText(60, 50, hookName, COLOR_WHITE, COLOR_DARKBG, FONT_SMALL);
    _tft->drawText(15, 70, "Tool:", COLOR_GRAY, COLOR_DARKBG, FONT_SMALL);
    _tft->drawText(60, 70, toolName, COLOR_ORANGE, COLOR_DARKBG, FONT_SMALL);
}

void ClaudeCodeView::drawDetail(const char* detail) {
    _tft->fillRect(0, 103, 240, 52, COLOR_DARKBG);
    _tft->drawText(15, 108, "Detail:", COLOR_GRAY, COLOR_DARKBG, FONT_SMALL);

    char truncated[33];
    strncpy(truncated, detail, 32);
    truncated[32] = '\0';
    if (strlen(detail) > 32) { truncated[29] = '.'; truncated[30] = '.'; truncated[31] = '.'; }
    _tft->drawText(15, 125, truncated, COLOR_WHITE, COLOR_DARKBG, FONT_SMALL);
}

void ClaudeCodeView::drawFooter(const char* model, unsigned long elapsedMs) {
    _tft->fillRect(0, 158, 240, 82, COLOR_DARKBG);
    _tft->drawText(15, 165, "Model:", COLOR_GRAY, COLOR_DARKBG, FONT_SMALL);
    _tft->drawText(65, 165, model, COLOR_BLUE, COLOR_DARKBG, FONT_SMALL);

    char elapsedStr[16];
    formatElapsed(elapsedMs, elapsedStr, sizeof(elapsedStr));
    _tft->drawText(15, 185, "Time:", COLOR_GRAY, COLOR_DARKBG, FONT_SMALL);
    _tft->drawText(55, 185, elapsedStr, COLOR_GREEN, COLOR_DARKBG, FONT_SMALL);
}

void ClaudeCodeView::drawStatusIcon(ClaudeCodeService::Status status, int x, int y) {
    uint16_t color = getStatusColor(status);
    switch (status) {
        case ClaudeCodeService::Status::IDLE:
        case ClaudeCodeService::Status::SLEEPING:
            _tft->fillCircle(x + 10, y + 10, 10, color);
            break;
        case ClaudeCodeService::Status::THINKING:
            _tft->drawCircle(x + 10, y + 10, 10, color);
            _tft->fillCircle(x + 10, y + 10, 6, color);
            break;
        case ClaudeCodeService::Status::WORKING:
            _tft->fillRect(x + 4, y + 4, 12, 12, color);
            break;
        case ClaudeCodeService::Status::ERROR:
            _tft->drawLine(x + 3, y + 3, x + 17, y + 17, color);
            _tft->drawLine(x + 17, y + 3, x + 3, y + 17, color);
            break;
        case ClaudeCodeService::Status::DONE:
            _tft->drawLine(x + 3, y + 10, x + 8, y + 15, color);
            _tft->drawLine(x + 8, y + 15, x + 17, y + 5, color);
            break;
        case ClaudeCodeService::Status::PERMISSION:
            _tft->fillCircle(x + 10, y + 10, 10, color);
            _tft->drawText(x + 5, y + 3, "?", COLOR_DARKBG, color, FONT_MEDIUM);
            break;
        case ClaudeCodeService::Status::SWEEPING:
            for (int i = 0; i < 3; i++) _tft->drawCircle(x + 5 + i * 5, y + 10, 4, color);
            break;
    }
}

uint16_t ClaudeCodeView::getStatusColor(ClaudeCodeService::Status status) {
    switch (status) {
        case ClaudeCodeService::Status::IDLE:
        case ClaudeCodeService::Status::SLEEPING:   return COLOR_ORANGE;
        case ClaudeCodeService::Status::THINKING:   return COLOR_BLUE;
        case ClaudeCodeService::Status::WORKING:    return COLOR_GREEN;
        case ClaudeCodeService::Status::ERROR:      return COLOR_RED;
        case ClaudeCodeService::Status::DONE:       return COLOR_GREEN;
        case ClaudeCodeService::Status::PERMISSION: return COLOR_YELLOW;
        case ClaudeCodeService::Status::SWEEPING:   return COLOR_ORANGE;
        default:                                    return COLOR_WHITE;
    }
}

void ClaudeCodeView::formatElapsed(unsigned long ms, char* buf, size_t bufSize) {
    unsigned long sec = ms / 1000;
    unsigned long min = sec / 60;
    sec = sec % 60;
    if (min > 0) snprintf(buf, bufSize, "%lu:%02lu", min, sec);
    else snprintf(buf, bufSize, "%lus", sec);
}

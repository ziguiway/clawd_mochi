#include "claude_code_view.h"

static constexpr uint16_t COLOR_MOCHI_BG = 0x18E4;
static constexpr uint16_t COLOR_CARD = 0x2125;
static constexpr uint16_t COLOR_CARD_DARK = 0x0841;
static constexpr uint16_t COLOR_CARD_LINE = 0x39E7;
static constexpr uint16_t COLOR_TEXT_SOFT = 0xBDF7;
static constexpr uint16_t COLOR_CYAN = 0x05FF;

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

ClaudeCodeView::ClaudeCodeView(TftDisplay* tft)
    : _tft(tft)
    , _layoutDrawn(false)
    , _lastStatus(ClaudeCodeService::Status::IDLE)
    , _lastElapsedSec(0)
{
    _lastHook[0] = '\0';
    _lastTool[0] = '\0';
    _lastDetail[0] = '\0';
    _lastModel[0] = '\0';
}

void ClaudeCodeView::reset() {
    _layoutDrawn = false;
    _lastHook[0] = '\0';
    _lastTool[0] = '\0';
    _lastDetail[0] = '\0';
    _lastModel[0] = '\0';
    _lastElapsedSec = 0xFFFFFFFF;
}

void ClaudeCodeView::render(ClaudeCodeService::Status status, const char* hookName,
                             const char* toolName, const char* detail,
                             const char* model, unsigned long elapsedMs) {
    hookName = safeText(hookName, "waiting");
    toolName = safeText(toolName, "-");
    detail = safeText(detail, "No active Claude Code event");
    model = safeText(model, "-");

    unsigned long elapsedSec = elapsedMs / 1000;

    if (!_layoutDrawn) {
        drawShell();
        _layoutDrawn = true;
        _lastStatus = status;
        drawHeader(status);
        drawStatusPanel(status);
        drawHookInfo(hookName, toolName);
        drawDetail(detail);
        drawFooter(model, elapsedSec);
        rememberText(_lastHook, sizeof(_lastHook), hookName);
        rememberText(_lastTool, sizeof(_lastTool), toolName);
        rememberText(_lastDetail, sizeof(_lastDetail), detail);
        rememberText(_lastModel, sizeof(_lastModel), model);
        _lastElapsedSec = elapsedSec;
        return;
    }

    if (status != _lastStatus) {
        _lastStatus = status;
        drawHeader(status);
        drawStatusPanel(status);
    }

    if (textChanged(_lastHook, hookName) || textChanged(_lastTool, toolName)) {
        drawHookInfo(hookName, toolName);
        rememberText(_lastHook, sizeof(_lastHook), hookName);
        rememberText(_lastTool, sizeof(_lastTool), toolName);
    }

    if (textChanged(_lastDetail, detail)) {
        drawDetail(detail);
        rememberText(_lastDetail, sizeof(_lastDetail), detail);
    }

    if (textChanged(_lastModel, model) || elapsedSec != _lastElapsedSec) {
        drawFooter(model, elapsedSec);
        rememberText(_lastModel, sizeof(_lastModel), model);
        _lastElapsedSec = elapsedSec;
    }
}

void ClaudeCodeView::drawShell() {
    _tft->fillScreen(COLOR_MOCHI_BG);

    _tft->fillRect(0, 0, 240, 38, COLOR_ORANGE);
    _tft->fillRect(0, 38, 240, 3, COLOR_BLACK);
    _tft->drawText(14, 10, "CLAWD", COLOR_WHITE, COLOR_ORANGE, FONT_MEDIUM);
    _tft->drawText(86, 16, "MOCHI", COLOR_DARKBG, COLOR_ORANGE, FONT_SMALL);

    _tft->fillRect(170, 12, 12, 12, COLOR_DARKBG);
    _tft->fillRect(196, 12, 12, 12, COLOR_DARKBG);
    _tft->drawLine(168, 30, 184, 24, COLOR_WHITE);
    _tft->drawLine(210, 30, 194, 24, COLOR_WHITE);

    _tft->drawRoundRect(8, 46, 224, 66, 8, COLOR_CARD_LINE);
    _tft->fillRoundRect(10, 48, 220, 62, 7, COLOR_CARD);

    _tft->drawRoundRect(8, 120, 224, 72, 8, COLOR_CARD_LINE);
    _tft->fillRoundRect(10, 122, 220, 68, 7, COLOR_CARD_DARK);
    _tft->fillRect(10, 122, 220, 17, COLOR_BLACK);
    _tft->drawText(18, 127, "clawd:~$", COLOR_TERM_GREEN, COLOR_BLACK, FONT_SMALL);

    _tft->drawRoundRect(8, 202, 224, 30, 8, COLOR_CARD_LINE);
    _tft->fillRoundRect(10, 204, 220, 26, 7, COLOR_CARD);
}

void ClaudeCodeView::drawHeader(ClaudeCodeService::Status status) {
    uint16_t color = getStatusColor(status);
    _tft->fillRect(12, 41, 216, 3, color);
    _tft->fillRoundRect(178, 8, 48, 22, 8, COLOR_DARKBG);
    _tft->drawText(190, 15, "CC", color, COLOR_DARKBG, FONT_SMALL);
}

void ClaudeCodeView::drawStatusPanel(ClaudeCodeService::Status status) {
    uint16_t color = getStatusColor(status);
    _tft->fillRoundRect(10, 48, 220, 62, 7, COLOR_CARD);
    drawStatusIcon(status, 21, 62);

    const char* label = statusToText(status);
    uint8_t labelSize = strlen(label) > 8 ? FONT_SMALL : FONT_MEDIUM;
    _tft->drawText(62, 59, label, color, COLOR_CARD, labelSize);
    drawClippedText(62, 87, 156, getStatusVerb(status), COLOR_TEXT_SOFT, COLOR_CARD, FONT_SMALL);

    _tft->fillRect(16, 104, 208, 3, color);
}

void ClaudeCodeView::drawHookInfo(const char* hookName, const char* toolName) {
    _tft->fillRect(18, 145, 204, 15, COLOR_CARD_DARK);

    _tft->drawText(18, 148, "hook", COLOR_GRAY, COLOR_CARD_DARK, FONT_SMALL);
    drawClippedText(50, 148, 70, hookName, COLOR_WHITE, COLOR_CARD_DARK, FONT_SMALL);
    _tft->drawText(126, 148, "tool", COLOR_GRAY, COLOR_CARD_DARK, FONT_SMALL);
    drawClippedText(158, 148, 62, toolName, COLOR_ORANGE, COLOR_CARD_DARK, FONT_SMALL);
}

void ClaudeCodeView::drawDetail(const char* detail) {
    _tft->fillRect(18, 164, 204, 18, COLOR_CARD_DARK);
    _tft->drawText(18, 166, ">", COLOR_TERM_GREEN, COLOR_CARD_DARK, FONT_SMALL);
    drawClippedText(30, 166, 190, detail, COLOR_WHITE, COLOR_CARD_DARK, FONT_SMALL);
}

void ClaudeCodeView::drawFooter(const char* model, unsigned long elapsedSec) {
    _tft->fillRect(18, 212, 204, 12, COLOR_CARD);
    _tft->drawText(20, 214, "model", COLOR_GRAY, COLOR_CARD, FONT_SMALL);
    drawClippedText(56, 214, 104, model, COLOR_CYAN, COLOR_CARD, FONT_SMALL);

    char elapsedStr[16];
    formatElapsed(elapsedSec, elapsedStr, sizeof(elapsedStr));
    _tft->drawText(172, 214, elapsedStr, COLOR_GREEN, COLOR_CARD, FONT_SMALL);
}

void ClaudeCodeView::drawStatusIcon(ClaudeCodeService::Status status, int x, int y) {
    uint16_t color = getStatusColor(status);
    _tft->fillRoundRect(x, y, 30, 30, 5, COLOR_MOCHI_BG);
    _tft->drawRoundRect(x, y, 30, 30, 5, color);
    switch (status) {
        case ClaudeCodeService::Status::IDLE:
        case ClaudeCodeService::Status::SLEEPING:
            _tft->fillRect(x + 8, y + 12, 6, 6, color);
            _tft->fillRect(x + 17, y + 12, 6, 6, color);
            break;
        case ClaudeCodeService::Status::THINKING:
            _tft->fillCircle(x + 9, y + 15, 3, color);
            _tft->fillCircle(x + 15, y + 15, 3, color);
            _tft->fillCircle(x + 21, y + 15, 3, color);
            break;
        case ClaudeCodeService::Status::WORKING:
            _tft->drawLine(x + 8, y + 8, x + 14, y + 15, color);
            _tft->drawLine(x + 14, y + 15, x + 8, y + 22, color);
            _tft->drawLine(x + 22, y + 8, x + 16, y + 15, color);
            _tft->drawLine(x + 16, y + 15, x + 22, y + 22, color);
            break;
        case ClaudeCodeService::Status::ERROR:
            _tft->drawLine(x + 8, y + 8, x + 22, y + 22, color);
            _tft->drawLine(x + 22, y + 8, x + 8, y + 22, color);
            break;
        case ClaudeCodeService::Status::DONE:
            _tft->drawLine(x + 7, y + 16, x + 13, y + 22, color);
            _tft->drawLine(x + 13, y + 22, x + 23, y + 8, color);
            break;
        case ClaudeCodeService::Status::PERMISSION:
            _tft->drawText(x + 12, y + 10, "?", color, COLOR_MOCHI_BG, FONT_MEDIUM);
            break;
        case ClaudeCodeService::Status::SWEEPING:
            for (int i = 0; i < 3; i++) _tft->drawLine(x + 7, y + 9 + i * 5, x + 23, y + 9 + i * 5, color);
            break;
    }
}

uint16_t ClaudeCodeView::getStatusColor(ClaudeCodeService::Status status) {
    switch (status) {
        case ClaudeCodeService::Status::IDLE:
        case ClaudeCodeService::Status::SLEEPING:   return COLOR_ORANGE;
        case ClaudeCodeService::Status::THINKING:   return COLOR_CYAN;
        case ClaudeCodeService::Status::WORKING:    return COLOR_GREEN;
        case ClaudeCodeService::Status::ERROR:      return COLOR_RED;
        case ClaudeCodeService::Status::DONE:       return COLOR_GREEN;
        case ClaudeCodeService::Status::PERMISSION: return COLOR_YELLOW;
        case ClaudeCodeService::Status::SWEEPING:   return COLOR_ORANGE;
        default:                                    return COLOR_WHITE;
    }
}

const char* ClaudeCodeView::getStatusVerb(ClaudeCodeService::Status status) {
    switch (status) {
        case ClaudeCodeService::Status::IDLE:       return "Ready for the next prompt";
        case ClaudeCodeService::Status::THINKING:   return "Reasoning in progress";
        case ClaudeCodeService::Status::WORKING:    return "Running tool activity";
        case ClaudeCodeService::Status::ERROR:      return "Needs attention";
        case ClaudeCodeService::Status::DONE:       return "Task completed";
        case ClaudeCodeService::Status::PERMISSION: return "Waiting for permission";
        case ClaudeCodeService::Status::SWEEPING:   return "Cleaning workspace state";
        case ClaudeCodeService::Status::SLEEPING:   return "Standing by";
        default:                                    return "Watching Claude Code";
    }
}

const char* ClaudeCodeView::safeText(const char* text, const char* fallback) {
    return (text && text[0] != '\0') ? text : fallback;
}

bool ClaudeCodeView::textChanged(const char* previous, const char* current) {
    return strcmp(previous, safeText(current, "")) != 0;
}

void ClaudeCodeView::rememberText(char* target, size_t targetSize, const char* source) {
    strncpy(target, safeText(source, ""), targetSize - 1);
    target[targetSize - 1] = '\0';
}

void ClaudeCodeView::drawClippedText(int x, int y, int w, const char* text, uint16_t color,
                                     uint16_t bgColor, uint8_t size) {
    const int maxChars = max(1, w / (6 * size));
    char clipped[40];
    strncpy(clipped, safeText(text, "-"), sizeof(clipped) - 1);
    clipped[sizeof(clipped) - 1] = '\0';
    int len = strlen(clipped);
    if (len > maxChars) {
        int cut = min(maxChars, (int)sizeof(clipped) - 1);
        if (cut > 3) {
            clipped[cut - 3] = '.';
            clipped[cut - 2] = '.';
            clipped[cut - 1] = '.';
        }
        clipped[cut] = '\0';
    }
    _tft->drawText(x, y, clipped, color, bgColor, size);
}

void ClaudeCodeView::drawWrappedText(int x, int y, int w, const char* text, uint16_t color,
                                     uint16_t bgColor) {
    const int maxChars = max(1, w / 6);
    char line[36];
    const char* src = safeText(text, "-");
    for (int row = 0; row < 2; row++) {
        int i = 0;
        while (src[i] && i < maxChars && i < (int)sizeof(line) - 1) {
            line[i] = src[i];
            i++;
        }
        line[i] = '\0';
        if (row == 1 && src[i]) {
            int len = strlen(line);
            if (len > 3) {
                line[len - 3] = '.';
                line[len - 2] = '.';
                line[len - 1] = '.';
            }
        }
        _tft->drawText(x, y + row * 12, line, color, bgColor, FONT_SMALL);
        src += i;
        while (*src == ' ') src++;
        if (!*src) break;
    }
}

void ClaudeCodeView::formatElapsed(unsigned long sec, char* buf, size_t bufSize) {
    unsigned long min = sec / 60;
    sec = sec % 60;
    if (min > 0) snprintf(buf, bufSize, "%lu:%02lu", min, sec);
    else snprintf(buf, bufSize, "%lus", sec);
}

#include "claude_code_view.h"

static constexpr uint16_t COLOR_MOCHI_BG = COLOR_ORANGE;
static constexpr uint16_t COLOR_CAPTION = COLOR_DARKBG;
static constexpr uint16_t COLOR_CAPTION_LINE = COLOR_ORANGE;
static constexpr uint16_t COLOR_CAPTION_TEXT = COLOR_WHITE;
static constexpr uint16_t COLOR_TEXT_SOFT = 0xBDF7;
static constexpr uint16_t COLOR_CYAN = 0x05FF;

static constexpr int16_t FACE_EYE_W = 30;
static constexpr int16_t FACE_EYE_H = 60;
static constexpr int16_t FACE_EYE_GAP = 120;
static constexpr int16_t FACE_EYE_X = (CFG_DISPLAY_WIDTH - (FACE_EYE_W * 2 + FACE_EYE_GAP)) / 2;
static constexpr int16_t FACE_EYE_Y = (CFG_DISPLAY_HEIGHT - FACE_EYE_H) / 2 - 40;
static constexpr int16_t CAPTION_X = 0;
static constexpr int16_t CAPTION_Y = 202;
static constexpr int16_t CAPTION_W = CFG_DISPLAY_WIDTH;
static constexpr int16_t CAPTION_H = 38;

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
        drawDetail(model);
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
        drawHookInfo(hookName, toolName);
        drawDetail(model);
    }

    if (textChanged(_lastHook, hookName) || textChanged(_lastTool, toolName)) {
        drawHookInfo(hookName, toolName);
        rememberText(_lastHook, sizeof(_lastHook), hookName);
        rememberText(_lastTool, sizeof(_lastTool), toolName);
    }

    if (textChanged(_lastDetail, detail) || textChanged(_lastModel, model)) {
        drawDetail(model);
        rememberText(_lastDetail, sizeof(_lastDetail), detail);
        rememberText(_lastModel, sizeof(_lastModel), model);
    }

    if (textChanged(_lastModel, model) || elapsedSec != _lastElapsedSec) {
        drawFooter(model, elapsedSec);
        _lastElapsedSec = elapsedSec;
    }
}

void ClaudeCodeView::drawShell() {
    _tft->fillScreen(COLOR_MOCHI_BG);
    _tft->fillRect(0, CAPTION_Y - 4, CFG_DISPLAY_WIDTH, 4, COLOR_BLACK);
    _tft->fillRect(CAPTION_X, CAPTION_Y, CAPTION_W, CAPTION_H, COLOR_CAPTION);
    _tft->fillRect(0, CAPTION_Y, CFG_DISPLAY_WIDTH, 2, COLOR_CAPTION_LINE);
}

void ClaudeCodeView::drawHeader(ClaudeCodeService::Status status) {
    (void)status;
}

void ClaudeCodeView::drawStatusPanel(ClaudeCodeService::Status status) {
    _tft->fillRect(0, 24, CFG_DISPLAY_WIDTH, 172, COLOR_MOCHI_BG);
    drawStatusIcon(status, 0, 0);

    if (status == ClaudeCodeService::Status::WORKING) {
        _tft->fillRect(FACE_EYE_X - 10, FACE_EYE_Y + FACE_EYE_H + 12,
                       FACE_EYE_GAP + FACE_EYE_W * 2 + 20, 3, COLOR_BLACK);
    }
}

void ClaudeCodeView::drawHookInfo(const char* hookName, const char* toolName) {
    (void)hookName;
    _tft->fillRect(8, CAPTION_Y + 6, 224, 11, COLOR_CAPTION);

    _tft->drawText(10, CAPTION_Y + 8, "STATUS", COLOR_ORANGE, COLOR_CAPTION, FONT_SMALL);
    drawClippedText(58, CAPTION_Y + 8, 96, statusToText(_lastStatus),
                    COLOR_CAPTION_TEXT, COLOR_CAPTION, FONT_SMALL);
    _tft->drawText(164, CAPTION_Y + 8, "TOOL", COLOR_ORANGE, COLOR_CAPTION, FONT_SMALL);
    drawClippedText(194, CAPTION_Y + 8, 38, toolName, COLOR_CAPTION_TEXT, COLOR_CAPTION, FONT_SMALL);
}

void ClaudeCodeView::drawDetail(const char* model) {
    _tft->fillRect(8, CAPTION_Y + 22, 152, 10, COLOR_CAPTION);
    _tft->drawText(10, CAPTION_Y + 23, "MODEL", COLOR_ORANGE, COLOR_CAPTION, FONT_SMALL);
    drawClippedText(58, CAPTION_Y + 23, 102, model, COLOR_TEXT_SOFT, COLOR_CAPTION, FONT_SMALL);
}

void ClaudeCodeView::drawFooter(const char* model, unsigned long elapsedSec) {
    (void)model;
    _tft->fillRect(164, CAPTION_Y + 22, 68, 11, COLOR_CAPTION);

    char elapsedStr[16];
    formatElapsed(elapsedSec, elapsedStr, sizeof(elapsedStr));
    _tft->drawText(164, CAPTION_Y + 23, "TIME", COLOR_ORANGE, COLOR_CAPTION, FONT_SMALL);
    _tft->drawText(194, CAPTION_Y + 23, elapsedStr, COLOR_CAPTION_TEXT, COLOR_CAPTION, FONT_SMALL);
}

void ClaudeCodeView::drawStatusIcon(ClaudeCodeService::Status status, int x, int y) {
    (void)x;
    (void)y;
    switch (status) {
        case ClaudeCodeService::Status::IDLE:
        case ClaudeCodeService::Status::SLEEPING:
        case ClaudeCodeService::Status::THINKING:
        case ClaudeCodeService::Status::WORKING:
        case ClaudeCodeService::Status::ERROR:
        case ClaudeCodeService::Status::PERMISSION:
            _tft->fillRect(FACE_EYE_X, FACE_EYE_Y, FACE_EYE_W, FACE_EYE_H, COLOR_BLACK);
            _tft->fillRect(FACE_EYE_X + FACE_EYE_W + FACE_EYE_GAP, FACE_EYE_Y,
                           FACE_EYE_W, FACE_EYE_H, COLOR_BLACK);
            break;
        case ClaudeCodeService::Status::DONE:
            for (int8_t t = -5; t <= 5; t++) {
                int16_t cy = FACE_EYE_Y + FACE_EYE_H / 2;
                int16_t lcx = FACE_EYE_X + FACE_EYE_W / 2;
                int16_t rcx = FACE_EYE_X + FACE_EYE_W + FACE_EYE_GAP + FACE_EYE_W / 2;
                _tft->drawLine(lcx - FACE_EYE_W / 2, cy - FACE_EYE_H / 2 + t,
                               lcx + FACE_EYE_W / 2, cy + t, COLOR_BLACK);
                _tft->drawLine(lcx + FACE_EYE_W / 2, cy + t,
                               lcx - FACE_EYE_W / 2, cy + FACE_EYE_H / 2 + t, COLOR_BLACK);
                _tft->drawLine(rcx + FACE_EYE_W / 2, cy - FACE_EYE_H / 2 + t,
                               rcx - FACE_EYE_W / 2, cy + t, COLOR_BLACK);
                _tft->drawLine(rcx - FACE_EYE_W / 2, cy + t,
                               rcx + FACE_EYE_W / 2, cy + FACE_EYE_H / 2 + t, COLOR_BLACK);
            }
            break;
        case ClaudeCodeService::Status::SWEEPING:
            _tft->fillRect(FACE_EYE_X, FACE_EYE_Y + FACE_EYE_H / 2 - 3,
                           FACE_EYE_W, 6, COLOR_BLACK);
            _tft->fillRect(FACE_EYE_X + FACE_EYE_W + FACE_EYE_GAP,
                           FACE_EYE_Y + FACE_EYE_H / 2 - 3, FACE_EYE_W, 6, COLOR_BLACK);
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

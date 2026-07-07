#include "display_service.h"
#include "../config/cfg_display.h"
#include "../config/app_config.h"
#include "../view/boot_animation.h"
#include <qrcode.h>

#define EYE_W   30
#define EYE_H   60
#define EYE_GAP 120
#define EYE_OX  0
#define EYE_OY  40

DisplayService::DisplayService(TftDisplay* tft, ClaudeCodeService* ccService,
                               WifiConfigService* wifiService, TimeService* timeService)
    : _tft(tft), _ccService(ccService), _wifiService(wifiService), _timeService(timeService)
    , _ccView(tft), _eyesView(tft)
    , _currentMode(DisplayMode::SETUP), _lastRefreshMs(0)
    , _interactiveView(InteractiveView::EYES_NORMAL), _interactiveActive(false)
    , _busy(false), _animSpeed(1)
    , _animBgColor(COLOR_ORANGE), _drawBgColor(COLOR_ORANGE)
    , _brightnessPercent(100)
    , _focusMinutes(25), _breakMinutes(5), _pomodoroPhase(PomodoroPhase::FOCUS)
    , _pomodoroRunning(false), _pomodoroPaused(false)
    , _pomodoroDurationSec(25UL * 60UL), _pomodoroRemainingAtPauseSec(25UL * 60UL)
    , _pomodoroStartedMs(0), _lastClockRenderSec(0)
    , _timeViewDirty(true), _timeViewLayoutDrawn(false)
    , _lastTimeText{0}, _lastSubText{0}, _lastHintText{0}
    , _lastProgressPermille(0xFFFF), _lastLightProgress(false)
    , _termMode(false), _termRow(0), _termCol(0)
{
}

void DisplayService::init() {
    _currentMode = DisplayMode::SETUP;
    _animBgColor = COLOR_ORANGE;
    _drawBgColor = COLOR_ORANGE;
    setBrightnessPercent(_brightnessPercent);
}

// ── Eye geometry ───────────────────────────────────────────────
int16_t DisplayService::eyeLX(int16_t ox) {
    return (CFG_DISPLAY_WIDTH - (EYE_W * 2 + EYE_GAP)) / 2 + EYE_OX + ox;
}
int16_t DisplayService::eyeRX(int16_t ox) { return eyeLX(ox) + EYE_W + EYE_GAP; }
int16_t DisplayService::eyeY()            { return (CFG_DISPLAY_HEIGHT - EYE_H) / 2 - EYE_OY; }
int16_t DisplayService::eyeCY()           { return eyeY() + EYE_H / 2; }

int DisplayService::speedMs(int ms) {
    if (_animSpeed == 3) return ms / 2;
    if (_animSpeed == 1) return ms * 2;
    return ms;
}

uint16_t DisplayService::hexToRgb565(const String& hex) {
    String h = hex;
    h.replace("#", "");
    if (h.length() != 6) return COLOR_WHITE;
    long v = strtol(h.c_str(), nullptr, 16);
    return _tft->getTft().color565((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
}

// ── Drawing helpers ────────────────────────────────────────────
void DisplayService::drawNormalEyes(int16_t ox, bool blink) {
    _tft->fillScreen(_animBgColor);
    const int16_t lx = eyeLX(ox), rx = eyeRX(ox), ey = eyeY();
    if (!blink) {
        _tft->fillRect(lx, ey, EYE_W, EYE_H, COLOR_BLACK);
        _tft->fillRect(rx, ey, EYE_W, EYE_H, COLOR_BLACK);
    } else {
        _tft->fillRect(lx, ey + EYE_H / 2 - 3, EYE_W, 6, COLOR_BLACK);
        _tft->fillRect(rx, ey + EYE_H / 2 - 3, EYE_W, 6, COLOR_BLACK);
    }
}

void DisplayService::drawChevron(int16_t cx, int16_t cy, int16_t arm, int16_t reach,
                                  uint8_t thk, bool rightFacing, uint16_t col) {
    for (int8_t t = -(int8_t)thk; t <= (int8_t)thk; t++) {
        if (rightFacing) {
            _tft->drawLine(cx - reach/2, cy - arm + t, cx + reach/2, cy + t,      col);
            _tft->drawLine(cx + reach/2, cy + t,       cx - reach/2, cy + arm + t, col);
        } else {
            _tft->drawLine(cx + reach/2, cy - arm + t, cx - reach/2, cy + t,      col);
            _tft->drawLine(cx - reach/2, cy + t,       cx + reach/2, cy + arm + t, col);
        }
    }
}

void DisplayService::drawSquishEyes(bool closed) {
    _tft->fillScreen(_animBgColor);
    const int16_t lx = eyeLX(0), rx = eyeRX(0), cy = eyeCY();
    const int16_t arm   = EYE_H / 2;
    const int16_t reach = EYE_W / 2;
    const int16_t lcx   = lx + EYE_W / 2;
    const int16_t rcx   = rx + EYE_W / 2;
    if (!closed) {
        drawChevron(lcx, cy, arm, reach, 10, true,  COLOR_BLACK);
        drawChevron(rcx, cy, arm, reach, 10, false, COLOR_BLACK);
    } else {
        _tft->fillRect(lx, cy - 5, EYE_W, 10, COLOR_BLACK);
        _tft->fillRect(rx, cy - 5, EYE_W, 10, COLOR_BLACK);
    }
}

void DisplayService::drawCodeView() {
    _termMode = false;
    _tft->fillScreen(COLOR_DARKBG);
    _tft->fillRect(0, 0, CFG_DISPLAY_WIDTH, 4, COLOR_ORANGE);
    _tft->fillRect(0, CFG_DISPLAY_HEIGHT - 4, CFG_DISPLAY_WIDTH, 4, COLOR_ORANGE);
    _tft->getTft().setTextColor(COLOR_ORANGE);
    _tft->getTft().setTextSize(4);
    _tft->getTft().setCursor((CFG_DISPLAY_WIDTH - 144) / 2, CFG_DISPLAY_HEIGHT / 2 - 52);
    _tft->getTft().print("Claude");
    _tft->getTft().setTextColor(COLOR_WHITE);
    _tft->getTft().setCursor((CFG_DISPLAY_WIDTH - 96) / 2, CFG_DISPLAY_HEIGHT / 2 + 8);
    _tft->getTft().print("Code");
    _tft->fillRect((CFG_DISPLAY_WIDTH - 96) / 2, CFG_DISPLAY_HEIGHT / 2 + 52, 96, 3, COLOR_ORANGE);
}

void DisplayService::invalidateTimeView() {
    _timeViewDirty = true;
    _lastTimeText[0] = '\0';
    _lastSubText[0] = '\0';
    _lastHintText[0] = '\0';
    _lastProgressPermille = 0xFFFF;
    _timeViewLayoutDrawn = false;
}

void DisplayService::renderTimeScreenLayout(const char* mark, const char* modeText) {
    const int16_t captionY = 214;
    const int16_t barX = 20;
    const int16_t barY = 184;
    const int16_t barW = 200;
    const int16_t barH = 12;

    _tft->fillScreen(COLOR_ORANGE);

    // 顶部 mark + 下划线
    _tft->getTft().setTextColor(COLOR_BLACK);
    _tft->getTft().setTextSize(1);
    _tft->getTft().setCursor(14, 14);
    _tft->getTft().print(mark);
    _tft->fillRect(14, 31, 42, 4, COLOR_BLACK);

    // 进度条外框(静态)
    _tft->drawRect(barX, barY, barW, barH, COLOR_BLACK);

    // 底部黑色信息条 + 静态 mode 文字
    _tft->fillRect(0, captionY, CFG_DISPLAY_WIDTH, CFG_DISPLAY_HEIGHT - captionY, COLOR_BLACK);
    _tft->getTft().setTextSize(1);
    _tft->getTft().setTextColor(COLOR_WHITE);
    _tft->getTft().setCursor(10, captionY + 8);
    _tft->getTft().print(modeText);

    _timeViewLayoutDrawn = true;
}

void DisplayService::renderTimeScreenDynamic(const char* timeText, const char* subText,
                                             const char* hintText,
                                             uint16_t progressPermille, bool lightProgress) {
    progressPermille = constrain(progressPermille, 0, 1000);
    const int16_t captionY = 214;
    const int16_t barX = 20;
    const int16_t barY = 184;
    const int16_t barW = 200;
    const int16_t barH = 12;

    // 动态区域 Y 范围:大约 60 ~ 180(覆盖时间文字、子文字、进度条)
    // 局部擦除只针对动态区域,避免全屏闪烁
    const int16_t dynTop = 60;
    const int16_t dynBottom = 180;
    const uint16_t bgFill = lightProgress ? COLOR_BLACK : COLOR_ORANGE;

    const bool timeChanged = strcmp(timeText, _lastTimeText) != 0;
    const bool subChanged = strcmp(subText, _lastSubText) != 0;
    const bool hintChanged = strcmp(hintText, _lastHintText) != 0;
    const bool progChanged = (progressPermille != _lastProgressPermille) ||
                             (lightProgress != _lastLightProgress);

    // 只要有任一动态项变化,才擦除动态背景区域一次(而非整屏)
    const bool anyChanged = timeChanged || subChanged || hintChanged || progChanged;
    if (anyChanged) {
        _tft->fillRect(0, dynTop, CFG_DISPLAY_WIDTH, dynBottom - dynTop, bgFill);
    }

    int16_t x1, y1;
    uint16_t w, h;

    // 时间文字(size 6)
    _tft->getTft().setTextSize(6);
    _tft->getTft().setTextColor(COLOR_BLACK);
    _tft->getTft().getTextBounds(timeText, 0, 0, &x1, &y1, &w, &h);
    _tft->getTft().setCursor((CFG_DISPLAY_WIDTH - w) / 2, 72);
    _tft->getTft().print(timeText);

    // 子文字(size 2)
    _tft->getTft().setTextSize(2);
    _tft->getTft().setTextColor(COLOR_BLACK);
    _tft->getTft().getTextBounds(subText, 0, 0, &x1, &y1, &w, &h);
    _tft->getTft().setCursor((CFG_DISPLAY_WIDTH - w) / 2, 132);
    _tft->getTft().print(subText);

    // 进度条填充
    const uint16_t fillColor = lightProgress ? COLOR_WHITE : COLOR_BLACK;
    const int16_t fillW = (barW - 4) * progressPermille / 1000;
    // 先清空进度条内部再用填充色覆盖,避免残留
    _tft->fillRect(barX + 2, barY + 2, barW - 4, barH - 4, bgFill);
    if (fillW > 0) _tft->fillRect(barX + 2, barY + 2, fillW, barH - 4, fillColor);

    // 底部 hint(右对齐,橙色,需擦旧字)
    if (hintChanged) {
        // 擦除 hint 区域(右半边底部黑条)
        _tft->fillRect(CFG_DISPLAY_WIDTH / 2, captionY + 6, CFG_DISPLAY_WIDTH / 2 - 4, 16, COLOR_BLACK);
        _tft->getTft().setTextSize(1);
        _tft->getTft().setTextColor(COLOR_ORANGE);
        _tft->getTft().getTextBounds(hintText, 0, 0, &x1, &y1, &w, &h);
        _tft->getTft().setCursor(CFG_DISPLAY_WIDTH - w - 10, captionY + 8);
        _tft->getTft().print(hintText);
    }

    // 缓存最新值
    strncpy(_lastTimeText, timeText, sizeof(_lastTimeText) - 1);
    _lastTimeText[sizeof(_lastTimeText) - 1] = '\0';
    strncpy(_lastSubText, subText, sizeof(_lastSubText) - 1);
    _lastSubText[sizeof(_lastSubText) - 1] = '\0';
    strncpy(_lastHintText, hintText, sizeof(_lastHintText) - 1);
    _lastHintText[sizeof(_lastHintText) - 1] = '\0';
    _lastProgressPermille = progressPermille;
    _lastLightProgress = lightProgress;
}

void DisplayService::renderTimeScreen(const char* mark, const char* timeText, const char* subText,
                                      const char* modeText, const char* hintText,
                                      uint16_t progressPermille, bool lightProgress) {
    if (_timeViewDirty || !_timeViewLayoutDrawn) {
        renderTimeScreenLayout(mark, modeText);
        _timeViewDirty = false;
        // 完整重绘后,重置缓存以强制 dynamic 重画所有动态项
        _lastTimeText[0] = '\0';
        _lastSubText[0] = '\0';
        _lastHintText[0] = '\0';
        _lastProgressPermille = 0xFFFF;
    }
    renderTimeScreenDynamic(timeText, subText, hintText, progressPermille, lightProgress);
}

void DisplayService::drawClockView() {
    char timeText[8] = "--:--";
    char dateText[16] = "TIME WAIT";
    uint16_t progress = 0;

    if (_timeService && _timeService->getEpoch() > 1000000000UL) {
        snprintf(timeText, sizeof(timeText), "%02d:%02d",
                 _timeService->getHour(), _timeService->getMinute());
        snprintf(dateText, sizeof(dateText), "%04d-%02d-%02d",
                 _timeService->getYear(), _timeService->getMonth(), _timeService->getDay());
        progress = (_timeService->getSecond() * 1000UL) / 59UL;
    }

    renderTimeScreen("MOCHI", timeText, dateText, "CLOCK", "IDLE", progress, false);
}

void DisplayService::drawPomodoroView() {
    uint32_t remaining = getPomodoroRemainingSec();
    uint32_t duration = getPomodoroDurationSec();
    uint32_t elapsed = duration > remaining ? duration - remaining : 0;

    char timeText[8];
    snprintf(timeText, sizeof(timeText), "%02lu:%02lu",
             (unsigned long)(remaining / 60), (unsigned long)(remaining % 60));

    const bool isBreak = _pomodoroPhase == PomodoroPhase::BREAK;
    const char* mark = isBreak ? "BREAK" : "FOCUS";
    const char* sub = isBreak ? "BREAK LEFT" : "FOCUS LEFT";
    const char* hint = _pomodoroPaused ? "PAUSED" : (isBreak ? "REST" : "FOCUS");
    uint16_t progress = (elapsed * 1000UL) / duration;
    renderTimeScreen(mark, timeText, sub, "POMODORO", hint, progress, isBreak);
}

// ── Terminal ───────────────────────────────────────────────────
void DisplayService::termClear() {
    for (uint8_t i = 0; i < TERM_ROWS; i++) _termLines[i] = "";
    _termRow = 0; _termCol = 0;
}

void DisplayService::termDrawHeader() {
    _tft->fillRect(0, 0, CFG_DISPLAY_WIDTH, TERM_PAD_Y + 1, COLOR_DARKBG);
    _tft->getTft().setTextColor(COLOR_ORANGE);
    _tft->getTft().setTextSize(1);
    _tft->getTft().setCursor(TERM_PAD_X, 4);
    _tft->getTft().print("clawd@mochi terminal");
    _tft->getTft().drawFastHLine(0, TERM_PAD_Y, CFG_DISPLAY_WIDTH, COLOR_ORANGE);
}

void DisplayService::termDrawPrefix(int16_t yy) {
    _tft->getTft().setTextColor(COLOR_TERM_GREEN);
    _tft->getTft().setTextSize(1);
    _tft->getTft().setCursor(TERM_PAD_X, yy + 6);
    _tft->getTft().print("clawd:~$ ");
}

void DisplayService::termDrawLine(uint8_t r) {
    const int16_t yy = TERM_PAD_Y + 4 + r * TERM_CHAR_H;
    _tft->fillRect(0, yy, CFG_DISPLAY_WIDTH, TERM_CHAR_H, COLOR_DARKBG);
    if (r == _termRow) termDrawPrefix(yy);
    _tft->getTft().setTextColor(COLOR_WHITE);
    _tft->getTft().setTextSize(2);
    _tft->getTft().setCursor(TERM_PAD_X + PREFIX_PX, yy + 1);
    _tft->getTft().print(_termLines[r]);
    if (r == _termRow) {
        const int16_t cx = TERM_PAD_X + PREFIX_PX + _termCol * TERM_CHAR_W;
        _tft->fillRect(cx, yy + 1, TERM_CHAR_W - 2, TERM_CHAR_H - 2, COLOR_TERM_GREEN);
    }
}

void DisplayService::termDrawLastChar() {
    if (_termCol == 0) return;
    const int16_t yy    = TERM_PAD_Y + 4 + _termRow * TERM_CHAR_H;
    const int16_t baseX = TERM_PAD_X + PREFIX_PX;
    const uint8_t prev  = _termCol - 1;
    _tft->fillRect(baseX + prev * TERM_CHAR_W, yy + 1, TERM_CHAR_W, TERM_CHAR_H - 1, COLOR_DARKBG);
    _tft->getTft().setTextColor(COLOR_WHITE);
    _tft->getTft().setTextSize(2);
    _tft->getTft().setCursor(baseX + prev * TERM_CHAR_W, yy + 1);
    _tft->getTft().print(_termLines[_termRow][prev]);
    _tft->fillRect(baseX + _termCol * TERM_CHAR_W, yy + 1, TERM_CHAR_W - 2, TERM_CHAR_H - 2, COLOR_TERM_GREEN);
}

void DisplayService::termDrawBackspace() {
    const int16_t yy    = TERM_PAD_Y + 4 + _termRow * TERM_CHAR_H;
    const int16_t baseX = TERM_PAD_X + PREFIX_PX;
    _tft->fillRect(baseX + _termCol * TERM_CHAR_W, yy + 1, TERM_CHAR_W * 2, TERM_CHAR_H - 1, COLOR_DARKBG);
    _tft->fillRect(baseX + _termCol * TERM_CHAR_W, yy + 1, TERM_CHAR_W - 2, TERM_CHAR_H - 2, COLOR_TERM_GREEN);
    if (_termLines[_termRow].length() == 0) {
        _tft->fillRect(0, yy, TERM_PAD_X + PREFIX_PX, TERM_CHAR_H, COLOR_DARKBG);
    }
}

void DisplayService::termFullRedraw() {
    _tft->fillScreen(COLOR_DARKBG);
    termDrawHeader();
    for (uint8_t r = 0; r < TERM_ROWS; r++) termDrawLine(r);
}

void DisplayService::termScroll() {
    for (uint8_t i = 0; i < TERM_ROWS - 1; i++) _termLines[i] = _termLines[i + 1];
    _termLines[TERM_ROWS - 1] = "";
    _termRow = TERM_ROWS - 1;
    termFullRedraw();
}

void DisplayService::termAddChar(char c) {
    if (c == '\n' || c == '\r') {
        const int16_t yy = TERM_PAD_Y + 4 + _termRow * TERM_CHAR_H;
        _tft->fillRect(TERM_PAD_X + PREFIX_PX + _termCol * TERM_CHAR_W,
                       yy + 1, TERM_CHAR_W, TERM_CHAR_H - 1, COLOR_DARKBG);
        _termRow++; _termCol = 0;
        if (_termRow >= TERM_ROWS) { termScroll(); return; }
        termDrawLine(_termRow);
    } else if (c == '\b' || c == 127) {
        if (_termCol > 0) {
            _termCol--;
            _termLines[_termRow].remove(_termLines[_termRow].length() - 1);
            termDrawBackspace();
        }
    } else if (c >= 32 && c < 127) {
        if (_termCol >= TERM_COLS) {
            _termRow++; _termCol = 0;
            if (_termRow >= TERM_ROWS) { termScroll(); return; }
        }
        if (_termCol == 0) termDrawPrefix(TERM_PAD_Y + 4 + _termRow * TERM_CHAR_H);
        _termLines[_termRow] += c;
        _termCol++;
        termDrawLastChar();
    }
}

void DisplayService::exitTerminal() {
    _termMode = false;
    drawCodeView();
}

// ── Animations ─────────────────────────────────────────────────
void DisplayService::animNormalEyes() {
    _busy = true;
    const int16_t offs[] = {-16, 16, -16, 16, 0};
    for (uint8_t i = 0; i < 5; i++) { drawNormalEyes(offs[i]); delay(speedMs(80)); }
    drawNormalEyes(0, true);  delay(speedMs(100));
    drawNormalEyes(0, false); delay(speedMs(70));
    drawNormalEyes(0, true);  delay(speedMs(70));
    drawNormalEyes(0, false);
    _busy = false;
}

void DisplayService::animSquishEyes() {
    _busy = true;
    for (uint8_t i = 0; i < 3; i++) {
        drawSquishEyes(false); delay(speedMs(160));
        drawSquishEyes(true);  delay(speedMs(100));
    }
    drawSquishEyes(false);
    _busy = false;
}

void DisplayService::drawThinking(uint8_t dotCount) {
    _tft->fillScreen(_animBgColor);
    const int16_t lx = eyeLX(0), rx = eyeRX(0);
    const int16_t ey = eyeY(), cy = eyeCY();
    _tft->fillRect(lx, ey, EYE_W, EYE_H, COLOR_BLACK);
    _tft->fillRect(lx + EYE_W/2 - 3, cy - 3, 6, 6, COLOR_WHITE);
    _tft->fillRect(rx, ey, EYE_W, EYE_H, COLOR_BLACK);
    _tft->fillRect(rx + EYE_W - 10, ey + 6, 6, 6, COLOR_WHITE);
    if (dotCount > 0) {
        int16_t dx = rx + EYE_W/2;
        int16_t dy = ey - 18;
        if (dotCount >= 1) _tft->fillCircle(dx - 10, dy, 3, COLOR_GREEN);
        if (dotCount >= 2) _tft->fillCircle(dx,      dy, 3, COLOR_GREEN);
        if (dotCount >= 3) _tft->fillCircle(dx + 10, dy, 3, COLOR_GREEN);
    }
}

void DisplayService::animThinking() {
    _busy = true;
    for (uint8_t rep = 0; rep < 3; rep++) {
        for (uint8_t d = 1; d <= 3; d++) { drawThinking(d); delay(speedMs(300)); }
        drawThinking(0); delay(speedMs(200));
    }
    _busy = false;
}

void DisplayService::drawWorking(bool blinkLeft, bool blinkRight) {
    _tft->fillScreen(_animBgColor);
    const int16_t lx = eyeLX(0), rx = eyeRX(0);
    const int16_t ey = eyeY(), cy = eyeCY();
    if (blinkLeft) _tft->fillRect(lx, cy - 5, EYE_W, 10, COLOR_BLACK);
    else { _tft->fillRect(lx, ey, EYE_W, EYE_H, COLOR_BLACK); _tft->fillRect(lx + EYE_W/2 - 3, cy + 10, 6, 6, COLOR_WHITE); }
    if (blinkRight) _tft->fillRect(rx, cy - 5, EYE_W, 10, COLOR_BLACK);
    else { _tft->fillRect(rx, ey, EYE_W, EYE_H, COLOR_BLACK); _tft->fillRect(rx + EYE_W/2 - 3, cy + 10, 6, 6, COLOR_WHITE); }
    _tft->fillRect(lx - 10, ey + EYE_H + 12, (rx - lx + EYE_W + 20), 3, COLOR_ORANGE);
}

void DisplayService::animWorking() {
    _busy = true;
    for (uint8_t i = 0; i < 4; i++) {
        drawWorking(true, false); delay(speedMs(100));
        drawWorking();            delay(speedMs(60));
        drawWorking(false, true); delay(speedMs(100));
        drawWorking();            delay(speedMs(60));
    }
    _busy = false;
}

void DisplayService::animLogoReveal() {
    _busy = true;
    BootAnimation::run(*_tft);
    _busy = false;
}

// ── Interactive mode ───────────────────────────────────────────
void DisplayService::enterInteractive() {
    _interactiveActive = true;
    _currentMode = DisplayMode::INTERACTIVE;
    _interactiveView = InteractiveView::EYES_NORMAL;
    drawNormalEyes();
}

void DisplayService::exitInteractive() {
    _interactiveActive = false;
    _termMode = false;
    _currentMode = DisplayMode::EXPRESSION;
    _tft->clear(COLOR_BLACK);
    _eyesView.init();
}

void DisplayService::setInteractiveView(uint8_t view) {
    if (!_interactiveActive) enterInteractive();
    _interactiveView = static_cast<InteractiveView>(view);
    _termMode = false;
    switch (_interactiveView) {
        case InteractiveView::EYES_NORMAL:
            animNormalEyes();
            break;
        case InteractiveView::EYES_SQUISH:
            animSquishEyes();
            break;
        case InteractiveView::CODE_VIEW:
            drawCodeView();
            _termMode = true;
            termClear();
            termFullRedraw();
            break;
        case InteractiveView::DRAW:
            _tft->fillScreen(_drawBgColor);
            break;
        case InteractiveView::THINKING:
            animThinking();
            break;
        case InteractiveView::WORKING:
            animWorking();
            break;
        case InteractiveView::CLOCK:
            showClock();
            break;
        case InteractiveView::POMODORO:
            startPomodoro(PomodoroPhase::FOCUS);
            break;
    }
}

void DisplayService::redrawCurrentView() {
    switch (_interactiveView) {
        case InteractiveView::EYES_NORMAL: drawNormalEyes(); break;
        case InteractiveView::EYES_SQUISH: drawSquishEyes(); break;
        case InteractiveView::CODE_VIEW:   drawCodeView();   break;
        case InteractiveView::DRAW:        _tft->fillScreen(_drawBgColor); break;
        case InteractiveView::THINKING:    drawThinking(); break;
        case InteractiveView::WORKING:     drawWorking();  break;
        case InteractiveView::CLOCK:       drawClockView(); break;
        case InteractiveView::POMODORO:    drawPomodoroView(); break;
    }
}

void DisplayService::drawClear(uint16_t bgColor) {
    _drawBgColor = bgColor;
    _animBgColor = bgColor;
    _interactiveView = InteractiveView::DRAW;
    _termMode = false;
    _tft->fillScreen(bgColor);
}

void DisplayService::drawStroke(uint16_t penColor, const String& pointsData) {
    _interactiveView = InteractiveView::DRAW;
    struct Pt { int16_t x, y; };
    Pt prev = {-1, -1};
    int start = 0;
    while (start < (int)pointsData.length()) {
        int semi = pointsData.indexOf(';', start);
        if (semi == -1) semi = pointsData.length();
        String entry = pointsData.substring(start, semi);
        const int comma = entry.indexOf(',');
        if (comma > 0) {
            const int16_t x = entry.substring(0, comma).toInt();
            const int16_t y = entry.substring(comma + 1).toInt();
            if (prev.x >= 0) {
                _tft->drawLine(prev.x, prev.y, x, y, penColor);
                _tft->drawLine(prev.x + 1, prev.y, x + 1, y, penColor);
                _tft->drawLine(prev.x, prev.y + 1, x, y + 1, penColor);
            } else {
                _tft->fillCircle(x, y, 2, penColor);
            }
            prev = {x, y};
        }
        start = semi + 1;
    }
}

void DisplayService::showClock() {
    if (!_interactiveActive) enterInteractive();
    _interactiveView = InteractiveView::CLOCK;
    _termMode = false;
    _lastClockRenderSec = 0;
    invalidateTimeView();
    drawClockView();
}

void DisplayService::startPomodoro(PomodoroPhase phase) {
    if (!_interactiveActive) enterInteractive();
    _interactiveView = InteractiveView::POMODORO;
    _termMode = false;
    _pomodoroPhase = phase;
    const uint16_t minutes = phase == PomodoroPhase::BREAK ? _breakMinutes : _focusMinutes;
    _pomodoroDurationSec = (uint32_t)minutes * 60UL;
    if (_pomodoroDurationSec == 0) _pomodoroDurationSec = 1;
    _pomodoroRemainingAtPauseSec = _pomodoroDurationSec;
    _pomodoroStartedMs = millis();
    _pomodoroRunning = true;
    _pomodoroPaused = false;
    _lastClockRenderSec = 0;
    invalidateTimeView();
    drawPomodoroView();
}

void DisplayService::pausePomodoro() {
    if (!_pomodoroRunning) return;
    if (_pomodoroPaused) {
        _pomodoroStartedMs = millis();
        _pomodoroPaused = false;
    } else {
        _pomodoroRemainingAtPauseSec = getPomodoroRemainingSec();
        _pomodoroPaused = true;
    }
    // 只刷新 hint 区域,不需要全屏重绘布局
    drawPomodoroView();
}

void DisplayService::resetPomodoro() {
    _pomodoroRunning = false;
    _pomodoroPaused = false;
    _pomodoroDurationSec = (uint32_t)_focusMinutes * 60UL;
    _pomodoroRemainingAtPauseSec = _pomodoroDurationSec;
    _pomodoroPhase = PomodoroPhase::FOCUS;
    if (_interactiveView == InteractiveView::POMODORO) {
        invalidateTimeView();
        drawPomodoroView();
    }
}

void DisplayService::setPomodoroDurations(uint16_t focusMinutes, uint16_t breakMinutes) {
    _focusMinutes = constrain(focusMinutes, 1, 180);
    _breakMinutes = constrain(breakMinutes, 1, 60);
    if (!_pomodoroRunning) {
        _pomodoroDurationSec = (uint32_t)_focusMinutes * 60UL;
        _pomodoroRemainingAtPauseSec = _pomodoroDurationSec;
        if (_interactiveView == InteractiveView::POMODORO) {
            invalidateTimeView();
            drawPomodoroView();
        }
    }
}

uint32_t DisplayService::getPomodoroRemainingSec() const {
    if (!_pomodoroRunning) return _pomodoroRemainingAtPauseSec;
    if (_pomodoroPaused) return _pomodoroRemainingAtPauseSec;
    const uint32_t elapsed = (millis() - _pomodoroStartedMs) / 1000UL;
    return elapsed >= _pomodoroRemainingAtPauseSec ? 0 : _pomodoroRemainingAtPauseSec - elapsed;
}

uint32_t DisplayService::getPomodoroDurationSec() const {
    return _pomodoroDurationSec == 0 ? 1 : _pomodoroDurationSec;
}

void DisplayService::setBrightnessPercent(uint8_t percent) {
    _brightnessPercent = constrain(percent, 0, 100);
    const uint8_t pwm = map(_brightnessPercent, 0, 100, 0, 255);
    _tft->setBrightness(pwm);
}

// ── Main update loop ───────────────────────────────────────────
// 状态机负责切换显示模式(switchToExpressionMode/switchToInfoMode/updateProvisioning),
// 本方法只负责按当前 _currentMode 渲染。
void DisplayService::update() {
    unsigned long now = millis();
    if (_currentMode == DisplayMode::INTERACTIVE) {
        if (_interactiveView != InteractiveView::CLOCK &&
            _interactiveView != InteractiveView::POMODORO) {
            return;
        }
        const unsigned long sec = now / 1000UL;
        if (sec == _lastClockRenderSec) return;
        _lastClockRenderSec = sec;
        if (_interactiveView == InteractiveView::CLOCK) {
            drawClockView();
        } else {
            if (_pomodoroRunning && !_pomodoroPaused && getPomodoroRemainingSec() == 0) {
                _pomodoroRemainingAtPauseSec = 0;
                _pomodoroRunning = false;
                _pomodoroPaused = false;
            }
            drawPomodoroView();
        }
        return;
    }

    if (now - _lastRefreshMs < DISPLAY_REFRESH_INTERVAL_MS) return;
    _lastRefreshMs = now;

    if (_currentMode == DisplayMode::SETUP) {
        return;
    } else if (_currentMode == DisplayMode::PROVISIONING) {
        updateProvisioning();
        return;
    } else if (_currentMode == DisplayMode::EXPRESSION) {
        // 状态变化时由调用方触发 switchToExpressionMode();这里保持当前画面
        return;
    } else {
        // INFO 模式:渲染 Claude Code 信息视图
        auto status = _ccService->getStatus();
        _ccView.render(status,
                       _ccService->getHookName(),
                       _ccService->getToolName(),
                       _ccService->getDetail(),
                       _ccService->getModel(),
                       _ccService->getElapsedMs());
    }
}

void DisplayService::updateProvisioning() {
    static WifiConfigService::ProvisioningMode lastMode = WifiConfigService::ProvisioningMode::NONE;
    auto mode = _wifiService->getProvisioningMode();
    if (mode == lastMode && _currentMode == DisplayMode::PROVISIONING) return;

    _currentMode = DisplayMode::PROVISIONING;
    lastMode = mode;

    const char* msg = _wifiService->getProvisioningMessage();
    _tft->fillScreen(COLOR_DARKBG);

    _tft->fillRect(0, 0, CFG_DISPLAY_WIDTH, 4, COLOR_ORANGE);
    _tft->fillRect(0, CFG_DISPLAY_HEIGHT - 4, CFG_DISPLAY_WIDTH, 4, COLOR_ORANGE);
    _tft->drawRoundRect(10, 12, CFG_DISPLAY_WIDTH - 20, CFG_DISPLAY_HEIGHT - 24, 8, COLOR_MUTED);

    _tft->getTft().setTextColor(COLOR_ORANGE);
    _tft->getTft().setTextSize(2);
    const char* title = "WiFi Setup";
    int16_t titleX = (CFG_DISPLAY_WIDTH - (int)strlen(title) * 12) / 2;
    _tft->getTft().setCursor(titleX, 22);
    _tft->getTft().print(title);

    _tft->getTft().setTextColor(COLOR_GRAY);
    _tft->getTft().setTextSize(1);
    const char* subtitle = "Keep this page open";
    int16_t subX = (CFG_DISPLAY_WIDTH - (int)strlen(subtitle) * 6) / 2;
    _tft->getTft().setCursor(subX, 46);
    _tft->getTft().print(subtitle);

    if (mode == WifiConfigService::ProvisioningMode::AP_FALLBACK) {
        QRCode qrcode;
        uint8_t qrcodeData[qrcode_getBufferSize(3)];
        qrcode_initText(&qrcode, qrcodeData, 3, ECC_LOW, "http://192.168.4.1");
        const int16_t scale = 3;
        const int16_t qrSize = qrcode.size * scale;
        const int16_t pad = 4;
        const int16_t boxSize = qrSize + pad * 2;
        const int16_t qrX = (CFG_DISPLAY_WIDTH - qrSize) / 2;
        const int16_t boxY = 66;
        const int16_t qrY = boxY + pad;
        _tft->drawRoundRect(qrX - pad - 3, boxY - 3, boxSize + 6, boxSize + 6, 6, COLOR_ORANGE);
        _tft->fillRect(qrX - pad, boxY, boxSize, boxSize, COLOR_WHITE);
        for (uint8_t y = 0; y < qrcode.size; y++) {
            for (uint8_t x2 = 0; x2 < qrcode.size; x2++) {
                if (qrcode_getModule(&qrcode, x2, y)) {
                    _tft->fillRect(qrX + x2 * scale, qrY + y * scale, scale, scale, COLOR_BLACK);
                }
            }
        }

        _tft->getTft().setTextColor(COLOR_WHITE);
        _tft->getTft().setTextSize(1);
        _tft->getTft().setCursor(34, 174);
        _tft->getTft().print("Scan QR or open:");

        _tft->getTft().setTextColor(COLOR_MUTED);
        _tft->getTft().setCursor(39, 190);
        _tft->getTft().print("http://192.168.4.1");

        _tft->getTft().setTextColor(COLOR_GREEN);
        _tft->getTft().setCursor(63, 210);
        _tft->getTft().print(msg);
    } else if (mode == WifiConfigService::ProvisioningMode::CONNECTING) {
        _tft->drawRoundRect(46, 88, 148, 74, 8, COLOR_YELLOW);
        _tft->getTft().setTextColor(COLOR_YELLOW);
        _tft->getTft().setTextSize(2);
        int16_t x = (CFG_DISPLAY_WIDTH - (int)strlen(msg) * 12) / 2;
        _tft->getTft().setCursor(x, 108);
        _tft->getTft().print(msg);

        _tft->getTft().setTextColor(COLOR_GRAY);
        _tft->getTft().setTextSize(1);
        const char* hint = "Joining network";
        int16_t hintX = (CFG_DISPLAY_WIDTH - (int)strlen(hint) * 6) / 2;
        _tft->getTft().setCursor(hintX, 140);
        _tft->getTft().print(hint);
    } else if (mode == WifiConfigService::ProvisioningMode::CONNECTED) {
        _tft->drawRoundRect(38, 82, 164, 92, 8, COLOR_GREEN);
        _tft->getTft().setTextColor(COLOR_GREEN);
        _tft->getTft().setTextSize(2);
        const char* ok = "Connected";
        int16_t x = (CFG_DISPLAY_WIDTH - (int)strlen(ok) * 12) / 2;
        _tft->getTft().setCursor(x, 102);
        _tft->getTft().print(ok);

        _tft->getTft().setTextColor(COLOR_WHITE);
        _tft->getTft().setTextSize(1);
        String ip = "IP: " + _wifiService->getIP();
        int16_t ipX = (CFG_DISPLAY_WIDTH - (int)ip.length() * 6) / 2;
        _tft->getTft().setCursor(ipX, 138);
        _tft->getTft().print(ip);
    }
}

void DisplayService::switchToExpressionMode() {
    _currentMode = DisplayMode::EXPRESSION;
    auto status = _ccService->getStatus();
    if (status == ClaudeCodeService::Status::THINKING) drawThinking(3);
    else if (status == ClaudeCodeService::Status::WORKING) drawWorking();
    else drawNormalEyes();
}

void DisplayService::switchToInfoMode() {
    _currentMode = DisplayMode::INFO;
    _ccView.reset();
    _tft->clear(COLOR_DARKBG);
}

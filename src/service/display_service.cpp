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

DisplayService::DisplayService(TftDisplay* tft, ClaudeCodeService* ccService, WifiConfigService* wifiService)
    : _tft(tft), _ccService(ccService), _wifiService(wifiService)
    , _ccView(tft), _eyesView(tft)
    , _currentMode(DisplayMode::SETUP), _lastRefreshMs(0)
    , _interactiveView(InteractiveView::EYES_NORMAL), _interactiveActive(false)
    , _busy(false), _animSpeed(1)
    , _animBgColor(COLOR_ORANGE), _drawBgColor(COLOR_ORANGE)
    , _termMode(false), _termRow(0), _termCol(0)
{
}

void DisplayService::init() {
    _currentMode = DisplayMode::SETUP;
    _animBgColor = COLOR_ORANGE;
    _drawBgColor = COLOR_ORANGE;
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

// ── Main update loop ───────────────────────────────────────────
// 状态机负责切换显示模式(switchToExpressionMode/switchToInfoMode/updateProvisioning),
// 本方法只负责按当前 _currentMode 渲染。
void DisplayService::update() {
    if (_currentMode == DisplayMode::INTERACTIVE) return;

    unsigned long now = millis();
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
    lastMode = mode;

    const char* msg = _wifiService->getProvisioningMessage();
    _tft->fillScreen(COLOR_DARKBG);

    // 顶部 + 底部装饰条
    _tft->fillRect(0, 0, CFG_DISPLAY_WIDTH, 3, COLOR_ORANGE);
    _tft->fillRect(0, CFG_DISPLAY_HEIGHT - 3, CFG_DISPLAY_WIDTH, 3, COLOR_ORANGE);

    // 标题 "Clawd Mochi" — 居中,size 2
    _tft->getTft().setTextColor(COLOR_ORANGE);
    _tft->getTft().setTextSize(2);
    const char* title = "Clawd Mochi";
    int16_t titleX = (CFG_DISPLAY_WIDTH - (int)strlen(title) * 12) / 2;
    _tft->getTft().setCursor(titleX, 10);
    _tft->getTft().print(title);

    // 副标题 — 居中,size 1
    _tft->getTft().setTextColor(COLOR_WHITE);
    _tft->getTft().setTextSize(1);
    const char* subtitle = "WiFi provisioning";
    int16_t subX = (CFG_DISPLAY_WIDTH - (int)strlen(subtitle) * 6) / 2;
    _tft->getTft().setCursor(subX, 32);
    _tft->getTft().print(subtitle);

    if (mode == WifiConfigService::ProvisioningMode::AP_FALLBACK) {
        // 二维码 — 版本 3, 3px 缩放 = 87×87, 白底 95×95 居中
        QRCode qrcode;
        uint8_t qrcodeData[qrcode_getBufferSize(3)];
        qrcode_initText(&qrcode, qrcodeData, 3, ECC_LOW, "http://192.168.4.1");
        const int16_t scale = 3;
        const int16_t qrSize = qrcode.size * scale;        // 87
        const int16_t pad = 4;
        const int16_t boxSize = qrSize + pad * 2;           // 95
        const int16_t qrX = (CFG_DISPLAY_WIDTH - qrSize) / 2;
        const int16_t boxY = 50;
        const int16_t qrY = boxY + pad;
        _tft->fillRect(qrX - pad, boxY, boxSize, boxSize, COLOR_WHITE);
        for (uint8_t y = 0; y < qrcode.size; y++) {
            for (uint8_t x2 = 0; x2 < qrcode.size; x2++) {
                if (qrcode_getModule(&qrcode, x2, y)) {
                    _tft->fillRect(qrX + x2 * scale, qrY + y * scale, scale, scale, COLOR_BLACK);
                }
            }
        }

        // 底部简约信息 — 两行
        _tft->getTft().setTextColor(COLOR_GRAY);
        _tft->getTft().setTextSize(1);
        _tft->getTft().setCursor(20, 158);
        _tft->getTft().print("Scan QR to setup WiFi");

        _tft->getTft().setTextColor(COLOR_MUTED);
        _tft->getTft().setCursor(20, 178);
        _tft->getTft().print("or join ClaWD-Mochi");

        // 状态行 — 底部,绿色
        _tft->getTft().setTextColor(COLOR_GREEN);
        _tft->getTft().setCursor(20, 210);
        _tft->getTft().print(msg);
    } else if (mode == WifiConfigService::ProvisioningMode::CONNECTING) {
        _tft->getTft().setTextColor(COLOR_YELLOW);
        _tft->getTft().setTextSize(2);
        int16_t x = (CFG_DISPLAY_WIDTH - (int)strlen(msg) * 12) / 2;
        _tft->getTft().setCursor(x, 120);
        _tft->getTft().print(msg);
    } else if (mode == WifiConfigService::ProvisioningMode::CONNECTED) {
        _tft->getTft().setTextColor(COLOR_GREEN);
        _tft->getTft().setTextSize(2);
        const char* ok = "Connected";
        int16_t x = (CFG_DISPLAY_WIDTH - (int)strlen(ok) * 12) / 2;
        _tft->getTft().setCursor(x, 110);
        _tft->getTft().print(ok);

        _tft->getTft().setTextColor(COLOR_WHITE);
        _tft->getTft().setTextSize(1);
        String ip = "IP: " + _wifiService->getIP();
        int16_t ipX = (CFG_DISPLAY_WIDTH - (int)ip.length() * 6) / 2;
        _tft->getTft().setCursor(ipX, 150);
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
    _tft->clear(COLOR_DARKBG);
}

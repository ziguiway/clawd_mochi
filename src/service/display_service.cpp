#include "display_service.h"
#include "../config/cfg_display.h"
#include "../config/app_config.h"
#include "../view/boot_animation.h"
#include "../view/breakout_game.h"
#include "../view/dino_game.h"
#include "../view/game_2048.h"
#include "../view/game_render_buffer.h"
#include "../view/snake_game.h"
#include "../view/sokoban_game.h"
#include "../view/tetris_game.h"
#include "../utils/logger.h"
#include "../utils/memory_monitor.h"
#include <LittleFS.h>
#include <AnimatedGIF.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <qrcode.h>
#include <time.h>
#include <new>

#define EYE_W   30
#define EYE_H   60
#define EYE_GAP 120
#define EYE_OX  0
#define EYE_OY  40

namespace {
DisplayService* s_mediaGifOwner = nullptr;

void prepareTimetableText(U8G2_FOR_ADAFRUIT_GFX& text,
                          const uint8_t* font, uint16_t foreground) {
    text.setFont(font);
    text.setFontMode(1);
    text.setFontDirection(0);
    text.setForegroundColor(foreground);
}

int16_t timetableTextWidth(U8G2_FOR_ADAFRUIT_GFX& text,
                           const uint8_t* font, const char* value) {
    text.setFont(font);
    return text.getUTF8Width(value);
}

void drawTimetableText(U8G2_FOR_ADAFRUIT_GFX& text,
                       const uint8_t* font, int16_t x, int16_t baseline,
                       const char* value, uint16_t foreground) {
    prepareTimetableText(text, font, foreground);
    text.drawUTF8(x, baseline, value);
}

void drawTimetableTextCentered(U8G2_FOR_ADAFRUIT_GFX& text,
                               const uint8_t* font, int16_t baseline,
                               const char* value, uint16_t foreground) {
    const int16_t width = timetableTextWidth(text, font, value);
    drawTimetableText(text, font, (CFG_DISPLAY_WIDTH - width) / 2,
                      baseline, value, foreground);
}
}

DisplayService::DisplayService(TftDisplay* tft, ClaudeCodeService* ccService,
                               WifiConfigService* wifiService, TimeService* timeService,
                               PreferenceService* preferenceService,
                               WeatherService* weatherService,
                               CryptoService* cryptoService,
                               MarketService* marketService,
                               HolidayService* holidayService,
                               TimetableService* timetableService)
    : _tft(tft), _ccService(ccService), _wifiService(wifiService), _timeService(timeService)
    , _preferenceService(preferenceService)
    , _weatherService(weatherService)
    , _cryptoService(cryptoService)
    , _marketService(marketService)
    , _holidayService(holidayService)
    , _timetableService(timetableService)
    , _salaryCounter(nullptr)
    , _ccView(tft), _eyesView(tft)
    , _monoGameBuffer(nullptr)
    , _arcadeCanvas(nullptr)
    , _activeArcadeGame(nullptr)
    , _mediaRowBuffer(nullptr)
    , _mediaFile(nullptr)
    , _mediaGif(nullptr)
    , _mediaRow(0), _mediaColumn(0)
    , _mediaX(0), _mediaY(0)
    , _mediaWidth(CFG_DISPLAY_WIDTH), _mediaHeight(CFG_DISPLAY_HEIGHT)
    , _mediaHighByte(0)
    , _mediaHasHighByte(false), _mediaFrameReceiving(false)
    , _mediaActive(false)
    , _mediaGifPlaying(false)
    , _mediaGifLoopPending(false)
    , _mediaNextFrameMs(0)
    , _mediaGifOffsetX(0), _mediaGifOffsetY(0)
    , _mediaGifStripX(0), _mediaGifStripY(0)
    , _mediaGifStripWidth(0), _mediaGifStripRows(0)
    , _mediaGifStripActive(false)
    , _mediaLastRenderMs(0), _mediaRenderedFrames(0)
    , _currentMode(DisplayMode::SETUP), _lastRefreshMs(0)
    , _interactiveView(InteractiveView::EYES_NORMAL), _interactiveActive(false)
    , _busy(false), _animSpeed(1)
    , _animBgColor(COLOR_ORANGE), _drawBgColor(COLOR_ORANGE)
    , _brightnessPercent(100)
    , _claudeStatusEnabled(true)
    , _displayTheme(THEME_ORANGE_WHITE), _themeForeground(COLOR_WHITE)
    , _fontStyle(FontStyle::PIXEL)
    , _expressionMode(ExpressionMode::MANUAL)
    , _selectedExpression(ExpressionId::NORMAL)
    , _renderedExpression(ExpressionId::NORMAL)
    , _lastAutoExpression(ExpressionId::NORMAL)
    , _nextAutoEventMs(0), _autoReturnMs(0)
    , _expressionPreferred(true)
    , _carouselEnabled(false), _carouselSpeedSeconds(12)
    , _carouselOrder{
        VIEW_WEATHER, VIEW_CRYPTO, VIEW_MARKET, VIEW_CLOCK, VIEW_SALARY
    }
    , _carouselFixedView(VIEW_WEATHER), _carouselIndex(0)
    , _carouselPageStartedMs(0), _carouselSuspended(false)
    , _focusMinutes(25), _breakMinutes(5), _pomodoroPhase(PomodoroPhase::FOCUS)
    , _pomodoroRunning(false), _pomodoroPaused(false)
    , _pomodoroDurationSec(25UL * 60UL), _pomodoroRemainingAtPauseSec(25UL * 60UL)
    , _pomodoroStartedMs(0), _lastClockRenderSec(0)
    , _lastSalaryRenderMs(0), _lastSalaryScheduleCheckSec(0)
    , _lastTimetableRenderMinute(ULONG_MAX)
    , _lastTimetableState(TimetableState::NOT_CONFIGURED)
    , _lastTimetableMinutes(0xFFFF)
    , _timetableLayoutDrawn(false)
    , _lastTimetableCourse{0}
    , _lastWeatherVersion(0)
    , _lastCryptoVersion(0)
    , _lastMarketVersion(0)
    , _lastNightDimCheckMs(0), _lastAppliedBrightnessPercent(255)
    , _timeViewDirty(true), _timeViewLayoutDrawn(false)
    , _lastTimeText{0}, _lastSubText{0}, _lastHintText{0}
    , _lastClockLayoutKey{0}
    , _lastProgressPermille(0xFFFF), _lastLightProgress(false)
    , _lastSalaryAmount{0}, _lastSalaryWorked{0}, _lastSalaryState{0}
    , _lastSalaryProgressPermille(0xFFFF)
    , _lastSalaryAmountX(0), _lastSalaryAmountSize(0)
    , _lastSalaryWorkedX(0), _lastSalaryWorkedSize(0)
    , _termMode(false), _termRow(0), _termCol(0)
{
}

DisplayService::~DisplayService() {
    releaseMediaBuffer();
    delete _salaryCounter;
    _salaryCounter = nullptr;
}

void DisplayService::init() {
    _currentMode = DisplayMode::SETUP;
    if (_preferenceService) {
        _animSpeed = _preferenceService->getAnimSpeed();
        _animBgColor = hexToRgb565(_preferenceService->getDefaultBgHex());
        _drawBgColor = _animBgColor;
        _brightnessPercent = _preferenceService->getBrightnessPercent();
        _claudeStatusEnabled = _preferenceService->getClaudeStatusEnabled();
        _displayTheme = _preferenceService->getDisplayTheme();
        _fontStyle = _preferenceService->getFontStyle();
        _tft->setFontStyle(_fontStyle);
        if (_displayTheme == THEME_ORANGE_BLACK) _themeForeground = COLOR_BLACK;
        else if (_displayTheme == THEME_DARK_ORANGE) _themeForeground = COLOR_ORANGE;
        else if (_displayTheme == THEME_MINT) _themeForeground = COLOR_MINT;
        else if (_displayTheme == THEME_PINK) _themeForeground = COLOR_PINK;
        else _themeForeground = COLOR_WHITE;
        _eyesView.setBackgroundColor(_animBgColor);
        _ccView.setForegroundColor(_themeForeground);
        loadIdleDisplayPreferences();
        const uint8_t startupView = _preferenceService->getStartupView();
        _expressionPreferred = !_carouselEnabled &&
            (startupView == VIEW_EYES_NORMAL || startupView == VIEW_EYES_SQUISH);
        _selectedExpression = _preferenceService->getDefaultExpression();
        _expressionMode = _preferenceService->getExpressionMode();
        _renderedExpression = _expressionMode == ExpressionMode::AUTO
            ? ExpressionId::NORMAL : _selectedExpression;
    } else {
        _animBgColor = COLOR_ORANGE;
        _drawBgColor = COLOR_ORANGE;
    }
    applyNightDimming();
}

void DisplayService::setDisplayTheme(uint8_t theme) {
    if (theme >= THEME_COUNT) return;
    _displayTheme = theme;
    if (theme == THEME_ORANGE_BLACK) _themeForeground = COLOR_BLACK;
    else if (theme == THEME_DARK_ORANGE) _themeForeground = COLOR_ORANGE;
    else if (theme == THEME_MINT) _themeForeground = COLOR_MINT;
    else if (theme == THEME_PINK) _themeForeground = COLOR_PINK;
    else _themeForeground = COLOR_WHITE;
    _ccView.setForegroundColor(_themeForeground);
    invalidateTimeView();
}

void DisplayService::setFontStyle(FontStyle style) {
    if (style >= FontStyle::COUNT || style == _fontStyle) return;
    _fontStyle = style;
    _tft->setFontStyle(style);
    _ccView.reset();
    _timeViewDirty = true;
    _timeViewLayoutDrawn = false;
    _lastTimetableRenderMinute = ULONG_MAX;
    _timetableLayoutDrawn = false;
    _lastSalaryAmount[0] = '\0';
    _lastSalaryWorked[0] = '\0';
    _lastSalaryState[0] = '\0';
    _lastSalaryProgressPermille = 0xFFFF;

    if (_currentMode == DisplayMode::INTERACTIVE) {
        if (_interactiveView == InteractiveView::SALARY_COUNTER) {
            drawSalaryCounterLayout();
            drawSalaryCounterView();
        } else {
            redrawCurrentView();
        }
    }
}

void DisplayService::setExpression(ExpressionId expression) {
    _expressionMode = ExpressionMode::MANUAL;
    _selectedExpression = expression;
    _autoReturnMs = 0;
    _nextAutoEventMs = 0;
    showExpression(expression);
}

void DisplayService::setExpressionMode(ExpressionMode mode) {
    _expressionMode = mode;
    _autoReturnMs = 0;
    if (mode == ExpressionMode::AUTO) {
        showExpression(ExpressionId::NORMAL);
        scheduleNextAutoEvent(millis());
        return;
    }
    _nextAutoEventMs = 0;
    showExpression(_selectedExpression);
}

void DisplayService::showExpression(ExpressionId expression) {
    _currentMode = DisplayMode::EXPRESSION;
    _interactiveActive = false;
    _expressionPreferred = true;
    _interactiveView = expression == ExpressionId::HAPPY
        ? InteractiveView::EYES_SQUISH : InteractiveView::EYES_NORMAL;
    _renderedExpression = expression;
    _eyesView.setExpression(expression);
}

void DisplayService::scheduleNextAutoEvent(unsigned long now) {
    _nextAutoEventMs = now + random(12000UL, 35001UL);
}

void DisplayService::updateAutoExpression(unsigned long now) {
    if (_expressionMode != ExpressionMode::AUTO) return;

    if (_autoReturnMs != 0 &&
        static_cast<long>(now - _autoReturnMs) >= 0) {
        _autoReturnMs = 0;
        showExpression(ExpressionId::NORMAL);
        scheduleNextAutoEvent(now);
        return;
    }
    if (_autoReturnMs != 0 || _nextAutoEventMs == 0 ||
        static_cast<long>(now - _nextAutoEventMs) < 0) {
        return;
    }

    constexpr ExpressionId AUTO_EVENTS[] = {
        ExpressionId::HAPPY,
        ExpressionId::CURIOUS,
        ExpressionId::SURPRISED,
        ExpressionId::THINKING,
        ExpressionId::HAPPY,
        ExpressionId::CURIOUS,
        ExpressionId::SLEEPING,
    };
    constexpr uint8_t AUTO_EVENT_COUNT =
        sizeof(AUTO_EVENTS) / sizeof(AUTO_EVENTS[0]);

    ExpressionId next = _lastAutoExpression;
    for (uint8_t attempt = 0; attempt < 4 && next == _lastAutoExpression; attempt++) {
        next = AUTO_EVENTS[random(0, AUTO_EVENT_COUNT)];
    }
    if (next == _lastAutoExpression) {
        next = next == ExpressionId::HAPPY
            ? ExpressionId::CURIOUS : ExpressionId::HAPPY;
    }

    _lastAutoExpression = next;
    showExpression(next);
    const unsigned long duration = next == ExpressionId::SLEEPING
        ? 6000UL : (next == ExpressionId::THINKING ? 4000UL : 2200UL);
    _autoReturnMs = now + duration;
    _nextAutoEventMs = 0;
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
    _lastClockLayoutKey[0] = '\0';
    _lastProgressPermille = 0xFFFF;
    _lastLightProgress = false;
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
    _tft->drawText(14, 14, mark, _themeForeground, COLOR_ORANGE,
                   FONT_SMALL);
    _tft->fillRect(14, 31, 42, 4, _themeForeground);

    // 进度条外框(静态)
    _tft->drawRect(barX, barY, barW, barH, _themeForeground);

    // 轻量分隔线代替深色底栏，保持橙白主题完整。
    _tft->fillRect(0, captionY, CFG_DISPLAY_WIDTH,
                   CFG_DISPLAY_HEIGHT - captionY, COLOR_ORANGE);
    _tft->fillRect(10, captionY, CFG_DISPLAY_WIDTH - 20, 3,
                   _themeForeground);
    _tft->drawText(10, captionY + 8, modeText, _themeForeground,
                   COLOR_ORANGE, FONT_SMALL);

    _timeViewLayoutDrawn = true;
}

void DisplayService::renderTimeScreenDynamic(const char* timeText, const char* subText,
                                             const char* hintText,
                                             uint16_t progressPermille,
                                             bool lightProgress) {
    progressPermille = constrain(progressPermille, 0, 1000);
    const int16_t captionY = 214;
    const int16_t barX = 20;
    const int16_t barY = 184;
    const int16_t barW = 200;
    const int16_t barH = 12;

    const bool timeChanged = strcmp(timeText, _lastTimeText) != 0;
    const bool subChanged = strcmp(subText, _lastSubText) != 0;
    const bool hintChanged = strcmp(hintText, _lastHintText) != 0;
    const bool progChanged = (progressPermille != _lastProgressPermille) ||
                             (lightProgress != _lastLightProgress);

    // 时间文字(size 6)
    if (timeChanged) {
        _tft->fillRect(0, 60, CFG_DISPLAY_WIDTH, 58, COLOR_ORANGE);
        const int16_t timeX =
            (CFG_DISPLAY_WIDTH - _tft->getTextWidth(timeText, 6)) / 2;
        _tft->drawText(timeX, 72, timeText, _themeForeground,
                       COLOR_ORANGE, 6);
    }

    // 子文字(size 2)
    if (subChanged) {
        _tft->fillRect(0, 124, CFG_DISPLAY_WIDTH, 28, COLOR_ORANGE);
        const int16_t subX =
            (CFG_DISPLAY_WIDTH - _tft->getTextWidth(subText, FONT_MEDIUM)) /
            2;
        _tft->drawText(subX, 132, subText, _themeForeground,
                       COLOR_ORANGE, FONT_MEDIUM);
    }

    // 进度条填充
    if (progChanged) {
        const int16_t fillW = (barW - 4) * progressPermille / 1000;
        _tft->fillRect(barX + 2, barY + 2, barW - 4, barH - 4, COLOR_ORANGE);
        if (fillW > 0) {
            _tft->fillRect(barX + 2, barY + 2, fillW, barH - 4,
                           _themeForeground);
        }
        _tft->drawRect(barX, barY, barW, barH, _themeForeground);
    }

    // 底部 hint(右对齐,需擦旧字)
    if (hintChanged) {
        _tft->fillRect(CFG_DISPLAY_WIDTH / 2, captionY + 6,
                       CFG_DISPLAY_WIDTH / 2 - 4, 16, COLOR_ORANGE);
        const int16_t hintX = CFG_DISPLAY_WIDTH -
            _tft->getTextWidth(hintText, FONT_SMALL) - 10;
        _tft->drawText(hintX, captionY + 8, hintText, _themeForeground,
                       COLOR_ORANGE, FONT_SMALL);
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
    renderTimeScreenDynamic(timeText, subText, hintText, progressPermille,
                            lightProgress);
}

void DisplayService::drawClockView() {
    static const char* WEEKDAYS[] = {
        "SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY",
        "THURSDAY", "FRIDAY", "SATURDAY"
    };
    static const char* MONTHS[] = {
        "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
        "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
    };

    const uint16_t background = COLOR_ORANGE;
    const uint16_t foreground = COLOR_WHITE;
    const bool timeValid =
        _timeService && _timeService->getEpoch() > 1000000000UL;

    char timeText[8] = "--:--";
    char secondText[4] = "--";
    char dateText[18] = "WAITING FOR TIME";
    const char* weekdayText = "WAITING";
    char holidayName[24] = {0};
    bool holidayLayout = false;

    if (timeValid) {
        const time_t epoch = static_cast<time_t>(_timeService->getEpoch());
        struct tm current = {};
        localtime_r(&epoch, &current);

        snprintf(timeText, sizeof(timeText), "%02d:%02d",
                 current.tm_hour, current.tm_min);
        snprintf(secondText, sizeof(secondText), "%02d", current.tm_sec);
        weekdayText = WEEKDAYS[current.tm_wday];
        snprintf(dateText, sizeof(dateText), "%02d %s %04d",
                 current.tm_mday, MONTHS[current.tm_mon],
                 current.tm_year + 1900);

        holidayLayout = _holidayService &&
                        _holidayService->isHolidayToday();
        if (holidayLayout) {
            strncpy(holidayName, _holidayService->getHolidayName(),
                    sizeof(holidayName) - 1);
            holidayName[sizeof(holidayName) - 1] = '\0';
        }
    }

    char layoutKey[48];
    snprintf(layoutKey, sizeof(layoutKey), "%s|%s",
             dateText, holidayLayout ? holidayName : "DEFAULT");
    const bool layoutChanged =
        _timeViewDirty ||
        strcmp(layoutKey, _lastClockLayoutKey) != 0;

    if (layoutChanged) {
        _tft->fillScreen(background);

        _tft->drawText(12, 14, "LOCAL TIME", foreground,
                       background, FONT_SMALL);
        const int16_t zoneX = CFG_DISPLAY_WIDTH -
                              _tft->getTextWidth("UTC+8", FONT_SMALL) - 12;
        _tft->drawText(zoneX, 14, "UTC+8", foreground,
                       background, FONT_SMALL);
        _tft->fillRect(12, 35, 216, 2, foreground);

        const int16_t secondsLabelX = CFG_DISPLAY_WIDTH -
                                      _tft->getTextWidth("SEC", FONT_SMALL) - 12;
        _tft->drawText(secondsLabelX, 115, "SEC", foreground,
                       background, FONT_SMALL);

        if (holidayLayout) {
            _tft->drawText(12, 145, weekdayText, foreground,
                           background, FONT_MEDIUM);
            const int16_t dateX = CFG_DISPLAY_WIDTH -
                                  _tft->getTextWidth(dateText, FONT_SMALL) - 12;
            _tft->drawText(dateX, 149, dateText, foreground,
                           background, FONT_SMALL);
            _tft->drawText(12, 173, "TODAY / HOLIDAY", foreground,
                           background, FONT_SMALL);
            _tft->drawText(12, 190, holidayName, foreground,
                           background, FONT_MEDIUM);
            _tft->fillRect(12, 215, 216, 2, foreground);
            _tft->drawText(12, 224, "MOCHI CLOCK", foreground,
                           background, FONT_SMALL);
            const int16_t statusX = CFG_DISPLAY_WIDTH -
                                    _tft->getTextWidth("HOLIDAY", FONT_SMALL) - 12;
            _tft->drawText(statusX, 224, "HOLIDAY", foreground,
                           background, FONT_SMALL);
        } else {
            _tft->drawText(12, 150, weekdayText, foreground,
                           background, FONT_LARGE);
            _tft->drawText(12, 181, dateText, foreground,
                           background, FONT_SMALL);
            _tft->fillRect(12, 207, 216, 2, foreground);
            _tft->drawText(12, 221, "MOCHI CLOCK", foreground,
                           background, FONT_SMALL);
            const char* statusText = timeValid ? "TIME SYNCED" : "SYNCING TIME";
            const int16_t statusX = CFG_DISPLAY_WIDTH -
                                    _tft->getTextWidth(statusText, FONT_SMALL) - 12;
            _tft->drawText(statusX, 221, statusText, foreground,
                           background, FONT_SMALL);
        }

        strncpy(_lastClockLayoutKey, layoutKey,
                sizeof(_lastClockLayoutKey) - 1);
        _lastClockLayoutKey[sizeof(_lastClockLayoutKey) - 1] = '\0';
        _lastTimeText[0] = '\0';
        _lastHintText[0] = '\0';
        _timeViewDirty = false;
        _timeViewLayoutDrawn = true;
    }

    const int16_t timeY = 58;
    const int16_t secondsY = 115;
    const char* timeSlotText = timeValid ? "88:88" : timeText;
    const char* secondsSlotText = timeValid ? "88" : secondText;
    const int16_t timeX = (CFG_DISPLAY_WIDTH -
                           _tft->getTextWidth(timeSlotText, 6)) / 2;
    const int16_t secondsLabelWidth =
        _tft->getTextWidth("SEC", FONT_SMALL);
    const int16_t secondsX = CFG_DISPLAY_WIDTH - secondsLabelWidth -
                             _tft->getTextWidth(" ", FONT_SMALL) -
                             _tft->getTextWidth(secondsSlotText, FONT_SMALL) - 12;

    if (strcmp(timeText, _lastTimeText) != 0) {
        if (_lastTimeText[0] == '\0' ||
            strlen(timeText) != strlen(_lastTimeText)) {
            _tft->drawText(timeX, timeY, timeText, foreground,
                           background, 6);
        } else {
            char prefix[8] = {0};
            for (size_t i = 0; timeText[i] != '\0'; ++i) {
                if (timeText[i] != _lastTimeText[i]) {
                    char glyph[2] = {timeText[i], '\0'};
                    prefix[i] = '\0';
                    const int16_t glyphX = timeX +
                        _tft->getTextWidth(prefix, 6);
                    _tft->drawText(glyphX, timeY, glyph, foreground,
                                   background, 6);
                }
                prefix[i] = timeText[i];
            }
        }
        strncpy(_lastTimeText, timeText, sizeof(_lastTimeText) - 1);
        _lastTimeText[sizeof(_lastTimeText) - 1] = '\0';
    }

    if (strcmp(secondText, _lastHintText) != 0) {
        if (_lastHintText[0] == '\0') {
            _tft->drawText(secondsX, secondsY, secondText, foreground,
                           background, FONT_SMALL);
        } else {
            char prefix[4] = {0};
            for (size_t i = 0; secondText[i] != '\0'; ++i) {
                if (secondText[i] != _lastHintText[i]) {
                    char glyph[2] = {secondText[i], '\0'};
                    prefix[i] = '\0';
                    const int16_t glyphX = secondsX +
                        _tft->getTextWidth(prefix, FONT_SMALL);
                    _tft->drawText(glyphX, secondsY, glyph, foreground,
                                   background, FONT_SMALL);
                }
                prefix[i] = secondText[i];
            }
        }
        strncpy(_lastHintText, secondText, sizeof(_lastHintText) - 1);
        _lastHintText[sizeof(_lastHintText) - 1] = '\0';
    }
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

void DisplayService::drawWeatherIcon(int weatherCode, int16_t x, int16_t y) {
    const bool isClear = weatherCode == 0;
    const bool isFog = weatherCode == 45 || weatherCode == 48;
    const bool isRain = (weatherCode >= 51 && weatherCode <= 67) ||
                        (weatherCode >= 80 && weatherCode <= 82);
    const bool isSnow = (weatherCode >= 71 && weatherCode <= 77) ||
                        (weatherCode >= 85 && weatherCode <= 86);
    const bool isStorm = weatherCode >= 95;

    if (isFog) {
        _tft->fillRect(x + 10, y + 24, 76, 6, _themeForeground);
        _tft->fillRect(x,      y + 42, 86, 6, _themeForeground);
        _tft->fillRect(x + 14, y + 60, 76, 6, _themeForeground);
        return;
    }

    // 仅晴天和多云间晴显示太阳；阴雨天气不再出现多余太阳。
    const bool showSun = isClear || weatherCode <= 2;
    if (showSun) {
        const int16_t sunX = isClear ? x + 50 : x + 72;
        const int16_t sunY = isClear ? y + 42 : y + 22;
        _tft->fillRect(sunX - 10, sunY - 15, 20, 30, _themeForeground);
        _tft->fillRect(sunX - 15, sunY - 10, 30, 20, _themeForeground);
        _tft->fillRect(sunX - 3,  sunY - 30, 6, 9, _themeForeground);
        _tft->fillRect(sunX - 3,  sunY + 21, 6, 9, _themeForeground);
        _tft->fillRect(sunX - 30, sunY - 3, 9, 6, _themeForeground);
        _tft->fillRect(sunX + 21, sunY - 3, 9, 6, _themeForeground);
    }

    if (isClear) return;

    // 云朵改为连贯的实心阶梯剪影，避免空心轮廓在小屏上变形。
    _tft->fillRect(x + 8,  y + 50, 84, 24, _themeForeground);
    _tft->fillRect(x + 18, y + 40, 64, 10, _themeForeground);
    _tft->fillRect(x + 30, y + 30, 38, 10, _themeForeground);
    _tft->fillRect(x + 40, y + 24, 20, 6, _themeForeground);

    if (isStorm) {
        _tft->fillRect(x + 48, y + 78, 10, 8, _themeForeground);
        _tft->fillRect(x + 42, y + 86, 10, 8, _themeForeground);
        _tft->fillRect(x + 36, y + 94, 10, 6, _themeForeground);
    } else if (isRain) {
        // 三条错位雨滴，避免看起来像云朵的支脚。
        _tft->fillRect(x + 24, y + 82, 6, 10, _themeForeground);
        _tft->fillRect(x + 45, y + 78, 6, 10, _themeForeground);
        _tft->fillRect(x + 66, y + 82, 6, 10, _themeForeground);
    } else if (isSnow) {
        _tft->fillRect(x + 26, y + 84, 6, 6, _themeForeground);
        _tft->fillRect(x + 50, y + 90, 6, 6, _themeForeground);
        _tft->fillRect(x + 74, y + 84, 6, 6, _themeForeground);
    }
}

void DisplayService::drawWeatherView() {
    _tft->fillScreen(COLOR_ORANGE);

    if (!_weatherService || !_weatherService->isValid()) {
        _tft->drawTextCentered(82, "WEATHER", _themeForeground,
                               COLOR_ORANGE, FONT_LARGE);
        const char* message = (_weatherService && _weatherService->isLoading())
            ? "LOCATING..."
            : "WAITING FOR NETWORK";
        _tft->drawTextCentered(132, message, _themeForeground,
                               COLOR_ORANGE, FONT_SMALL);
        _tft->fillRect(8, 198, CFG_DISPLAY_WIDTH - 16, 3,
                       _themeForeground);
        _tft->drawTextCentered(216, "IP LOCATION + OPEN-METEO",
                               _themeForeground, COLOR_ORANGE, FONT_SMALL);
        return;
    }

    // 方案 1 / Temperature Hero：温度是绝对主视觉。
    char temp[8];
    snprintf(temp, sizeof(temp), "%d", _weatherService->getTemperature());
    const uint8_t tempSize = strlen(temp) <= 2 ? 9 : 7;
    _tft->drawText(8, 14, temp, _themeForeground, COLOR_ORANGE, tempSize);
    const int16_t degreeX =
        8 + _tft->getTextWidth(temp, tempSize) + 6;
    _tft->fillRect(degreeX, 18, 18, 18, _themeForeground);
    _tft->fillRect(degreeX + 5, 23, 8, 8, COLOR_ORANGE);

    drawWeatherIcon(_weatherService->getWeatherCode(), 128, 78);

    // 以细分隔线组织两列天气数据，不增加突兀的底色区块。
    _tft->fillRect(8, 184, CFG_DISPLAY_WIDTH - 16, 3, _themeForeground);
    _tft->fillRect(CFG_DISPLAY_WIDTH / 2 - 1, 196, 3, 36,
                   _themeForeground);

    char city[19];
    strncpy(city, _weatherService->getCity(), sizeof(city) - 1);
    city[sizeof(city) - 1] = '\0';
    _tft->drawText(8, 198, city, _themeForeground, COLOR_ORANGE, FONT_SMALL);

    const char* weekdays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    char date[12] = "--- --/--";
    if (_timeService && _timeService->getEpoch() > 1000000000UL) {
        const time_t now = static_cast<time_t>(_timeService->getEpoch());
        const struct tm* current = localtime(&now);
        snprintf(date, sizeof(date), "%s %02d/%02d",
                 weekdays[current->tm_wday], current->tm_mon + 1, current->tm_mday);
    }
    const int16_t dateX = CFG_DISPLAY_WIDTH - _tft->getTextWidth(date, FONT_SMALL) - 8;
    _tft->drawText(dateX, 198, date, _themeForeground,
                   COLOR_ORANGE, FONT_SMALL);

    char humidity[12];
    snprintf(humidity, sizeof(humidity), "HUM %d%%", _weatherService->getHumidity());
    _tft->drawText(8, 220, humidity, _themeForeground,
                   COLOR_ORANGE, FONT_SMALL);

    char highLow[18];
    snprintf(highLow, sizeof(highLow), "H%d / L%d",
             _weatherService->getHighTemperature(),
             _weatherService->getLowTemperature());
    const int16_t highLowX = CFG_DISPLAY_WIDTH - _tft->getTextWidth(highLow, FONT_SMALL) - 8;
    _tft->drawText(highLowX, 220, highLow, _themeForeground,
                   COLOR_ORANGE, FONT_SMALL);
}

void DisplayService::formatCryptoPrice(float price, char* output, size_t size) {
    if (!output || size == 0) return;
    if (price >= 1000000.0f) {
        snprintf(output, size, "$%.2fM", price / 1000000.0f);
    } else if (price >= 100000.0f) {
        snprintf(output, size, "$%.0fK", price / 1000.0f);
    } else if (price >= 10000.0f) {
        snprintf(output, size, "$%.1fK", price / 1000.0f);
    } else if (price >= 1000.0f) {
        snprintf(output, size, "$%.0f", price);
    } else if (price >= 100.0f) {
        snprintf(output, size, "$%.1f", price);
    } else if (price >= 1.0f) {
        snprintf(output, size, "$%.2f", price);
    } else if (price >= 0.01f) {
        snprintf(output, size, "$%.4f", price);
    } else {
        snprintf(output, size, "$%.6f", price);
    }
}

void DisplayService::drawCryptoView() {
    _tft->fillScreen(COLOR_ORANGE);

    auto drawBold = [this](int16_t x, int16_t y, const char* text,
                           uint8_t size) {
        _tft->drawText(x, y, text, _themeForeground, COLOR_ORANGE, size);
    };

    drawBold(8, 8, "CRYPTO", FONT_MEDIUM);
    if (_timeService && _timeService->isSynced() && _cryptoService &&
        _cryptoService->getLastSuccessMs() != 0) {
        const unsigned long ageSec =
            (millis() - _cryptoService->getLastSuccessMs()) / 1000UL;
        const time_t updatedEpoch =
            static_cast<time_t>(_timeService->getEpoch() - ageSec);
        const struct tm* updated = localtime(&updatedEpoch);
        char updatedText[16];
        snprintf(updatedText, sizeof(updatedText), "UPDATED %02d:%02d",
                 updated->tm_hour, updated->tm_min);
        const int16_t updatedX = CFG_DISPLAY_WIDTH -
            _tft->getTextWidth(updatedText, FONT_SMALL) - 8;
        drawBold(updatedX, 12, updatedText, FONT_SMALL);
    }
    _tft->fillRect(0, 30, CFG_DISPLAY_WIDTH, 2, _themeForeground);

    if (!_cryptoService || _cryptoService->getAssetCount() == 0) {
        const char* emptyText = "NO ASSETS";
        const int16_t emptyX = (CFG_DISPLAY_WIDTH -
            _tft->getTextWidth(emptyText, FONT_MEDIUM)) / 2;
        drawBold(emptyX, 116, emptyText, FONT_MEDIUM);
        return;
    }

    const uint8_t count = _cryptoService->getAssetCount();
    const int16_t listTop = 33;
    const int16_t listHeight = CFG_DISPLAY_HEIGHT - listTop;
    for (uint8_t i = 0; i < count; i++) {
        const int16_t rowTop = listTop + (listHeight * i) / count;
        const int16_t rowBottom = listTop + (listHeight * (i + 1)) / count;
        const int16_t rowHeight = rowBottom - rowTop;
        const CryptoAsset& asset = _cryptoService->getAsset(i);

        if (i > 0) {
            _tft->fillRect(0, rowTop, CFG_DISPLAY_WIDTH, 1,
                           _themeForeground);
        }
        const int16_t centeredOffset = (rowHeight - 16) / 2;
        const int16_t textY = rowTop + (centeredOffset > 3 ? centeredOffset : 3);
        const uint8_t symbolSize = strlen(asset.symbol) <= 4 ? FONT_MEDIUM : FONT_SMALL;
        drawBold(8, textY + (symbolSize == FONT_SMALL ? 4 : 0),
                 asset.symbol, symbolSize);

        char price[14] = "--";
        if (asset.priceValid) formatCryptoPrice(asset.price, price, sizeof(price));
        const uint8_t priceSize = strlen(price) <= 8 ? FONT_MEDIUM : FONT_SMALL;
        drawBold(66, textY + (priceSize == FONT_SMALL ? 4 : 0),
                 price, priceSize);

        char change[12] = "--";
        if (asset.changeValid) {
            snprintf(change, sizeof(change), "%+.1f%%", asset.change24h);
        }
        const int16_t changeX = CFG_DISPLAY_WIDTH -
            _tft->getTextWidth(change, FONT_MEDIUM) - 8;
        drawBold(changeX, textY, change, FONT_MEDIUM);
    }
}

void DisplayService::formatMarketPrice(float price, char* output, size_t size) {
    if (!output || size == 0) return;
    if (price >= 10000.0f) {
        snprintf(output, size, "%.0f", price);
    } else if (price >= 1000.0f) {
        snprintf(output, size, "%.1f", price);
    } else {
        snprintf(output, size, "%.2f", price);
    }
}

void DisplayService::drawMarketView() {
    _tft->fillScreen(COLOR_ORANGE);

    auto drawBold = [this](int16_t x, int16_t y, const char* text,
                           uint8_t size) {
        _tft->drawText(x, y, text, _themeForeground, COLOR_ORANGE, size);
    };

    drawBold(8, 8, "MARKET", FONT_MEDIUM);
    if (_timeService && _timeService->isSynced() && _marketService &&
        _marketService->getLastSuccessMs() != 0) {
        const unsigned long ageSec =
            (millis() - _marketService->getLastSuccessMs()) / 1000UL;
        const time_t updatedEpoch =
            static_cast<time_t>(_timeService->getEpoch() - ageSec);
        const struct tm* updated = localtime(&updatedEpoch);
        char updatedText[16];
        snprintf(updatedText, sizeof(updatedText), "UPDATED %02d:%02d",
                 updated->tm_hour, updated->tm_min);
        const int16_t updatedX = CFG_DISPLAY_WIDTH -
            _tft->getTextWidth(updatedText, FONT_SMALL) - 8;
        drawBold(updatedX, 12, updatedText, FONT_SMALL);
    }
    _tft->fillRect(0, 30, CFG_DISPLAY_WIDTH, 2, _themeForeground);

    if (!_marketService || _marketService->getAssetCount() == 0) {
        const char* emptyText = "NO STOCKS";
        const int16_t emptyX = (CFG_DISPLAY_WIDTH -
            _tft->getTextWidth(emptyText, FONT_MEDIUM)) / 2;
        drawBold(emptyX, 116, emptyText, FONT_MEDIUM);
        return;
    }

    const uint8_t count = _marketService->getAssetCount();
    const int16_t listTop = 33;
    const int16_t listHeight = CFG_DISPLAY_HEIGHT - listTop;
    for (uint8_t i = 0; i < count; i++) {
        const int16_t rowTop = listTop + (listHeight * i) / count;
        const int16_t rowBottom = listTop + (listHeight * (i + 1)) / count;
        const int16_t rowHeight = rowBottom - rowTop;
        const MarketAsset& asset = _marketService->getAsset(i);

        if (i > 0) {
            _tft->fillRect(0, rowTop, CFG_DISPLAY_WIDTH, 1,
                           _themeForeground);
        }
        const int16_t centeredOffset = (rowHeight - 16) / 2;
        const int16_t textY = rowTop + (centeredOffset > 3 ? centeredOffset : 3);
        const uint8_t labelSize = strlen(asset.label) <= 4 ?
            FONT_MEDIUM : FONT_SMALL;
        drawBold(8, textY + (labelSize == FONT_SMALL ? 4 : 0),
                 asset.label, labelSize);

        char price[12] = "--";
        if (asset.priceValid) {
            formatMarketPrice(asset.price, price, sizeof(price));
        }
        const uint8_t priceSize = strlen(price) <= 7 ? FONT_MEDIUM : FONT_SMALL;
        drawBold(66, textY + (priceSize == FONT_SMALL ? 4 : 0),
                 price, priceSize);

        char change[12] = "--";
        if (asset.changeValid) {
            snprintf(change, sizeof(change), "%+.1f%%", asset.changePercent);
        }
        const int16_t changeX = CFG_DISPLAY_WIDTH -
            _tft->getTextWidth(change, FONT_MEDIUM) - 8;
        drawBold(changeX, textY, change, FONT_MEDIUM);
    }
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
void DisplayService::drawThinking(uint8_t dotCount) {
    _tft->fillScreen(_animBgColor);
    const int16_t lx = eyeLX(0), rx = eyeRX(0);
    const int16_t ey = eyeY(), cy = eyeCY();
    _tft->fillRect(lx, ey, EYE_W, EYE_H, COLOR_EYES);
    _tft->fillRect(lx + EYE_W/2 - 3, cy - 3, 6, 6, _animBgColor);
    _tft->fillRect(rx, ey, EYE_W, EYE_H, COLOR_EYES);
    _tft->fillRect(rx + EYE_W - 10, ey + 6, 6, 6, _animBgColor);
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
    if (blinkLeft) {
        _tft->fillRect(lx, cy - 5, EYE_W, 10, COLOR_EYES);
    } else {
        _tft->fillRect(lx, ey, EYE_W, EYE_H, COLOR_EYES);
        _tft->fillRect(lx + EYE_W/2 - 3, cy + 10, 6, 6, _animBgColor);
    }
    if (blinkRight) {
        _tft->fillRect(rx, cy - 5, EYE_W, 10, COLOR_EYES);
    } else {
        _tft->fillRect(rx, ey, EYE_W, EYE_H, COLOR_EYES);
        _tft->fillRect(rx + EYE_W/2 - 3, cy + 10, 6, 6, _animBgColor);
    }
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
    const String line1 = _preferenceService
        ? _preferenceService->getBootLine1() : "HELLO";
    const String line2 = _preferenceService
        ? _preferenceService->getBootLine2() : "MOCHI";
    BootAnimation::run(*_tft, line1, line2);
    _busy = false;
}

// ── Interactive mode ───────────────────────────────────────────
void DisplayService::enterInteractive() {
    _interactiveActive = false;
    _currentMode = DisplayMode::EXPRESSION;
    _interactiveView = InteractiveView::EYES_NORMAL;
    showExpression(_renderedExpression);
}

void DisplayService::exitInteractive() {
    releaseArcadeGame();
    releaseMediaBuffer();
    _mediaActive = false;
    releaseSalaryCounterIfIdle(VIEW_EYES_NORMAL);
    _interactiveActive = false;
    _termMode = false;
    _currentMode = DisplayMode::EXPRESSION;
    _interactiveView = InteractiveView::EYES_NORMAL;
    _tft->clear(COLOR_BLACK);
    showExpression(_renderedExpression);
}

void DisplayService::setInteractiveView(uint8_t view) {
    if (_mediaActive && view != VIEW_MEDIA) {
        releaseMediaBuffer();
        _mediaActive = false;
    }
    const char* requestedGame = slugForArcadeView(view);
    if (requestedGame &&
        (!_activeArcadeGame ||
         view != viewForArcadeGame(_activeArcadeGame->id()))) {
        startArcadeGame(requestedGame);
        return;
    }
    if (_activeArcadeGame &&
        view != viewForArcadeGame(_activeArcadeGame->id())) {
        releaseArcadeGame();
    }
    releaseSalaryCounterIfIdle(view);
    if (!_interactiveActive) {
        _interactiveActive = true;
    }
    _currentMode = DisplayMode::INTERACTIVE;
    _interactiveView = static_cast<InteractiveView>(view);
    if (view != VIEW_EYES_NORMAL && view != VIEW_EYES_SQUISH) {
        _expressionPreferred = false;
    }
    _termMode = false;
    if (_carouselEnabled && isCarouselView(view) && !_carouselSuspended) {
        syncCarouselIndexForView(view);
        _carouselPageStartedMs = millis();
    }
    switch (_interactiveView) {
        case InteractiveView::EYES_NORMAL:
            setExpression(ExpressionId::NORMAL);
            break;
        case InteractiveView::EYES_SQUISH:
            setExpression(ExpressionId::HAPPY);
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
        case InteractiveView::WEATHER:
            // 轮播只展示已有缓存；刷新由服务自身的定时后台任务负责，
            // 不能因每次翻页触发新的网络请求。
            drawWeatherView();
            break;
        case InteractiveView::CRYPTO:
            drawCryptoView();
            break;
        case InteractiveView::MARKET:
            drawMarketView();
            break;
        case InteractiveView::SALARY_COUNTER:
            showSalaryCounter();
            break;
        case InteractiveView::TIMETABLE:
            _timetableLayoutDrawn = false;
            drawTimetableView();
            break;
        case InteractiveView::STATS:
            showStatsView();
            break;
        case InteractiveView::MEDIA:
            // 画面由上传回调逐行更新，这里不清屏。
            break;
        case InteractiveView::DINO_GAME:
        case InteractiveView::SOKOBAN_GAME:
        case InteractiveView::TETRIS_GAME:
        case InteractiveView::SNAKE_GAME:
        case InteractiveView::GAME_2048:
        case InteractiveView::BREAKOUT_GAME:
            // 游戏对象及其渲染缓冲由 startArcadeGame() 按需创建。
            break;
    }
}

void DisplayService::redrawCurrentView() {
    switch (_interactiveView) {
        case InteractiveView::EYES_NORMAL:
        case InteractiveView::EYES_SQUISH: _eyesView.redraw(); break;
        case InteractiveView::CODE_VIEW:   drawCodeView();   break;
        case InteractiveView::DRAW:        _tft->fillScreen(_drawBgColor); break;
        case InteractiveView::THINKING:    drawThinking(); break;
        case InteractiveView::WORKING:     drawWorking();  break;
        case InteractiveView::CLOCK:       drawClockView(); break;
        case InteractiveView::POMODORO:    drawPomodoroView(); break;
        case InteractiveView::WEATHER:     drawWeatherView(); break;
        case InteractiveView::CRYPTO:      drawCryptoView(); break;
        case InteractiveView::MARKET:      drawMarketView(); break;
        case InteractiveView::SALARY_COUNTER: drawSalaryCounterView(); break;
        case InteractiveView::STATS:
            drawStatsLayout();
            drawStatsView();
            break;
        case InteractiveView::TIMETABLE:
            _timetableLayoutDrawn = false;
            drawTimetableView();
            break;
        case InteractiveView::MEDIA:
            // TFT GRAM 已保留最后一帧，无需重画。
            break;
        case InteractiveView::DINO_GAME:
        case InteractiveView::SOKOBAN_GAME:
        case InteractiveView::TETRIS_GAME:
        case InteractiveView::SNAKE_GAME:
        case InteractiveView::GAME_2048:
        case InteractiveView::BREAKOUT_GAME:
            if (_activeArcadeGame) _activeArcadeGame->redraw();
            break;
    }
}

void DisplayService::startDinoGame() {
    startArcadeGame("dino");
}

void DisplayService::dinoJump() {
    if (isDinoGameActive()) {
        static_cast<DinoGame*>(_activeArcadeGame)->jump();
    }
}

void DisplayService::restartDinoGame() {
    if (isDinoGameActive()) {
        static_cast<DinoGame*>(_activeArcadeGame)->restart();
    }
}

void DisplayService::exitDinoGame() {
    if (isDinoGameActive()) exitArcadeGame();
}

bool DisplayService::isDinoGameActive() const {
    return _currentMode == DisplayMode::INTERACTIVE &&
           _interactiveView == InteractiveView::DINO_GAME &&
           _activeArcadeGame &&
           _activeArcadeGame->id() == ArcadeGameId::DINO &&
           _activeArcadeGame->isActive();
}

String DisplayService::getDinoGameStateJson() const {
    if (isDinoGameActive()) return _activeArcadeGame->getStateJson();
    return "{\"id\":\"dino\",\"active\":false,\"state\":\"closed\"}";
}

void DisplayService::startSokobanGame() {
    startArcadeGame("sokoban");
}

bool DisplayService::moveSokoban(int8_t dx, int8_t dy) {
    return isSokobanGameActive() &&
           static_cast<SokobanGame*>(_activeArcadeGame)->move(dx, dy);
}

bool DisplayService::undoSokoban() {
    return isSokobanGameActive() &&
           static_cast<SokobanGame*>(_activeArcadeGame)->undo();
}

void DisplayService::restartSokoban() {
    if (isSokobanGameActive()) {
        static_cast<SokobanGame*>(_activeArcadeGame)->restart();
    }
}

bool DisplayService::selectSokobanLevel(uint8_t level) {
    return isSokobanGameActive() &&
           static_cast<SokobanGame*>(_activeArcadeGame)->selectLevel(level);
}

void DisplayService::exitSokobanGame() {
    if (isSokobanGameActive()) exitArcadeGame();
}

IArcadeGame* DisplayService::createArcadeGame(const String& slug) {
    if (!isKnownArcadeGame(slug)) return nullptr;

    if (slug == "dino" || slug == "sokoban") {
        _monoGameBuffer =
            new (std::nothrow) uint8_t[GameRenderBuffer::MONO_FRAME_BYTES];
        if (!_monoGameBuffer) return nullptr;
        IArcadeGame* game = slug == "dino"
            ? static_cast<IArcadeGame*>(
                new (std::nothrow) DinoGame(
                    _tft, _preferenceService, _monoGameBuffer))
            : static_cast<IArcadeGame*>(
                new (std::nothrow) SokobanGame(
                    _tft, _preferenceService, _monoGameBuffer));
        if (!game) {
            delete[] _monoGameBuffer;
            _monoGameBuffer = nullptr;
        }
        return game;
    }

    _arcadeCanvas = new (std::nothrow) ArcadeCanvas(_tft);
    if (!_arcadeCanvas) return nullptr;

    IArcadeGame* game = nullptr;
    if (slug == "tetris") {
        game = new (std::nothrow) TetrisGame(
            _arcadeCanvas, _preferenceService);
    } else if (slug == "snake") {
        game = new (std::nothrow) SnakeGame(
            _arcadeCanvas, _preferenceService);
    } else if (slug == "2048") {
        game = new (std::nothrow) Game2048(
            _arcadeCanvas, _preferenceService);
    } else if (slug == "breakout") {
        game = new (std::nothrow) BreakoutGame(
            _arcadeCanvas, _preferenceService);
    }
    if (!game) {
        delete _arcadeCanvas;
        _arcadeCanvas = nullptr;
    }
    return game;
}

void DisplayService::releaseArcadeGame() {
    const bool hadAllocation =
        _activeArcadeGame || _arcadeCanvas || _monoGameBuffer;
    if (_activeArcadeGame) {
        _activeArcadeGame->stop();
        delete _activeArcadeGame;
        _activeArcadeGame = nullptr;
    }
    delete _arcadeCanvas;
    _arcadeCanvas = nullptr;
    delete[] _monoGameBuffer;
    _monoGameBuffer = nullptr;
    if (hadAllocation) MemoryMonitor::logSnapshot("game released");
}

bool DisplayService::isKnownArcadeGame(const String& slug) const {
    return slug == "dino" || slug == "sokoban" ||
           slug == "tetris" || slug == "snake" ||
           slug == "2048" || slug == "breakout";
}

uint8_t DisplayService::viewForArcadeGame(ArcadeGameId id) const {
    switch (id) {
        case ArcadeGameId::DINO: return VIEW_DINO;
        case ArcadeGameId::SOKOBAN: return VIEW_SOKOBAN;
        case ArcadeGameId::TETRIS: return VIEW_TETRIS;
        case ArcadeGameId::SNAKE: return VIEW_SNAKE;
        case ArcadeGameId::MERGE_2048: return VIEW_2048;
        case ArcadeGameId::BREAKOUT: return VIEW_BREAKOUT;
        default: return VIEW_DINO;
    }
}

const char* DisplayService::slugForArcadeView(uint8_t view) const {
    switch (static_cast<InteractiveView>(view)) {
        case InteractiveView::DINO_GAME: return "dino";
        case InteractiveView::SOKOBAN_GAME: return "sokoban";
        case InteractiveView::TETRIS_GAME: return "tetris";
        case InteractiveView::SNAKE_GAME: return "snake";
        case InteractiveView::GAME_2048: return "2048";
        case InteractiveView::BREAKOUT_GAME: return "breakout";
        default: return nullptr;
    }
}

bool DisplayService::startArcadeGame(const String& slug) {
    if (_activeArcadeGame && slug == _activeArcadeGame->slug()) {
        _activeArcadeGame->begin();
        return true;
    }
    releaseArcadeGame();
    IArcadeGame* game = createArcadeGame(slug);
    if (!game) {
        LOG_ERROR("Game", "按需加载失败: %s", slug.c_str());
        return false;
    }
    _activeArcadeGame = game;
    setInteractiveView(viewForArcadeGame(game->id()));
    game->begin();
    MemoryMonitor::logSnapshot("game loaded");
    return true;
}

bool DisplayService::handleArcadeAction(const String& action, int value) {
    return isGameActive() &&
           _activeArcadeGame->handleAction(action, value);
}

void DisplayService::exitArcadeGame() {
    if (!_activeArcadeGame) return;
    releaseArcadeGame();
    restoreAfterExclusiveView();
}

void DisplayService::restoreAfterExclusiveView() {
    const auto status = _ccService->getStatus();
    const bool codexActive =
        status == ClaudeCodeService::Status::THINKING ||
        status == ClaudeCodeService::Status::WORKING ||
        status == ClaudeCodeService::Status::PERMISSION ||
        status == ClaudeCodeService::Status::SWEEPING ||
        status == ClaudeCodeService::Status::SLEEPING;
    if (_claudeStatusEnabled && codexActive) switchToInfoMode();
    else switchToIdleDisplay();
}

bool DisplayService::beginMediaFrame(uint16_t x, uint16_t y,
                                     uint16_t width, uint16_t height) {
    if (width == 0 || height == 0 || x + width > CFG_DISPLAY_WIDTH ||
        y + height > CFG_DISPLAY_HEIGHT) {
        return false;
    }
    releaseArcadeGame();
    releaseSalaryCounterIfIdle(VIEW_MEDIA);
    if (!_mediaRowBuffer) {
        _mediaRowBuffer = new (std::nothrow) uint16_t[
            MEDIA_ROW_BUFFER_BYTES / sizeof(uint16_t)];
        if (!_mediaRowBuffer) {
            LOG_ERROR("Media", "行缓冲分配失败");
            return false;
        }
        MemoryMonitor::logSnapshot("media loaded");
    }

    _interactiveActive = true;
    _currentMode = DisplayMode::INTERACTIVE;
    _interactiveView = InteractiveView::MEDIA;
    _expressionPreferred = false;
    _termMode = false;
    _mediaActive = true;
    _mediaFrameReceiving = true;
    _mediaRow = 0;
    _mediaColumn = 0;
    _mediaX = x;
    _mediaY = y;
    _mediaWidth = width;
    _mediaHeight = height;
    _mediaHighByte = 0;
    _mediaHasHighByte = false;
    return true;
}

bool DisplayService::writeMediaFrameBytes(const uint8_t* data, size_t length) {
    if (!_mediaFrameReceiving || !_mediaRowBuffer || !data) return false;

    for (size_t i = 0; i < length; i++) {
        if (!_mediaHasHighByte) {
            _mediaHighByte = data[i];
            _mediaHasHighByte = true;
            continue;
        }

        if (_mediaRow >= _mediaHeight) {
            _mediaFrameReceiving = false;
            return false;
        }
        _mediaRowBuffer[_mediaColumn++] =
            static_cast<uint16_t>(_mediaHighByte) << 8 | data[i];
        _mediaHasHighByte = false;

        if (_mediaColumn == _mediaWidth) {
            _tft->pushRgb565Row(_mediaX, _mediaY + _mediaRow,
                                _mediaRowBuffer, _mediaWidth);
            _mediaColumn = 0;
            _mediaRow++;
        }
    }
    return true;
}

bool DisplayService::finishMediaFrame() {
    const bool complete = _mediaFrameReceiving && !_mediaHasHighByte &&
                          _mediaRow == _mediaHeight &&
                          _mediaColumn == 0;
    _mediaFrameReceiving = false;
    if (!complete) {
        LOG_WARN("Media", "媒体帧不完整 row=%u col=%u pending=%u",
                 static_cast<unsigned int>(_mediaRow),
                 static_cast<unsigned int>(_mediaColumn),
                 _mediaHasHighByte ? 1U : 0U);
    }
    return complete;
}

void DisplayService::abortMediaFrame() {
    _mediaFrameReceiving = false;
    _mediaHasHighByte = false;
    _mediaRow = 0;
    _mediaColumn = 0;
}

void DisplayService::releaseMediaBuffer() {
    const bool hadResources = _mediaRowBuffer != nullptr ||
                              _mediaFile != nullptr || _mediaGif != nullptr;
    if (_mediaGif) {
        _mediaGif->close();
        delete _mediaGif;
        _mediaGif = nullptr;
    }
    if (_mediaFile) {
        _mediaFile->close();
        delete _mediaFile;
        _mediaFile = nullptr;
    }
    delete[] _mediaRowBuffer;
    _mediaRowBuffer = nullptr;
    _mediaFrameReceiving = false;
    _mediaHasHighByte = false;
    _mediaRow = 0;
    _mediaColumn = 0;
    _mediaGifPlaying = false;
    _mediaGifLoopPending = false;
    _mediaGifStripActive = false;
    if (s_mediaGifOwner == this) s_mediaGifOwner = nullptr;
    if (hadResources) MemoryMonitor::logSnapshot("media released");
}

void DisplayService::stopMedia() {
    if (!_mediaActive && !_mediaRowBuffer) return;
    releaseMediaBuffer();
    _mediaActive = false;
    restoreAfterExclusiveView();
}

void* DisplayService::openMediaGif(const char* path, int32_t* fileSize) {
    if (!s_mediaGifOwner || !fileSize) return nullptr;
    fs::File opened = LittleFS.open(path, "r");
    if (!opened) return nullptr;
    s_mediaGifOwner->_mediaFile = new (std::nothrow) fs::File(opened);
    if (!s_mediaGifOwner->_mediaFile) return nullptr;
    *fileSize = static_cast<int32_t>(s_mediaGifOwner->_mediaFile->size());
    return s_mediaGifOwner->_mediaFile;
}

void DisplayService::closeMediaGif(void* handle) {
    fs::File* file = static_cast<fs::File*>(handle);
    if (file) file->close();
}

int32_t DisplayService::readMediaGif(gif_file_tag* gifFile, uint8_t* data,
                                     int32_t length) {
    if (!gifFile || !gifFile->fHandle || length <= 0) return 0;
    fs::File* file = static_cast<fs::File*>(gifFile->fHandle);
    const int32_t remaining = gifFile->iSize - gifFile->iPos;
    if (length > remaining) length = remaining;
    if (length <= 0) return 0;
    const int32_t read = static_cast<int32_t>(file->read(data, length));
    gifFile->iPos = static_cast<int32_t>(file->position());
    return read;
}

int32_t DisplayService::seekMediaGif(gif_file_tag* gifFile,
                                     int32_t position) {
    if (!gifFile || !gifFile->fHandle || position < 0 ||
        position > gifFile->iSize) return -1;
    fs::File* file = static_cast<fs::File*>(gifFile->fHandle);
    if (!file->seek(position)) return -1;
    gifFile->iPos = static_cast<int32_t>(file->position());
    return gifFile->iPos;
}

bool DisplayService::flushMediaGifStrip() {
    if (!_mediaGifStripActive || !_mediaRowBuffer) return true;
    _tft->pushRgb565Rect(_mediaGifStripX, _mediaGifStripY,
                         _mediaGifStripWidth, _mediaGifStripRows,
                         _mediaRowBuffer);
    _mediaGifStripActive = false;
    _mediaGifStripRows = 0;
    return true;
}

void DisplayService::drawMediaGif(gif_draw_tag* draw) {
    DisplayService* service = draw
        ? static_cast<DisplayService*>(draw->pUser) : nullptr;
    if (!service || !service->_mediaRowBuffer) return;
    uint8_t* source = draw->pPixels;
    uint16_t* palette = draw->pPalette;
    int width = min(draw->iWidth,
                    CFG_DISPLAY_WIDTH - draw->iX - service->_mediaGifOffsetX);
    const int screenX = service->_mediaGifOffsetX + draw->iX;
    const int screenY = service->_mediaGifOffsetY + draw->iY + draw->y;
    if (width <= 0 || screenX < 0 || screenY < 0 ||
        screenY >= CFG_DISPLAY_HEIGHT) return;

    if (draw->ucDisposalMethod == 2) {
        for (int x = 0; x < width; x++) {
            if (source[x] == draw->ucTransparent) {
                source[x] = draw->ucBackground;
            }
        }
        draw->ucHasTransparency = 0;
    }

    if (draw->ucHasTransparency) {
        service->flushMediaGifStrip();
        int x = 0;
        while (x < width) {
            while (x < width && source[x] == draw->ucTransparent) x++;
            const int runStart = x;
            while (x < width && source[x] != draw->ucTransparent) {
                service->_mediaRowBuffer[x - runStart] = palette[source[x]];
                x++;
            }
            if (x > runStart) {
                service->_tft->pushRgb565Row(screenX + runStart, screenY,
                                             service->_mediaRowBuffer,
                                             x - runStart);
            }
        }
        return;
    }

    const bool continuesStrip = service->_mediaGifStripActive &&
        service->_mediaGifStripX == screenX &&
        service->_mediaGifStripWidth == width &&
        service->_mediaGifStripY + service->_mediaGifStripRows == screenY &&
        service->_mediaGifStripRows < MEDIA_STRIP_ROWS;
    if (!continuesStrip) {
        service->flushMediaGifStrip();
        service->_mediaGifStripActive = true;
        service->_mediaGifStripX = screenX;
        service->_mediaGifStripY = screenY;
        service->_mediaGifStripWidth = width;
        service->_mediaGifStripRows = 0;
    }
    uint16_t* target = service->_mediaRowBuffer +
                       service->_mediaGifStripRows * width;
    for (int x = 0; x < width; x++) target[x] = palette[source[x]];
    service->_mediaGifStripRows++;
    if (service->_mediaGifStripRows == MEDIA_STRIP_ROWS ||
        draw->y == draw->iHeight - 1) {
        service->flushMediaGifStrip();
    }
}

bool DisplayService::startMediaGif(const char* path) {
    releaseMediaBuffer();
    releaseArcadeGame();
    releaseSalaryCounterIfIdle(VIEW_MEDIA);
    _mediaRowBuffer = new (std::nothrow) uint16_t[
        MEDIA_ROW_BUFFER_BYTES / sizeof(uint16_t)];
    _mediaGif = new (std::nothrow) AnimatedGIF();
    if (!_mediaRowBuffer || !_mediaGif) {
        releaseMediaBuffer();
        return false;
    }
    s_mediaGifOwner = this;
    // Adafruit_SPITFT::writePixels() receives host-endian RGB565 values and
    // performs the wire-order swap itself.
    _mediaGif->begin(LITTLE_ENDIAN_PIXELS);
    if (!_mediaGif->open(path, &DisplayService::openMediaGif,
                         &DisplayService::closeMediaGif,
                         &DisplayService::readMediaGif,
                         &DisplayService::seekMediaGif,
                         &DisplayService::drawMediaGif)) {
        releaseMediaBuffer();
        return false;
    }
    const int width = _mediaGif->getCanvasWidth();
    const int height = _mediaGif->getCanvasHeight();
    if (width <= 0 || height <= 0 || width > CFG_DISPLAY_WIDTH ||
        height > CFG_DISPLAY_HEIGHT) {
        releaseMediaBuffer();
        return false;
    }
    _mediaGifOffsetX = (CFG_DISPLAY_WIDTH - width) / 2;
    _mediaGifOffsetY = (CFG_DISPLAY_HEIGHT - height) / 2;
    _tft->fillScreen(COLOR_BLACK);
    _interactiveActive = true;
    _currentMode = DisplayMode::INTERACTIVE;
    _interactiveView = InteractiveView::MEDIA;
    _expressionPreferred = false;
    _termMode = false;
    _mediaActive = true;
    _mediaGifPlaying = true;
    _mediaNextFrameMs = millis();
    MemoryMonitor::logSnapshot("media GIF loaded");
    return true;
}

void DisplayService::updateMediaGif(unsigned long now) {
    if (!_mediaGifPlaying || !_mediaGif ||
        static_cast<long>(now - _mediaNextFrameMs) < 0) return;
    if (_mediaGifLoopPending) {
        _mediaGif->reset();
        _mediaGifLoopPending = false;
    }
    const unsigned long started = millis();
    int delayMs = 0;
    _tft->beginRgb565Batch();
    const int playResult = _mediaGif->playFrame(false, &delayMs, this);
    flushMediaGifStrip();
    _tft->endRgb565Batch();
    if (playResult < 0) {
        LOG_ERROR("Media", "GIF 帧解码失败");
        stopMedia();
        return;
    }
    _mediaLastRenderMs = millis() - started;
    _mediaRenderedFrames++;
    static constexpr unsigned long MIN_MEDIA_FRAME_MS = 20UL;
    const unsigned long frameDelay = max(
        MIN_MEDIA_FRAME_MS, static_cast<unsigned long>(delayMs));
    // 按 GIF 原始时间线推进；渲染耗时只消耗本帧预算，不应额外叠加到
    // 下一帧的延迟上。若设备已经落后，则从当前时刻重新同步，避免
    // 连续补帧造成闪屏，同时保持能达到的最高实际帧率。
    const unsigned long targetNextFrame = _mediaNextFrameMs + frameDelay;
    _mediaNextFrameMs = static_cast<long>(targetNextFrame - millis()) > 0
        ? targetNextFrame : millis();
    if (playResult == 0) _mediaGifLoopPending = true;
}

String DisplayService::getArcadeGameStateJson(const String& slug) const {
    if (_activeArcadeGame &&
        (slug.isEmpty() || slug == _activeArcadeGame->slug())) {
        return _activeArcadeGame->getStateJson();
    }
    if (!slug.isEmpty() && isKnownArcadeGame(slug)) {
        String json = "{\"id\":\"";
        json += slug;
        json += "\",\"active\":false,\"state\":\"closed\"}";
        return json;
    }
    return "{\"active\":false,\"state\":\"unavailable\"}";
}

const char* DisplayService::getActiveArcadeGameSlug() const {
    return _activeArcadeGame ? _activeArcadeGame->slug() : "";
}

bool DisplayService::isArcadeGameView() const {
    return _interactiveView >= InteractiveView::DINO_GAME &&
           _interactiveView <= InteractiveView::BREAKOUT_GAME;
}

bool DisplayService::isSokobanGameActive() const {
    return _currentMode == DisplayMode::INTERACTIVE &&
           _interactiveView == InteractiveView::SOKOBAN_GAME &&
           _activeArcadeGame &&
           _activeArcadeGame->id() == ArcadeGameId::SOKOBAN &&
           _activeArcadeGame->isActive();
}

bool DisplayService::isGameActive() const {
    return _currentMode == DisplayMode::INTERACTIVE &&
           isArcadeGameView() && _activeArcadeGame &&
           _activeArcadeGame->isActive();
}

String DisplayService::getSokobanStateJson() const {
    if (isSokobanGameActive()) return _activeArcadeGame->getStateJson();
    return "{\"id\":\"sokoban\",\"active\":false,\"state\":\"closed\"}";
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
    if (!_interactiveActive) {
        _interactiveActive = true;
        _currentMode = DisplayMode::INTERACTIVE;
    }
    _interactiveView = InteractiveView::CLOCK;
    _termMode = false;
    _lastClockRenderSec = 0;
    invalidateTimeView();
    drawClockView();
}

void DisplayService::showPomodoroReady() {
    if (!_interactiveActive) {
        _interactiveActive = true;
        _currentMode = DisplayMode::INTERACTIVE;
    }
    _interactiveView = InteractiveView::POMODORO;
    _termMode = false;
    _pomodoroRunning = false;
    _pomodoroPaused = false;
    _pomodoroPhase = PomodoroPhase::FOCUS;
    _pomodoroDurationSec = (uint32_t)_focusMinutes * 60UL;
    if (_pomodoroDurationSec == 0) _pomodoroDurationSec = 1;
    _pomodoroRemainingAtPauseSec = _pomodoroDurationSec;
    _lastClockRenderSec = 0;
    invalidateTimeView();
    drawPomodoroView();
}

void DisplayService::startPomodoro(PomodoroPhase phase) {
    if (!_interactiveActive) {
        _interactiveActive = true;
        _currentMode = DisplayMode::INTERACTIVE;
    }
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

SalaryCounterService* DisplayService::salaryCounter() {
    if (_salaryCounter) return _salaryCounter;

    _salaryCounter = new (std::nothrow) SalaryCounterService(_timeService);
    if (!_salaryCounter) {
        LOG_ERROR("Salary", "计薪模块内存分配失败");
        return nullptr;
    }
    _salaryCounter->init();
    if (_preferenceService && _timeService &&
        _preferenceService->getSalaryAutoEnabled() &&
        _timeService->isSynced()) {
        const uint32_t today =
            static_cast<uint32_t>(_timeService->getYear()) * 10000UL +
            static_cast<uint32_t>(_timeService->getMonth()) * 100UL +
            static_cast<uint32_t>(_timeService->getDay());
        _salaryCounter->rolloverToDate(today);
    }
    MemoryMonitor::logSnapshot("salary load");
    return _salaryCounter;
}

bool DisplayService::showSalaryCounter() {
    if (!salaryCounter()) return false;
    _interactiveActive = true;
    _currentMode = DisplayMode::INTERACTIVE;
    _interactiveView = InteractiveView::SALARY_COUNTER;
    _expressionPreferred = false;
    _termMode = false;
    _lastSalaryRenderMs = 0;
    _lastSalaryAmount[0] = '\0';
    _lastSalaryWorked[0] = '\0';
    _lastSalaryState[0] = '\0';
    _lastSalaryProgressPermille = 0xFFFF;
    _lastSalaryAmountX = 0;
    _lastSalaryAmountSize = 0;
    _lastSalaryWorkedX = 0;
    _lastSalaryWorkedSize = 0;
    drawSalaryCounterLayout();
    drawSalaryCounterView();
    return true;
}

void DisplayService::refreshSalaryCounter() {
    if (_currentMode == DisplayMode::INTERACTIVE &&
        _interactiveView == InteractiveView::SALARY_COUNTER) {
        drawSalaryCounterView();
    }
}

bool DisplayService::isSalarySessionActive() const {
    return _salaryCounter && _salaryCounter->isSessionActive();
}

void DisplayService::releaseSalaryCounterIfIdle(uint8_t nextView) {
    if (!_salaryCounter || nextView == VIEW_SALARY || nextView == VIEW_STATS ||
        _salaryCounter->isSessionActive()) {
        return;
    }
    delete _salaryCounter;
    _salaryCounter = nullptr;
    MemoryMonitor::logSnapshot("salary release");
}

void DisplayService::drawSalaryCounterLayout() {
    constexpr uint16_t foreground = COLOR_WHITE;
    _tft->fillScreen(COLOR_ORANGE);

    // 橙白账本式布局：静态内容只在进入页面时绘制一次。
    _tft->drawText(12, 8, "CNY TODAY", foreground, COLOR_ORANGE,
                   FONT_MEDIUM);
    _tft->fillRect(12, 30, 216, 2, foreground);

    _tft->drawText(14, 108, "TODAY'S EARNINGS", foreground,
                   COLOR_ORANGE, FONT_SMALL);

    _tft->fillRect(12, 124, 216, 1, foreground);
    _tft->drawText(192, 143, "WORKED", foreground, COLOR_ORANGE,
                   FONT_SMALL);

    char startText[6];
    char endText[6];
    const uint16_t startMinutes = _preferenceService
        ? _preferenceService->getSalaryStartMinutes() : 570;
    const uint16_t endMinutes = _preferenceService
        ? _preferenceService->getSalaryEndMinutes() : 1140;
    snprintf(startText, sizeof(startText), "%02u:%02u",
             startMinutes / 60, startMinutes % 60);
    snprintf(endText, sizeof(endText), "%02u:%02u",
             endMinutes / 60, endMinutes % 60);
    _tft->drawText(12, 168, startText, foreground, COLOR_ORANGE,
                   FONT_SMALL);
    const int16_t endX = 228 - _tft->getTextWidth(endText, FONT_SMALL);
    _tft->drawText(endX, 168, endText, foreground, COLOR_ORANGE,
                   FONT_SMALL);

    // 最简连续进度条：白色外框保持静态，内部按进度增量填充。
    _tft->drawRect(12, 182, 216, 8, foreground);
    _tft->fillRect(12, 207, 216, 1, foreground);
    _tft->drawTextCentered(219, "EVERY SECOND COUNTS", foreground,
                           COLOR_ORANGE, FONT_SMALL);
}

void DisplayService::drawSalaryCounterView() {
    SalaryCounterService* counter = salaryCounter();
    if (!counter) return;

    constexpr uint16_t foreground = COLOR_WHITE;
    const uint64_t earned = counter->getLiveEarnedTenThousandths();
    const uint64_t integerPart = earned / 10000ULL;
    char amountText[20];
    snprintf(amountText, sizeof(amountText), "%llu.%03llu",
             static_cast<unsigned long long>(integerPart),
             static_cast<unsigned long long>((earned % 10000ULL) / 10ULL));

    const uint32_t elapsed = counter->getActiveSeconds();
    char workedText[24];
    snprintf(workedText, sizeof(workedText), "%02lu:%02lu:%02lu",
             static_cast<unsigned long>(elapsed / 3600UL),
             static_cast<unsigned long>((elapsed / 60UL) % 60UL),
             static_cast<unsigned long>(elapsed % 60UL));

    const char* stateText = "READY";
    if (counter->isRunning()) stateText = "RUNNING";
    else if (counter->isPaused()) stateText = "PAUSED";
    else if (counter->getState() == SalaryCounterState::FINISHED) {
        stateText = "DONE";
    } else if (!counter->isConfigured()) {
        stateText = "SET PAY";
    }

    if (strcmp(amountText, _lastSalaryAmount) != 0) {
        const size_t length = strlen(amountText);
        const uint8_t amountSize = length <= 6 ? 5
            : (length <= 8 ? 4
            : (length <= 10 ? 3
            : (length <= 16 ? 2 : 1)));
        const int16_t amountWidth =
            _tft->getTextWidth(amountText, amountSize);
        const int16_t amountX = 32 + (196 - amountWidth) / 2;
        const bool layoutChanged =
            _lastSalaryAmount[0] == '\0' ||
            strlen(_lastSalaryAmount) != length ||
            _lastSalaryAmountX != amountX ||
            _lastSalaryAmountSize != amountSize;
        if (layoutChanged) {
            _tft->fillRect(12, 52, 228, 54, COLOR_ORANGE);
            // 符号跟随金额左边缘，位数变化时仍保持紧凑间距。
            const int16_t currencyX = amountX - 19;
            _tft->drawLine(currencyX, 56, currencyX + 4, 61, foreground);
            _tft->drawLine(currencyX + 8, 56, currencyX + 4, 61,
                           foreground);
            _tft->drawLine(currencyX + 4, 61, currencyX + 4, 69,
                           foreground);
            _tft->drawLine(currencyX + 1, 63, currencyX + 7, 63,
                           foreground);
            _tft->drawLine(currencyX + 1, 66, currencyX + 7, 66,
                           foreground);
            _tft->drawText(amountX, 58, amountText, foreground,
                           COLOR_ORANGE, amountSize);
        } else {
            const int16_t charWidth = _tft->getTextWidth("0", amountSize);
            char glyph[2] = {'\0', '\0'};
            for (size_t i = 0; i < length; i++) {
                if (_lastSalaryAmount[i] == amountText[i]) continue;
                glyph[0] = amountText[i];
                _tft->drawText(amountX + static_cast<int16_t>(i) * charWidth,
                               58, glyph, foreground, COLOR_ORANGE,
                               amountSize);
            }
        }
        strncpy(_lastSalaryAmount, amountText,
                sizeof(_lastSalaryAmount) - 1);
        _lastSalaryAmount[sizeof(_lastSalaryAmount) - 1] = '\0';
        _lastSalaryAmountX = amountX;
        _lastSalaryAmountSize = amountSize;
    }

    if (strcmp(workedText, _lastSalaryWorked) != 0) {
        constexpr uint8_t workedSize = FONT_LARGE;
        constexpr int16_t workedX = 14;
        const size_t workedLength = strlen(workedText);
        const bool layoutChanged =
            _lastSalaryWorked[0] == '\0' ||
            strlen(_lastSalaryWorked) != workedLength ||
            _lastSalaryWorkedX != workedX ||
            _lastSalaryWorkedSize != workedSize;
        if (layoutChanged) {
            _tft->fillRect(12, 132, 174, 28, COLOR_ORANGE);
            _tft->drawText(workedX, 136, workedText, foreground,
                           COLOR_ORANGE, workedSize);
        } else {
            const int16_t charWidth =
                _tft->getTextWidth("0", workedSize);
            char glyph[2] = {'\0', '\0'};
            for (size_t i = 0; i < workedLength; i++) {
                if (_lastSalaryWorked[i] == workedText[i]) continue;
                glyph[0] = workedText[i];
                _tft->drawText(
                    workedX + static_cast<int16_t>(i) * charWidth,
                    136, glyph, foreground, COLOR_ORANGE, workedSize);
            }
        }
        strncpy(_lastSalaryWorked, workedText,
                sizeof(_lastSalaryWorked) - 1);
        _lastSalaryWorked[sizeof(_lastSalaryWorked) - 1] = '\0';
        _lastSalaryWorkedX = workedX;
        _lastSalaryWorkedSize = workedSize;
    }

    const uint16_t progress = _preferenceService
        ? _preferenceService->getSalaryScheduleProgressPermille(_timeService)
        : counter->getProgressPermille();
    if (progress != _lastSalaryProgressPermille) {
        char progressText[6];
        snprintf(progressText, sizeof(progressText), "%u%%",
                 static_cast<unsigned>((progress + 5) / 10));
        _tft->fillRect(99, 164, 42, 13, COLOR_ORANGE);
        const int16_t progressX =
            (CFG_DISPLAY_WIDTH -
             _tft->getTextWidth(progressText, FONT_SMALL)) / 2;
        _tft->drawText(progressX, 168, progressText, foreground,
                       COLOR_ORANGE, FONT_SMALL);

        const int16_t previousWidth =
            _lastSalaryProgressPermille == 0xFFFF
                ? 0 : 214 * _lastSalaryProgressPermille / 1000;
        const int16_t fillWidth = 214 * progress / 1000;
        if (fillWidth > previousWidth) {
            _tft->fillRect(13 + previousWidth, 183,
                           fillWidth - previousWidth, 6, foreground);
        } else if (fillWidth < previousWidth) {
            _tft->fillRect(13 + fillWidth, 183,
                           previousWidth - fillWidth, 6, COLOR_ORANGE);
        }
        _lastSalaryProgressPermille = progress;
    }

    if (strcmp(stateText, _lastSalaryState) != 0) {
        _tft->fillRect(150, 8, 90, 20, COLOR_ORANGE);
        const int16_t stateX = CFG_DISPLAY_WIDTH -
            _tft->getTextWidth(stateText, FONT_SMALL) - 8;
        _tft->drawText(stateX, 12, stateText, foreground,
                       COLOR_ORANGE, FONT_SMALL);
        _tft->fillRect(stateX - 13, 12, 7, 7, foreground);

        strncpy(_lastSalaryState, stateText,
                sizeof(_lastSalaryState) - 1);
        _lastSalaryState[sizeof(_lastSalaryState) - 1] = '\0';
    }
}

// ============================================================
// Claude Code 会话/今日统计面板
// ============================================================

bool DisplayService::showStatsView() {
    _interactiveActive = true;
    _currentMode = DisplayMode::INTERACTIVE;
    _interactiveView = InteractiveView::STATS;
    _expressionPreferred = false;
    _termMode = false;
    _lastStatsRenderMs = 0;
    _lastStatsWorked[0] = '\0';
    _lastStatsEarned[0] = '\0';
    _lastStatsCounts[0] = '\0';
    _lastStatsLongest[0] = '\0';
    _lastStatsSession[0] = '\0';
    drawStatsLayout();
    drawStatsView();
    return true;
}

void DisplayService::drawStatsLayout() {
    constexpr uint16_t foreground = COLOR_WHITE;
    _tft->fillScreen(COLOR_ORANGE);

    // 顶部标题
    _tft->drawText(12, 8, "CC FOCUS", foreground, COLOR_ORANGE, FONT_MEDIUM);
    _tft->drawText(168, 12, "TODAY", foreground, COLOR_ORANGE, FONT_SMALL);
    _tft->fillRect(12, 30, 216, 2, foreground);

    // 中央大字:今日工作时长
    _tft->drawText(14, 60, "WORKED", foreground, COLOR_ORANGE, FONT_SMALL);
    _tft->fillRect(12, 124, 216, 1, foreground);

    // 收益行标签
    _tft->drawText(14, 138, "CC EARNED", foreground, COLOR_ORANGE, FONT_SMALL);

    // 计数行分隔
    _tft->fillRect(12, 162, 216, 1, foreground);
    _tft->drawText(12, 168, "DONE", foreground, COLOR_ORANGE, FONT_SMALL);
    _tft->drawText(92, 168, "ERR", foreground, COLOR_ORANGE, FONT_SMALL);
    _tft->drawText(160, 168, "PERM", foreground, COLOR_ORANGE, FONT_SMALL);

    // 底部:最长 + 会话
    _tft->fillRect(12, 188, 216, 1, foreground);
    _tft->drawText(12, 194, "MAX", foreground, COLOR_ORANGE, FONT_SMALL);
    _tft->drawText(124, 194, "SESS", foreground, COLOR_ORANGE, FONT_SMALL);
    _tft->drawTextCentered(222, "EVERY SECOND COUNTS", foreground,
                           COLOR_ORANGE, FONT_SMALL);
}

// 把毫秒格式化成 H:MM:SS(或 M:SS),写到 out(>=sizeBytes)
static void formatDuration(uint32_t ms, char* out, size_t sizeBytes) {
    const uint32_t totalSec = ms / 1000UL;
    const uint32_t h = totalSec / 3600UL;
    const uint32_t m = (totalSec / 60UL) % 60UL;
    const uint32_t s = totalSec % 60UL;
    if (h > 0) {
        snprintf(out, sizeBytes, "%lu:%02lu:%02lu",
                 static_cast<unsigned long>(h),
                 static_cast<unsigned long>(m),
                 static_cast<unsigned long>(s));
    } else {
        snprintf(out, sizeBytes, "%lu:%02lu",
                 static_cast<unsigned long>(m),
                 static_cast<unsigned long>(s));
    }
}

void DisplayService::drawStatsView() {
    if (!_ccService) return;
    constexpr uint16_t foreground = COLOR_WHITE;

    // —— 今日工作时长(大字,逐字符 dirty) ——
    char workedText[16];
    formatDuration(_ccService->getTodayWorkingMs(), workedText,
                   sizeof(workedText));
    if (strcmp(workedText, _lastStatsWorked) != 0) {
        constexpr uint8_t workedSize = FONT_LARGE;
        constexpr int16_t workedX = 14;
        const size_t workedLen = strlen(workedText);
        const bool layoutChanged = _lastStatsWorked[0] == '\0' ||
                                   strlen(_lastStatsWorked) != workedLen;
        if (layoutChanged) {
            _tft->fillRect(12, 78, 216, 40, COLOR_ORANGE);
            _tft->drawText(workedX, 82, workedText, foreground,
                           COLOR_ORANGE, workedSize);
        } else {
            const int16_t charWidth = _tft->getTextWidth("0", workedSize);
            char glyph[2] = {'\0', '\0'};
            for (size_t i = 0; i < workedLen; i++) {
                if (_lastStatsWorked[i] == workedText[i]) continue;
                glyph[0] = workedText[i];
                _tft->drawText(workedX + static_cast<int16_t>(i) * charWidth,
                               82, glyph, foreground, COLOR_ORANGE,
                               workedSize);
            }
        }
        strncpy(_lastStatsWorked, workedText, sizeof(_lastStatsWorked) - 1);
        _lastStatsWorked[sizeof(_lastStatsWorked) - 1] = '\0';
    }

    // —— 收益 = 今日 WORKING 秒数 × 时薪(十毫分之一元) ——
    char earnedText[16];
    SalaryCounterService* counter = salaryCounter();
    const bool payConfigured = counter && counter->isConfigured();
    if (payConfigured) {
        const uint32_t workedSec = _ccService->getTodayWorkingMs() / 1000UL;
        const uint32_t rate = counter->getRateTenThousandthsPerSecond();
        const uint64_t earned = static_cast<uint64_t>(workedSec) *
                               static_cast<uint64_t>(rate);
        const uint64_t integerPart = earned / 10000ULL;
        snprintf(earnedText, sizeof(earnedText), "%llu.%02llu",
                 static_cast<unsigned long long>(integerPart),
                 static_cast<unsigned long long>(
                     (earned % 10000ULL) / 100ULL));
    } else {
        snprintf(earnedText, sizeof(earnedText), "SET PAY");
    }
    if (strcmp(earnedText, _lastStatsEarned) != 0) {
        _tft->fillRect(12, 142, 216, 18, COLOR_ORANGE);
        _tft->drawText(14, 145, earnedText, foreground, COLOR_ORANGE,
                       FONT_MEDIUM);
        strncpy(_lastStatsEarned, earnedText, sizeof(_lastStatsEarned) - 1);
        _lastStatsEarned[sizeof(_lastStatsEarned) - 1] = '\0';
    }

    // —— 计数行(整段 dirty) ——
    char countsText[24];
    snprintf(countsText, sizeof(countsText), "%u   %u   %u",
             _ccService->getDoneCount(),
             _ccService->getErrorCount(),
             _ccService->getPermissionCount());
    if (strcmp(countsText, _lastStatsCounts) != 0) {
        _tft->fillRect(40, 168, 184, 14, COLOR_ORANGE);
        // 三段分别定位到对应标签右侧
        char num[8];
        snprintf(num, sizeof(num), "%u", _ccService->getDoneCount());
        _tft->drawText(46, 168, num, foreground, COLOR_ORANGE, FONT_SMALL);
        snprintf(num, sizeof(num), "%u", _ccService->getErrorCount());
        _tft->drawText(118, 168, num, foreground, COLOR_ORANGE, FONT_SMALL);
        snprintf(num, sizeof(num), "%u", _ccService->getPermissionCount());
        _tft->drawText(190, 168, num, foreground, COLOR_ORANGE, FONT_SMALL);
        strncpy(_lastStatsCounts, countsText, sizeof(_lastStatsCounts) - 1);
        _lastStatsCounts[sizeof(_lastStatsCounts) - 1] = '\0';
    }

    // —— 最长连续 + 会话时长 ——
    char longestText[12];
    formatDuration(_ccService->getLongestWorkingMs(), longestText,
                   sizeof(longestText));
    if (strcmp(longestText, _lastStatsLongest) != 0) {
        _tft->fillRect(40, 194, 80, 14, COLOR_ORANGE);
        _tft->drawText(46, 194, longestText, foreground, COLOR_ORANGE,
                       FONT_SMALL);
        strncpy(_lastStatsLongest, longestText, sizeof(_lastStatsLongest) - 1);
        _lastStatsLongest[sizeof(_lastStatsLongest) - 1] = '\0';
    }
    char sessionText[12];
    formatDuration(_ccService->getSessionWorkingMs(), sessionText,
                   sizeof(sessionText));
    if (strcmp(sessionText, _lastStatsSession) != 0) {
        _tft->fillRect(152, 194, 80, 14, COLOR_ORANGE);
        _tft->drawText(158, 194, sessionText, foreground, COLOR_ORANGE,
                       FONT_SMALL);
        strncpy(_lastStatsSession, sessionText, sizeof(_lastStatsSession) - 1);
        _lastStatsSession[sizeof(_lastStatsSession) - 1] = '\0';
    }
}

void DisplayService::updateSalarySchedule(unsigned long now) {
    if (!_preferenceService || !_timeService ||
        !_preferenceService->getSalaryAutoEnabled() ||
        !_timeService->isSynced()) {
        return;
    }
    const unsigned long second = now / 1000UL;
    if (second == _lastSalaryScheduleCheckSec) return;
    _lastSalaryScheduleCheckSec = second;

    const uint16_t minutesNow =
        static_cast<uint16_t>(_timeService->getHour() * 60 +
                              _timeService->getMinute());
    const uint16_t startMinutes =
        _preferenceService->getSalaryStartMinutes();
    const uint16_t endMinutes =
        _preferenceService->getSalaryEndMinutes();
    const uint32_t today =
        static_cast<uint32_t>(_timeService->getYear()) * 10000UL +
        static_cast<uint32_t>(_timeService->getMonth()) * 100UL +
        static_cast<uint32_t>(_timeService->getDay());
    const bool inWorkWindow =
        minutesNow >= startMinutes && minutesNow < endMinutes;
    const uint32_t lastAutoDate =
        _preferenceService->getSalaryLastAutoDate();
    SalaryCounterService* counter = _salaryCounter;
    if (counter && counter->rolloverToDate(today)) {
        refreshSalaryCounter();
    }

    if (minutesNow < startMinutes) {
        if (counter && counter->isSessionActive()) {
            counter->reset();
            refreshSalaryCounter();
            LOG_INFO("Salary", "班次开始前清除遗留计薪 date=%lu",
                     static_cast<unsigned long>(today));
        }
        return;
    }

    if (inWorkWindow) {
        if (!counter) counter = salaryCounter();
        if (!counter || !counter->isConfigured()) return;
        counter->rolloverToDate(today);
        if (counter->isRunning()) {
            _preferenceService->setSalaryLastAutoDate(today);
            return;
        }
        if (counter->getState() == SalaryCounterState::READY &&
            lastAutoDate != today &&
            counter->start(_timeService->getEpoch())) {
            _preferenceService->setSalaryLastAutoDate(today);
            if (_currentMode != DisplayMode::INFO &&
                _currentMode != DisplayMode::PROVISIONING &&
                !isExclusiveDisplayActive()) {
                showSalaryCounter();
            }
            LOG_INFO("Salary", "自动上班触发 date=%lu",
                     static_cast<unsigned long>(today));
        }
        return;
    }

    if (minutesNow >= endMinutes) {
        const uint32_t scheduleSeconds =
            static_cast<uint32_t>(endMinutes - startMinutes) * 60UL;
        if (counter && counter->isSessionActive() &&
            counter->finish(scheduleSeconds)) {
            refreshSalaryCounter();
            LOG_INFO("Salary", "自动下班触发 date=%lu",
                     static_cast<unsigned long>(today));
            if (_interactiveView != InteractiveView::SALARY_COUNTER) {
                releaseSalaryCounterIfIdle(
                    static_cast<uint8_t>(_interactiveView));
            }
        }
        if (_preferenceService->getSalaryLastAutoEndDate() != today) {
            _preferenceService->setSalaryLastAutoEndDate(today);
        }
        if (counter && !counter->isSessionActive() &&
            _interactiveView != InteractiveView::SALARY_COUNTER) {
            releaseSalaryCounterIfIdle(
                static_cast<uint8_t>(_interactiveView));
        }
    }
}

void DisplayService::setBrightnessPercent(uint8_t percent) {
    _brightnessPercent = constrain(percent, 0, 100);
    applyNightDimming();
}

const char* DisplayService::timetableWeekday(uint8_t weekday) const {
    static const char* DAYS[] = {"MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"};
    return weekday >= 1 && weekday <= 7 ? DAYS[weekday - 1] : "---";
}

void DisplayService::drawTimetableHeader(U8G2_FOR_ADAFRUIT_GFX& text,
                                         const TimetableSnapshot& snapshot,
                                         const char* title) {
    const uint16_t fg = _themeForeground;
    drawTimetableText(text, u8g2_font_t0_11b_tr, 14, 22, title, fg);
    char right[16];
    snprintf(right, sizeof(right), "%s · W%02u",
             timetableWeekday(snapshot.weekday),
             snapshot.academicWeek);
    const int width = timetableTextWidth(text, u8g2_font_5x8_tf, right);
    drawTimetableText(text, u8g2_font_5x8_tf, 226 - width, 22, right, fg);
    _tft->fillRect(14, 30, 212, 2, fg);
}

void DisplayService::drawTimetableCourseName(
    U8G2_FOR_ADAFRUIT_GFX& text, const TimetableCourseSnapshot& course,
    int16_t firstBaseline) {
    const uint16_t fg = _themeForeground;
    char name[44];
    snprintf(name, sizeof(name), "%s", course.name);
    if (timetableTextWidth(text, u8g2_font_courB24_tr, name) <= 212) {
        drawTimetableText(text, u8g2_font_courB24_tr, 14,
                          firstBaseline, name, fg);
        return;
    }
    int split = -1;
    for (int i = strlen(name) - 1; i > 0; --i) {
        if (name[i] != ' ') continue;
        name[i] = '\0';
        if (timetableTextWidth(text, u8g2_font_courB24_tr, name) <= 212 &&
            timetableTextWidth(text, u8g2_font_courB24_tr,
                               name + i + 1) <= 212) {
            split = i;
            break;
        }
        name[i] = ' ';
    }
    if (split > 0) {
        drawTimetableText(text, u8g2_font_courB24_tr, 14,
                          firstBaseline, name, fg);
        drawTimetableText(text, u8g2_font_courB24_tr, 14,
                          firstBaseline + 30, name + split + 1, fg);
    } else {
        const char* fallback = course.shortName[0] ? course.shortName : course.name;
        const uint8_t* font =
            timetableTextWidth(text, u8g2_font_courB18_tr, fallback) <= 212
                ? u8g2_font_courB18_tr : u8g2_font_courB14_tr;
        drawTimetableText(text, font, 14, firstBaseline, fallback, fg);
    }
}

void DisplayService::drawTimetableView() {
    TimetableSnapshot snapshot = {};
    if (!_timetableService ||
        !_timetableService->getSnapshot(_timeService, snapshot)) {
        snapshot.state = TimetableState::NOT_CONFIGURED;
    }
    const uint16_t bg = _animBgColor;
    const uint16_t fg = _themeForeground;
    U8G2_FOR_ADAFRUIT_GFX text;
    text.begin(_tft->getTft());
    const bool courseChanged =
        strncmp(_lastTimetableCourse, snapshot.course.name,
                sizeof(_lastTimetableCourse)) != 0;
    const bool fullRedraw = !_timetableLayoutDrawn ||
        snapshot.state != _lastTimetableState || courseChanged;

    if (!fullRedraw &&
        (snapshot.state == TimetableState::NEXT_CLASS ||
         snapshot.state == TimetableState::IN_CLASS)) {
        if (snapshot.minutesRemaining == _lastTimetableMinutes) return;
        _tft->fillRect(88, 176, 138, 29, bg);
        char countdown[16];
        snprintf(countdown, sizeof(countdown), "%u MIN", snapshot.minutesRemaining);
        const int width = timetableTextWidth(text, u8g2_font_courB24_tr,
                                             countdown);
        drawTimetableText(text, u8g2_font_courB24_tr, 226 - width, 201,
                          countdown, fg);
        _lastTimetableMinutes = snapshot.minutesRemaining;
        return;
    }

    _tft->fillScreen(bg);
    if (snapshot.state == TimetableState::NOT_CONFIGURED) {
        drawTimetableHeader(text, snapshot, "TIMETABLE");
        drawTimetableTextCentered(text, u8g2_font_courB18_tr, 106,
                                  "NO SCHEDULE", fg);
        drawTimetableTextCentered(text, u8g2_font_t0_12b_tr, 136,
                                  "OPEN CONTROLLER", fg);
        drawTimetableTextCentered(text, u8g2_font_t0_12b_tr, 158,
                                  "TO IMPORT CLASSES", fg);
    } else if (snapshot.state == TimetableState::NEXT_CLASS ||
               snapshot.state == TimetableState::IN_CLASS) {
        drawTimetableHeader(text, snapshot,
            snapshot.state == TimetableState::IN_CLASS ? "IN CLASS" : "NEXT CLASS");
        char timeRange[20];
        snprintf(timeRange, sizeof(timeRange), "%s - %s",
                 snapshot.course.start, snapshot.course.end);
        drawTimetableText(text, u8g2_font_t0_11b_tr, 14, 63,
                          timeRange, fg);
        drawTimetableCourseName(text, snapshot.course, 100);
        char detail[48];
        if (snapshot.course.teacher[0]) {
            snprintf(detail, sizeof(detail), "%s / %s",
                     snapshot.course.room, snapshot.course.teacher);
        } else {
            snprintf(detail, sizeof(detail), "%s", snapshot.course.room);
        }
        const uint8_t* detailFont =
            timetableTextWidth(text, u8g2_font_t0_12b_tr, detail) <= 212
                ? u8g2_font_t0_12b_tr : u8g2_font_t0_11b_tr;
        drawTimetableText(text, detailFont, 14, 154, detail, fg);
        _tft->fillRect(14, 169, 212, 1, fg);
        drawTimetableText(text, u8g2_font_t0_11b_tr, 14, 197,
            snapshot.state == TimetableState::IN_CLASS ? "ENDS IN" : "STARTS IN",
            fg);
        char countdown[16];
        snprintf(countdown, sizeof(countdown), "%u MIN", snapshot.minutesRemaining);
        const int width = timetableTextWidth(text, u8g2_font_courB24_tr,
                                             countdown);
        drawTimetableText(text, u8g2_font_courB24_tr, 226 - width, 201,
                          countdown, fg);
        _tft->drawRect(14, 218, 212, 7, fg);
        int startHour = 0, startMinute = 0, endHour = 0, endMinute = 0;
        sscanf(snapshot.course.start, "%d:%d", &startHour, &startMinute);
        sscanf(snapshot.course.end, "%d:%d", &endHour, &endMinute);
        const int duration = max(1, endHour * 60 + endMinute -
                                    startHour * 60 - startMinute);
        int progress = snapshot.state == TimetableState::IN_CLASS
            ? 208 * (duration - snapshot.minutesRemaining) / duration
            : 0;
        progress = constrain(progress, 0, 208);
        if (progress > 0) _tft->fillRect(16, 220, progress, 3, fg);
        char footer[24];
        snprintf(footer, sizeof(footer), "%u MORE TODAY",
                 snapshot.state == TimetableState::IN_CLASS
                     ? max(0, snapshot.remainingToday - 1)
                     : snapshot.remainingToday);
        drawTimetableText(text, u8g2_font_5x7_tr, 14, 237, footer, fg);
    } else {
        drawTimetableHeader(text, snapshot, "TODAY");
        if (snapshot.state == TimetableState::ALL_DONE) {
            // 与设计稿一致的双层八边形完成章和方角勾。
            _tft->drawLine(96,45,144,45,fg); _tft->drawLine(144,45,158,59,fg);
            _tft->drawLine(158,59,158,101,fg); _tft->drawLine(158,101,144,115,fg);
            _tft->drawLine(144,115,96,115,fg); _tft->drawLine(96,115,82,101,fg);
            _tft->drawLine(82,101,82,59,fg); _tft->drawLine(82,59,96,45,fg);
            for (int i = 0; i < 6; ++i) {
                _tft->drawLine(99,80+i,113,94+i,fg);
                _tft->drawLine(113,94+i,142,64+i,fg);
            }
            drawTimetableTextCentered(text, u8g2_font_courB18_tr, 139,
                                      "ALL CLASSES", fg);
            drawTimetableTextCentered(text, u8g2_font_courB18_tr, 160,
                                      "COMPLETE", fg);
            _tft->fillRect(14, 174, 212, 2, fg);
            char total[12];
            snprintf(total, sizeof(total), "%u / %u",
                     snapshot.todayCompleted, snapshot.todayTotal);
            drawTimetableText(text, u8g2_font_courB10_tr, 14, 194,
                              "TODAY", fg);
            const int16_t totalWidth = timetableTextWidth(
                text, u8g2_font_courB12_tr, total);
            drawTimetableText(text, u8g2_font_courB12_tr,
                              226 - totalWidth, 194, total, fg);
        } else {
            _tft->drawRect(70, 58, 100, 70, fg);
            _tft->fillRect(70, 73, 100, 4, fg);
            _tft->fillRect(91, 50, 8, 18, fg);
            _tft->fillRect(141, 50, 8, 18, fg);
            for (int row = 0; row < 2; ++row) {
                for (int col = 0; col < 3; ++col) {
                    _tft->fillRect(87 + col * 27, 88 + row * 19, 12, 10, fg);
                }
            }
            for (int i = 0; i < 5; ++i)
                _tft->drawLine(82,124+i,158,58+i,fg);
            drawTimetableTextCentered(text, u8g2_font_courB18_tr, 155,
                                      "NO CLASSES", fg);
            drawTimetableTextCentered(text, u8g2_font_courB18_tr, 177,
                                      "TODAY", fg);
            _tft->fillRect(14, 190, 212, 2, fg);
        }
        if (snapshot.hasNextCourse) {
            char next[28];
            snprintf(next, sizeof(next), "NEXT · %s %s",
                     timetableWeekday(snapshot.nextCourse.weekday),
                     snapshot.nextCourse.start);
            drawTimetableText(text, u8g2_font_5x8_tr, 14, 211, next, fg);
            const char* nextName = snapshot.nextCourse.shortName[0]
                ? snapshot.nextCourse.shortName : snapshot.nextCourse.name;
            drawTimetableText(text, u8g2_font_courB10_tr, 14, 229,
                              nextName, fg);
            const int roomWidth = timetableTextWidth(
                text, u8g2_font_5x8_tr, snapshot.nextCourse.room);
            drawTimetableText(text, u8g2_font_5x8_tr, 226 - roomWidth,
                              229, snapshot.nextCourse.room, fg);
        }
    }
    _timetableLayoutDrawn = true;
    _lastTimetableState = snapshot.state;
    _lastTimetableMinutes = snapshot.minutesRemaining;
    snprintf(_lastTimetableCourse, sizeof(_lastTimetableCourse), "%s",
             snapshot.course.name);
}

void DisplayService::applyNightDimming() {
    const bool nightActive = _preferenceService &&
                             _preferenceService->isNightDimActive(_timeService);
    const uint8_t effectivePercent = nightActive
        ? _preferenceService->getNightBrightnessPercent()
        : _brightnessPercent;
    if (effectivePercent == _lastAppliedBrightnessPercent) return;
    _lastAppliedBrightnessPercent = effectivePercent;
    const uint8_t pwm = map(effectivePercent, 0, 100, 0, 255);
    _tft->setBrightness(pwm);
}

void DisplayService::applyIdleDefaultView() {
    const uint8_t view = _preferenceService
        ? _preferenceService->getStartupView()
        : VIEW_EYES_NORMAL;
    switch (view) {
        case VIEW_EYES_SQUISH:
            showExpression(ExpressionId::HAPPY);
            break;
        case VIEW_CLOCK:
            _expressionPreferred = false;
            showClock();
            break;
        case VIEW_POMODORO:
            _expressionPreferred = false;
            showPomodoroReady();
            break;
        case VIEW_SALARY:
            _expressionPreferred = false;
            showSalaryCounter();
            break;
        case VIEW_TIMETABLE:
            _expressionPreferred = false;
            setInteractiveView(view);
            break;
        case VIEW_WEATHER:
        case VIEW_CRYPTO:
        case VIEW_MARKET:
            _expressionPreferred = false;
            setInteractiveView(view);
            break;
        case VIEW_EYES_NORMAL:
        default:
            showExpression(_expressionMode == ExpressionMode::AUTO
                ? _renderedExpression : _selectedExpression);
            break;
    }
}

bool DisplayService::isCarouselView(uint8_t view) const {
    return view == VIEW_CLOCK || view == VIEW_WEATHER ||
           view == VIEW_CRYPTO || view == VIEW_MARKET ||
           view == VIEW_SALARY;
}

void DisplayService::loadIdleDisplayPreferences() {
    if (!_preferenceService) return;
    _carouselEnabled = _preferenceService->getCarouselEnabled();
    _carouselSpeedSeconds = _preferenceService->getCarouselSpeedSeconds();
    _carouselFixedView = _preferenceService->getCarouselFixedView();
    for (uint8_t i = 0; i < CAROUSEL_VIEW_COUNT; i++) {
        _carouselOrder[i] = _preferenceService->getCarouselView(i);
    }
    if (_carouselIndex >= CAROUSEL_VIEW_COUNT) _carouselIndex = 0;
}

void DisplayService::syncCarouselIndexForView(uint8_t view) {
    for (uint8_t i = 0; i < CAROUSEL_VIEW_COUNT; i++) {
        if (_carouselOrder[i] == view) {
            _carouselIndex = i;
            return;
        }
    }
    _carouselIndex = 0;
}

void DisplayService::showCarouselCurrentView() {
    _carouselIndex %= CAROUSEL_VIEW_COUNT;
    _carouselSuspended = false;
    _carouselPageStartedMs = millis();
    setInteractiveView(_carouselOrder[_carouselIndex]);
}

void DisplayService::switchToIdleDisplay() {
    if (provisioningScreenProtected()) return;
    if (_carouselEnabled) {
        showCarouselCurrentView();
        return;
    }
    if (_salaryCounter && _salaryCounter->isSessionActive()) {
        showSalaryCounter();
        return;
    }
    _carouselSuspended = false;
    applyIdleDefaultView();
}

void DisplayService::reloadIdleDisplayPreferences() {
    const uint8_t currentView = static_cast<uint8_t>(_interactiveView);
    loadIdleDisplayPreferences();
    if (isCarouselView(currentView)) syncCarouselIndexForView(currentView);
    else _carouselIndex = 0;

    // Claude Code 正在占用屏幕时只更新配置；结束后再按新配置恢复。
    if (_currentMode != DisplayMode::INFO && _currentMode != DisplayMode::PROVISIONING) {
        switchToIdleDisplay();
    }
}

// ── Main update loop ───────────────────────────────────────────
// 状态机负责切换显示模式(switchToExpressionMode/switchToInfoMode/updateProvisioning),
// 本方法只负责按当前 _currentMode 渲染。
void DisplayService::update() {
    unsigned long now = millis();
    const bool exclusiveDisplayActive = isExclusiveDisplayActive();
    // 游戏/媒体与 TLS 都需要短时内存和 CPU 峰值。独占视图期间
    // 保留现有数据，退出后按原有刷新/重试逻辑继续。
    if (!exclusiveDisplayActive) {
        if (_holidayService) _holidayService->update();
        if (_weatherService) _weatherService->update();
        if (_cryptoService) _cryptoService->update();
        if (_marketService) _marketService->update();
    }
    // 自动上下班属于时间边界，不应被游戏或网络刷新暂停。
    updateSalarySchedule(now);
    if (now - _lastNightDimCheckMs > 30000UL) {
        _lastNightDimCheckMs = now;
        applyNightDimming();
    }
    if (_currentMode == DisplayMode::INTERACTIVE) {
        if (_interactiveView == InteractiveView::MEDIA) {
            updateMediaGif(now);
            return;
        }
        if (isArcadeGameView() && _activeArcadeGame) {
            _activeArcadeGame->update();
            return;
        }
        if (_carouselEnabled && !_carouselSuspended &&
            isCarouselView(static_cast<uint8_t>(_interactiveView)) &&
            now - _carouselPageStartedMs >=
                static_cast<unsigned long>(_carouselSpeedSeconds) * 1000UL) {
            _carouselIndex = (_carouselIndex + 1) % CAROUSEL_VIEW_COUNT;
            showCarouselCurrentView();
            return;
        }
        if (_interactiveView == InteractiveView::WEATHER) {
            const uint32_t version = _weatherService ? _weatherService->getVersion() : 0;
            if (version != _lastWeatherVersion) {
                _lastWeatherVersion = version;
                drawWeatherView();
            }
            return;
        }
        if (_interactiveView == InteractiveView::CRYPTO) {
            const uint32_t version = _cryptoService ? _cryptoService->getVersion() : 0;
            if (version != _lastCryptoVersion) {
                _lastCryptoVersion = version;
                drawCryptoView();
            }
            return;
        }
        if (_interactiveView == InteractiveView::MARKET) {
            const uint32_t version = _marketService ? _marketService->getVersion() : 0;
            if (version != _lastMarketVersion) {
                _lastMarketVersion = version;
                drawMarketView();
            }
            return;
        }
        if (_interactiveView != InteractiveView::CLOCK &&
            _interactiveView != InteractiveView::POMODORO &&
            _interactiveView != InteractiveView::SALARY_COUNTER &&
            _interactiveView != InteractiveView::TIMETABLE &&
            _interactiveView != InteractiveView::STATS) {
            return;
        }
        if (_interactiveView == InteractiveView::TIMETABLE) {
            const unsigned long minute = _timeService
                ? _timeService->getEpoch() / 60UL : now / 60000UL;
            if (minute == _lastTimetableRenderMinute) return;
            _lastTimetableRenderMinute = minute;
            drawTimetableView();
            return;
        }
        if (_interactiveView == InteractiveView::STATS) {
            if (now - _lastStatsRenderMs < 250UL) return;
            _lastStatsRenderMs = now;
            drawStatsView();
            return;
        }
        const unsigned long sec = now / 1000UL;
        if (_interactiveView == InteractiveView::SALARY_COUNTER) {
            if (now - _lastSalaryRenderMs < 40UL) return;
            _lastSalaryRenderMs = now;
            drawSalaryCounterView();
            return;
        }
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
        updateAutoExpression(now);
        _eyesView.update();
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
    static int lastDotCount = -1;
    static int lastRetrySec = -1;
    static char lastPhase[16] = "";
    auto mode = _wifiService->getProvisioningMode();

    // 模式切换(或从其它显示模式回来)时整屏重绘静态内容;
    // 模式不变时只刷新下方动态元素,避免整屏闪烁
    if (mode != lastMode || _currentMode != DisplayMode::PROVISIONING) {
        _currentMode = DisplayMode::PROVISIONING;
        lastMode = mode;
        lastDotCount = -1;
        lastRetrySec = -1;
        lastPhase[0] = '\0';

        // 与其他视图一致的橙底白字(背景跟随偏好设置)
        _tft->fillScreen(_animBgColor);

        // 标题(CONNECTED 屏布局独立,不显示)
        if (mode != WifiConfigService::ProvisioningMode::CONNECTED) {
            _tft->getTft().setTextColor(COLOR_WHITE);
            _tft->getTft().setTextSize(2);
            const char* title = "WiFi Setup";
            int16_t titleX = (CFG_DISPLAY_WIDTH - (int)strlen(title) * 12) / 2;
            _tft->getTft().setCursor(titleX, 22);
            _tft->getTft().print(title);
        }

        if (mode == WifiConfigService::ProvisioningMode::AP_FALLBACK) {
            _tft->getTft().setTextColor(COLOR_WHITE);
            _tft->getTft().setTextSize(1);
            // 重试打满回到配网页时,提示用户重新配置
            const char* subtitle = _wifiService->isRetryExhausted()
                ? "Too many attempts, set up again"
                : "Keep this page open";
            int16_t subX = (CFG_DISPLAY_WIDTH - (int)strlen(subtitle) * 6) / 2;
            _tft->getTft().setCursor(subX, 46);
            _tft->getTft().print(subtitle);

            // 二维码:白底黑码,保证扫码对比度
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
            const char* scan = "Scan QR or open:";
            const char* url = "http://192.168.4.1";
            _tft->getTft().setCursor((CFG_DISPLAY_WIDTH - (int)strlen(scan) * 6) / 2, 174);
            _tft->getTft().print(scan);
            _tft->getTft().setCursor((CFG_DISPLAY_WIDTH - (int)strlen(url) * 6) / 2, 190);
            _tft->getTft().print(url);
        } else if (mode == WifiConfigService::ProvisioningMode::CONNECTING) {
            // 副标题 + 目标 SSID(阶段文案与三点动画走动态刷新)
            _tft->getTft().setTextColor(COLOR_WHITE);
            _tft->getTft().setTextSize(1);
            const char* subtitle = "Joining network";
            int16_t subX = (CFG_DISPLAY_WIDTH - (int)strlen(subtitle) * 6) / 2;
            _tft->getTft().setCursor(subX, 46);
            _tft->getTft().print(subtitle);

            String ssid = _wifiService->getSavedSSID();
            if (ssid.length() > 16) ssid = ssid.substring(0, 13) + "...";
            _tft->getTft().setTextSize(2);
            int16_t ssidX = (CFG_DISPLAY_WIDTH - (int)ssid.length() * 12) / 2;
            _tft->getTft().setCursor(ssidX, 98);
            _tft->getTft().print(ssid);
        } else if (mode == WifiConfigService::ProvisioningMode::RETRY_WAIT) {
            // 连接失败:白色圆圈叉图标 + 失败信息 + SSID,重试倒计时走动态刷新
            const int16_t cx = CFG_DISPLAY_WIDTH / 2;
            const int16_t cy = 72;
            _tft->drawCircle(cx, cy, 17, COLOR_WHITE);
            _tft->drawCircle(cx, cy, 16, COLOR_WHITE);
            for (int8_t off = -1; off <= 1; off++) {
                _tft->drawLine(cx - 7 + off, cy - 7, cx + 7 + off, cy + 7, COLOR_WHITE);
                _tft->drawLine(cx + 7 + off, cy - 7, cx - 7 + off, cy + 7, COLOR_WHITE);
            }

            _tft->getTft().setTextColor(COLOR_WHITE);
            _tft->getTft().setTextSize(2);
            // 失败原因: Wrong password / Network not found / Connection failed
            const char* fail = _wifiService->getLastError();
            int16_t failX = (CFG_DISPLAY_WIDTH - (int)strlen(fail) * 12) / 2;
            _tft->getTft().setCursor(failX, 104);
            _tft->getTft().print(fail);

            String ssid = _wifiService->getSavedSSID();
            if (ssid.length() > 30) ssid = ssid.substring(0, 27) + "...";
            _tft->getTft().setTextSize(1);
            int16_t ssidX = (CFG_DISPLAY_WIDTH - (int)ssid.length() * 6) / 2;
            _tft->getTft().setCursor(ssidX, 132);
            _tft->getTft().print(ssid);

            // 已失败次数 / 最大重试次数
            char attempt[24];
            snprintf(attempt, sizeof(attempt), "Attempt %d of %d",
                     _wifiService->getRetryCount(), CFG_WIFI_MAX_RETRIES);
            int16_t hintX = (CFG_DISPLAY_WIDTH - (int)strlen(attempt) * 6) / 2;
            _tft->getTft().setCursor(hintX, 148);
            _tft->getTft().print(attempt);
        } else if (mode == WifiConfigService::ProvisioningMode::CONNECTED) {
            // 白色圆圈勾图标 + 连接信息
            const int16_t cx = CFG_DISPLAY_WIDTH / 2;
            const int16_t cy = 74;
            _tft->drawCircle(cx, cy, 17, COLOR_WHITE);
            _tft->drawCircle(cx, cy, 16, COLOR_WHITE);
            for (int8_t off = -1; off <= 1; off++) {
                _tft->drawLine(cx - 8 + off, cy, cx - 2 + off, cy + 6, COLOR_WHITE);
                _tft->drawLine(cx - 2 + off, cy + 6, cx + 9 + off, cy - 8, COLOR_WHITE);
            }

            _tft->getTft().setTextColor(COLOR_WHITE);
            _tft->getTft().setTextSize(2);
            const char* ok = "Connected";
            int16_t x = (CFG_DISPLAY_WIDTH - (int)strlen(ok) * 12) / 2;
            _tft->getTft().setCursor(x, 106);
            _tft->getTft().print(ok);

            _tft->getTft().setTextSize(1);
            String lanIp = "LAN: " + _wifiService->getLanIP();
            String apIp = "AP: " + _wifiService->getAPIP();
            String mdns = "clawd-mochi.local";

            int16_t lanX = (CFG_DISPLAY_WIDTH - (int)lanIp.length() * 6) / 2;
            int16_t apX = (CFG_DISPLAY_WIDTH - (int)apIp.length() * 6) / 2;
            int16_t mdnsX = (CFG_DISPLAY_WIDTH - (int)mdns.length() * 6) / 2;
            _tft->getTft().setCursor(lanX, 140);
            _tft->getTft().print(lanIp);
            _tft->getTft().setCursor(apX, 156);
            _tft->getTft().print(apIp);
            _tft->getTft().setCursor(mdnsX, 172);
            _tft->getTft().print(mdns);
        }
    }

    // 动态元素:局部擦除重绘,仅在内容变化时执行
    if (mode == WifiConfigService::ProvisioningMode::CONNECTING) {
        // 阶段文案(Connecting / Obtaining IP),随 WiFi 事件推进
        const char* phase = _wifiService->getConnectPhaseText();
        int phaseLen = (int)strlen(phase);
        int16_t blockX = (CFG_DISPLAY_WIDTH - (phaseLen + 3) * 12) / 2;
        if (strcmp(phase, lastPhase) != 0) {
            // 阶段切换:整行擦除重排
            strlcpy(lastPhase, phase, sizeof(lastPhase));
            lastDotCount = -1;
            _tft->fillRect(20, 136, CFG_DISPLAY_WIDTH - 40, 16, _animBgColor);
            _tft->getTft().setTextColor(COLOR_WHITE);
            _tft->getTft().setTextSize(2);
            _tft->getTft().setCursor(blockX, 136);
            _tft->getTft().print(phase);
        }
        // "..." 三点循环动画(每 400ms 变一次),跟在阶段文案后
        int dots = (int)((millis() / 400) % 4);
        if (dots != lastDotCount) {
            lastDotCount = dots;
            int16_t dotX = blockX + phaseLen * 12;
            _tft->fillRect(dotX, 136, 3 * 12, 16, _animBgColor);
            _tft->getTft().setTextColor(COLOR_WHITE);
            _tft->getTft().setTextSize(2);
            _tft->getTft().setCursor(dotX, 136);
            for (int i = 0; i < dots; i++) _tft->getTft().print('.');
        }
    } else if (mode == WifiConfigService::ProvisioningMode::RETRY_WAIT) {
        // 重试倒计时(每秒刷新,向上取整避免显示 0s)
        int sec = (int)((_wifiService->getRetryRemainingMs() + 999) / 1000);
        if (sec != lastRetrySec) {
            lastRetrySec = sec;
            char buf[24];
            snprintf(buf, sizeof(buf), "Retrying in %ds", sec);
            _tft->fillRect(20, 172, CFG_DISPLAY_WIDTH - 40, 18, _animBgColor);
            _tft->getTft().setTextColor(COLOR_WHITE);
            _tft->getTft().setTextSize(2);
            int16_t x = (CFG_DISPLAY_WIDTH - (int)strlen(buf) * 12) / 2;
            _tft->getTft().setCursor(x, 174);
            _tft->getTft().print(buf);
        }
    }
}

bool DisplayService::provisioningScreenProtected() const {
    // 仅保护短暂的成功确认窗;AP/CONNECTING/RETRY_WAIT 由状态机驱动,不在此拦截
    return _wifiService &&
           _wifiService->getProvisioningMode() ==
               WifiConfigService::ProvisioningMode::CONNECTED;
}

void DisplayService::switchToExpressionMode() {
    if (provisioningScreenProtected()) return;
    _currentMode = DisplayMode::EXPRESSION;
    if (!_claudeStatusEnabled) {
        applyIdleDefaultView();
        return;
    }
    auto status = _ccService->getStatus();
    if (status == ClaudeCodeService::Status::THINKING) drawThinking(3);
    else if (status == ClaudeCodeService::Status::WORKING) drawWorking();
    else applyIdleDefaultView();
}

void DisplayService::switchToInfoMode() {
    _carouselSuspended = _carouselEnabled;
    _currentMode = DisplayMode::INFO;
    _ccView.reset();
    _tft->clear(COLOR_ORANGE);
}

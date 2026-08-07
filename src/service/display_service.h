#pragma once

#include <Arduino.h>
#include <FS.h>
#include "../hardware/tft_display.h"
#include "claude_code_service.h"
#include "../view/claude_code_view.h"
#include "../view/eyes_view.h"
#include "../view/arcade_game.h"
#include "../view/arcade_canvas.h"
#include "wifi_config_service.h"
#include "time_service.h"
#include "preference_service.h"
#include "weather_service.h"
#include "crypto_service.h"
#include "market_service.h"
#include "holiday_service.h"
#include "salary_counter_service.h"
#include "timetable_service.h"
#include "desktop_stream_service.h"
#include "keyboard_pet_service.h"
#include "../config/cfg_display.h"

class U8G2_FOR_ADAFRUIT_GFX;
class AnimatedGIF;
struct gif_file_tag;
struct gif_draw_tag;

enum class DisplayMode {
    SETUP,
    EXPRESSION,
    INFO,
    PROVISIONING,  // 配网流程:SmartConfig/AP/连接中
    INTERACTIVE  // Web-controlled mode
};

enum class InteractiveView {
    EYES_NORMAL,
    EYES_SQUISH,
    CODE_VIEW,
    DRAW,
    THINKING,
    WORKING,
    CLOCK,
    POMODORO,
    WEATHER,
    CRYPTO,
    MARKET,
    DINO_GAME,
    SOKOBAN_GAME,
    TETRIS_GAME,
    SNAKE_GAME,
    GAME_2048,
    BREAKOUT_GAME,
    SALARY_COUNTER,
    TIMETABLE,
    MEDIA,
    STATS,
    DESKTOP_STREAM
    , KEYBOARD_PET
};

enum class PomodoroPhase {
    FOCUS,
    BREAK
};

class DisplayService {
public:
    static constexpr uint8_t MEDIA_STRIP_ROWS = 16;
    static constexpr size_t MEDIA_ROW_BUFFER_BYTES =
        CFG_DISPLAY_WIDTH * MEDIA_STRIP_ROWS * sizeof(uint16_t);

    DisplayService(TftDisplay* tft, ClaudeCodeService* ccService,
                   WifiConfigService* wifiService, TimeService* timeService,
                   PreferenceService* preferenceService,
                   WeatherService* weatherService,
                   CryptoService* cryptoService,
                   MarketService* marketService,
                   HolidayService* holidayService,
                   TimetableService* timetableService,
                   DesktopStreamService* streamService,
                   KeyboardPetService* keyboardPetService);
    ~DisplayService();
    void init();
    void update();

    // Interactive mode control (called by WebService)
    void enterInteractive();
    void exitInteractive();
    bool isInteractive() const { return _currentMode == DisplayMode::INTERACTIVE; }

    void setInteractiveView(uint8_t view);
    uint8_t getInteractiveView() const { return static_cast<uint8_t>(_interactiveView); }

    void setExpression(ExpressionId expression);
    void setExpressionMode(ExpressionMode mode);
    ExpressionId getSelectedExpression() const { return _selectedExpression; }
    ExpressionId getRenderedExpression() const { return _renderedExpression; }
    ExpressionMode getExpressionMode() const { return _expressionMode; }

    void setAnimSpeed(uint8_t speed) { _animSpeed = constrain(speed, 1, 3); }
    uint8_t getAnimSpeed() const { return _animSpeed; }

    void setAnimBgColor(uint16_t color) {
        _animBgColor = color;
        _eyesView.setBackgroundColor(color);
    }
    void setDrawBgColor(uint16_t color) { _drawBgColor = color; }
    uint16_t getAnimBgColor() const { return _animBgColor; }
    uint16_t getDrawBgColor() const { return _drawBgColor; }

    void redrawCurrentView();

    // Terminal
    void termClear();
    void termAddChar(char c);
    void exitTerminal();  // q key: leave term mode, redraw static Code view
    bool isTermMode() const { return _termMode; }

    // Canvas
    void drawClear(uint16_t bgColor);
    void drawStroke(uint16_t penColor, const String& pointsData);

    // Dino game
    void startDinoGame();
    void dinoJump();
    void restartDinoGame();
    void exitDinoGame();
    bool isDinoGameActive() const;
    String getDinoGameStateJson() const;

    // Sokoban game
    void startSokobanGame();
    bool moveSokoban(int8_t dx, int8_t dy);
    bool undoSokoban();
    void restartSokoban();
    bool selectSokobanLevel(uint8_t level);
    void exitSokobanGame();
    bool isSokobanGameActive() const;
    bool isGameActive() const;
    String getSokobanStateJson() const;

    // 静态图投屏，以及 LittleFS 中 GIF 的本地流式解码播放。
    bool beginMediaFrame(uint16_t x, uint16_t y,
                         uint16_t width, uint16_t height);
    bool writeMediaFrameBytes(const uint8_t* data, size_t length);
    bool finishMediaFrame();
    void abortMediaFrame();
    bool startMediaGif(const char* path);
    void stopMedia();
    bool isMediaActive() const { return _mediaActive; }
    unsigned long getMediaLastRenderMs() const { return _mediaLastRenderMs; }
    uint32_t getMediaRenderedFrames() const { return _mediaRenderedFrames; }
    bool isExclusiveDisplayActive() const {
        return isGameActive() || _mediaActive || _streamActive ||
               _interactiveView == InteractiveView::KEYBOARD_PET;
    }

    // 桌面投屏(Desktop Stream):进入/退出视图与状态查询
    bool enterDesktopStream();
    void exitDesktopStream();
    bool isStreamActive() const { return _streamActive; }
    String getStreamStatusJson() const;

    // 可扩展游戏注册入口
    bool startArcadeGame(const String& slug);
    bool handleArcadeAction(const String& action, int value = 0);
    void exitArcadeGame();
    String getArcadeGameStateJson(const String& slug = "") const;
    const char* getActiveArcadeGameSlug() const;

    // Logo animation
    void animLogoReveal();

    void drawThinking(uint8_t dotCount = 0);
    void drawWorking(bool blinkLeft = false, bool blinkRight = false);
    void animThinking();
    void animWorking();

    bool isBusy() const { return _busy; }

    uint16_t hexToRgb565(const String& hex);

    // Clock / Pomodoro
    void showClock();
    void showPomodoroReady();
    void startPomodoro(PomodoroPhase phase);
    void pausePomodoro();
    void resetPomodoro();
    void setPomodoroDurations(uint16_t focusMinutes, uint16_t breakMinutes);
    bool isPomodoroRunning() const { return _pomodoroRunning; }
    bool isPomodoroPaused() const { return _pomodoroPaused; }
    PomodoroPhase getPomodoroPhase() const { return _pomodoroPhase; }
    uint16_t getFocusMinutes() const { return _focusMinutes; }
    uint16_t getBreakMinutes() const { return _breakMinutes; }
    uint32_t getPomodoroRemainingSec() const;
    uint32_t getPomodoroDurationSec() const;

    // 上班赚钱计数。运行会话在切换页面后继续，空闲模块离开时立即释放。
    SalaryCounterService* salaryCounter();
    bool showSalaryCounter();
    void refreshSalaryCounter();
    bool isSalarySessionActive() const;

    // Claude Code 会话/今日统计面板(数据来自 ClaudeCodeService,收益由薪资时薪合成)
    bool showStatsView();

    // Backlight brightness
    void setBrightnessPercent(uint8_t percent);
    uint8_t getBrightnessPercent() const { return _brightnessPercent; }

    void setClaudeStatusEnabled(bool enabled) { _claudeStatusEnabled = enabled; }
    bool isClaudeStatusEnabled() const { return _claudeStatusEnabled; }

    void setDisplayTheme(uint8_t theme);
    uint8_t getDisplayTheme() const { return _displayTheme; }

    void setFontStyle(FontStyle style);
    FontStyle getFontStyle() const { return _fontStyle; }

    // 配网流程绘制(供 ProvisioningState 调用)
    void updateProvisioning();

    // 表情/信息模式切换(供 Idle/Working 状态调用)
    void switchToExpressionMode();
    void switchToIdleDisplay();
    void switchToInfoMode();
    void reloadIdleDisplayPreferences();
    bool isCarouselEnabled() const { return _carouselEnabled; }

private:
    TftDisplay* _tft;
    ClaudeCodeService* _ccService;
    WifiConfigService* _wifiService;
    TimeService* _timeService;
    PreferenceService* _preferenceService;
    WeatherService* _weatherService;
    CryptoService* _cryptoService;
    MarketService* _marketService;
    HolidayService* _holidayService;
    TimetableService* _timetableService;
    SalaryCounterService* _salaryCounter;
    ClaudeCodeView _ccView;
    EyesView _eyesView;
    uint8_t* _monoGameBuffer;
    ArcadeCanvas* _arcadeCanvas;
    IArcadeGame* _activeArcadeGame;
    DesktopStreamService* _streamService;
    KeyboardPetService* _keyboardPetService;
    bool _streamActive;
    KeyboardPetService::Paw _lastPetPaw;
    void drawKeyboardPet(KeyboardPetService::Paw paw);
    uint16_t* _mediaRowBuffer;
    fs::File* _mediaFile;
    AnimatedGIF* _mediaGif;
    uint16_t _mediaRow;
    uint16_t _mediaColumn;
    uint16_t _mediaX;
    uint16_t _mediaY;
    uint16_t _mediaWidth;
    uint16_t _mediaHeight;
    uint8_t _mediaHighByte;
    bool _mediaHasHighByte;
    bool _mediaFrameReceiving;
    bool _mediaActive;
    bool _mediaGifPlaying;
    bool _mediaGifLoopPending;
    unsigned long _mediaNextFrameMs;
    int16_t _mediaGifOffsetX;
    int16_t _mediaGifOffsetY;
    uint16_t _mediaGifStripX;
    uint16_t _mediaGifStripY;
    uint16_t _mediaGifStripWidth;
    uint16_t _mediaGifStripRows;
    bool _mediaGifStripActive;
    unsigned long _mediaLastRenderMs;
    uint32_t _mediaRenderedFrames;
    DisplayMode _currentMode;
    unsigned long _lastRefreshMs;

    // Interactive state
    InteractiveView _interactiveView;
    bool _interactiveActive;
    bool _busy;
    uint8_t _animSpeed;
    uint16_t _animBgColor;
    uint16_t _drawBgColor;
    uint8_t _brightnessPercent;
    bool _claudeStatusEnabled;
    uint8_t _displayTheme;
    FontStyle _fontStyle;
    uint16_t _themeForeground;

    // 表情模式。AUTO 只由用户主动开启，默认保持 MANUAL / Normal。
    ExpressionMode _expressionMode;
    ExpressionId _selectedExpression;
    ExpressionId _renderedExpression;
    ExpressionId _lastAutoExpression;
    unsigned long _nextAutoEventMs;
    unsigned long _autoReturnMs;
    bool _expressionPreferred;

    // 空闲时的信息轮播。Claude Code 进入 INFO 后只暂停，不丢失当前位置。
    bool _carouselEnabled;
    uint8_t _carouselSpeedSeconds;
    uint8_t _carouselOrder[CAROUSEL_VIEW_COUNT];
    uint8_t _carouselFixedView;
    uint8_t _carouselIndex;
    unsigned long _carouselPageStartedMs;
    bool _carouselSuspended;

    // Clock / Pomodoro state
    uint16_t _focusMinutes;
    uint16_t _breakMinutes;
    PomodoroPhase _pomodoroPhase;
    bool _pomodoroRunning;
    bool _pomodoroPaused;
    uint32_t _pomodoroDurationSec;
    uint32_t _pomodoroRemainingAtPauseSec;
    unsigned long _pomodoroStartedMs;
    unsigned long _lastClockRenderSec;
    unsigned long _lastSalaryRenderMs;
    unsigned long _lastSalaryScheduleCheckSec;
    unsigned long _lastTimetableRenderMinute;
    TimetableState _lastTimetableState;
    uint16_t _lastTimetableMinutes;
    bool _timetableLayoutDrawn;
    char _lastTimetableCourse[44];
    uint32_t _lastWeatherVersion;
    uint32_t _lastCryptoVersion;
    uint32_t _lastMarketVersion;
    unsigned long _lastNightDimCheckMs;
    uint8_t _lastAppliedBrightnessPercent;

    // Time view rendering cache (anti-flicker)
    bool _timeViewDirty;
    bool _timeViewLayoutDrawn;
    char _lastTimeText[12];
    char _lastSubText[20];
    char _lastHintText[12];
    char _lastClockLayoutKey[48];
    uint16_t _lastProgressPermille;
    bool _lastLightProgress;
    char _lastSalaryAmount[20];
    char _lastSalaryWorked[24];
    char _lastSalaryState[28];
    uint16_t _lastSalaryProgressPermille;
    int16_t _lastSalaryAmountX;
    uint8_t _lastSalaryAmountSize;
    int16_t _lastSalaryWorkedX;
    uint8_t _lastSalaryWorkedSize;

    // STATS 视图渲染缓存(反闪烁)
    char _lastStatsWorked[16];
    char _lastStatsEarned[16];
    char _lastStatsCounts[24];
    char _lastStatsLongest[12];
    char _lastStatsSession[12];
    unsigned long _lastStatsRenderMs;

    IArcadeGame* createArcadeGame(const String& slug);
    void releaseArcadeGame();
    void releaseMediaBuffer();
    void releaseStreamIfActive(uint8_t nextView);
    void updateMediaGif(unsigned long now);
    bool flushMediaGifStrip();
    static void* openMediaGif(const char* path, int32_t* fileSize);
    static void closeMediaGif(void* handle);
    static int32_t readMediaGif(gif_file_tag* file, uint8_t* data,
                                int32_t length);
    static int32_t seekMediaGif(gif_file_tag* file, int32_t position);
    static void drawMediaGif(gif_draw_tag* draw);
    void restoreAfterExclusiveView();
    bool isKnownArcadeGame(const String& slug) const;
    uint8_t viewForArcadeGame(ArcadeGameId id) const;
    const char* slugForArcadeView(uint8_t view) const;
    bool isArcadeGameView() const;
    void releaseSalaryCounterIfIdle(uint8_t nextView);

    // Terminal state
    bool _termMode;
    String _termLines[TERM_ROWS];
    uint8_t _termRow;
    uint8_t _termCol;

    // Drawing helpers
    void drawCodeView();
    void drawClockView();
    void drawPomodoroView();
    void drawWeatherView();
    void drawWeatherIcon(int weatherCode, int16_t x, int16_t y);
    void drawCryptoView();
    void formatCryptoPrice(float price, char* output, size_t size);
    void drawMarketView();
    void drawSalaryCounterView();
    void drawTimetableView();
    void drawTimetableHeader(U8G2_FOR_ADAFRUIT_GFX& text,
                             const TimetableSnapshot& snapshot,
                             const char* title);
    void drawTimetableCourseName(U8G2_FOR_ADAFRUIT_GFX& text,
                                 const TimetableCourseSnapshot& course,
                                 int16_t firstBaseline);
    const char* timetableWeekday(uint8_t weekday) const;
    void drawSalaryCounterLayout();
    void drawStatsView();
    void drawStatsLayout();
    void updateSalarySchedule(unsigned long now);
    void formatMarketPrice(float price, char* output, size_t size);
    void renderTimeScreen(const char* mark, const char* timeText, const char* subText,
                          const char* modeText, const char* hintText,
                          uint16_t progressPermille, bool lightProgress);
    void renderTimeScreenLayout(const char* mark, const char* modeText);
    void renderTimeScreenDynamic(const char* timeText, const char* subText,
                                 const char* hintText,
                                 uint16_t progressPermille, bool lightProgress);
    void invalidateTimeView();
    void applyIdleDefaultView();
    void loadIdleDisplayPreferences();
    void showCarouselCurrentView();
    bool isCarouselView(uint8_t view) const;
    void syncCarouselIndexForView(uint8_t view);
    void applyNightDimming();
    void showExpression(ExpressionId expression);
    void updateAutoExpression(unsigned long now);
    void scheduleNextAutoEvent(unsigned long now);

    // Eye geometry
    int16_t eyeLX(int16_t ox);
    int16_t eyeRX(int16_t ox);
    int16_t eyeY();
    int16_t eyeCY();

    // Terminal helpers
    void termDrawHeader();
    void termDrawLine(uint8_t r);
    void termDrawPrefix(int16_t yy);
    void termDrawLastChar();
    void termDrawBackspace();
    void termFullRedraw();
    void termScroll();

    int speedMs(int ms);
};

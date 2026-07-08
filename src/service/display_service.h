#pragma once

#include <Arduino.h>
#include "../hardware/tft_display.h"
#include "claude_code_service.h"
#include "../view/claude_code_view.h"
#include "../view/eyes_view.h"
#include "wifi_config_service.h"
#include "time_service.h"
#include "preference_service.h"
#include "../config/cfg_display.h"

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
    POMODORO
};

enum class PomodoroPhase {
    FOCUS,
    BREAK
};

class DisplayService {
public:
    DisplayService(TftDisplay* tft, ClaudeCodeService* ccService,
                   WifiConfigService* wifiService, TimeService* timeService,
                   PreferenceService* preferenceService);
    void init();
    void update();

    // Interactive mode control (called by WebService)
    void enterInteractive();
    void exitInteractive();
    bool isInteractive() const { return _currentMode == DisplayMode::INTERACTIVE; }

    void setInteractiveView(uint8_t view);
    uint8_t getInteractiveView() const { return static_cast<uint8_t>(_interactiveView); }

    void setAnimSpeed(uint8_t speed) { _animSpeed = constrain(speed, 1, 3); }
    uint8_t getAnimSpeed() const { return _animSpeed; }

    void setAnimBgColor(uint16_t color) { _animBgColor = color; }
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

    // Logo animation
    void animLogoReveal();

    // Eye animations
    void animNormalEyes();
    void animSquishEyes();

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

    // Backlight brightness
    void setBrightnessPercent(uint8_t percent);
    uint8_t getBrightnessPercent() const { return _brightnessPercent; }

    // 配网流程绘制(供 ProvisioningState 调用)
    void updateProvisioning();

    // 表情/信息模式切换(供 Idle/Working 状态调用)
    void switchToExpressionMode();
    void switchToInfoMode();

private:
    TftDisplay* _tft;
    ClaudeCodeService* _ccService;
    WifiConfigService* _wifiService;
    TimeService* _timeService;
    PreferenceService* _preferenceService;
    ClaudeCodeView _ccView;
    EyesView _eyesView;
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
    unsigned long _lastNightDimCheckMs;
    uint8_t _lastAppliedBrightnessPercent;

    // Time view rendering cache (anti-flicker)
    bool _timeViewDirty;
    bool _timeViewLayoutDrawn;
    char _lastTimeText[12];
    char _lastSubText[20];
    char _lastHintText[12];
    uint16_t _lastProgressPermille;
    bool _lastLightProgress;

    // Terminal state
    bool _termMode;
    String _termLines[TERM_ROWS];
    uint8_t _termRow;
    uint8_t _termCol;

    // Drawing helpers
    void drawNormalEyes(int16_t ox = 0, bool blink = false);
    void drawSquishEyes(bool closed = false);
    void drawCodeView();
    void drawClockView();
    void drawPomodoroView();
    void renderTimeScreen(const char* mark, const char* timeText, const char* subText,
                          const char* modeText, const char* hintText,
                          uint16_t progressPermille, bool lightProgress);
    void renderTimeScreenLayout(const char* mark, const char* modeText);
    void renderTimeScreenDynamic(const char* timeText, const char* subText,
                                 const char* hintText,
                                 uint16_t progressPermille, bool lightProgress);
    void invalidateTimeView();
    void applyIdleDefaultView();
    void applyNightDimming();
    void drawChevron(int16_t cx, int16_t cy, int16_t arm, int16_t reach, uint8_t thk, bool rightFacing, uint16_t col);

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

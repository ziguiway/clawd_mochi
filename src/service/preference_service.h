#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "../config/cfg_display.h"
#include "../config/font_style.h"
#include "../view/expression_id.h"

class TimeService;

class PreferenceService {
public:
    void init();

    String getDefaultBgHex() const { return _defaultBgHex; }
    void setDefaultBgHex(const String& hex);

    uint8_t getAnimSpeed() const { return _animSpeed; }
    void setAnimSpeed(uint8_t speed);

    uint8_t getStartupView() const { return _startupView; }
    void setStartupView(uint8_t view);

    uint8_t getBrightnessPercent() const { return _brightnessPercent; }
    void setBrightnessPercent(uint8_t percent);

    bool getClaudeStatusEnabled() const { return _claudeStatusEnabled; }
    void setClaudeStatusEnabled(bool enabled);

    uint32_t getDinoHighScore() const { return _dinoHighScore; }
    void setDinoHighScore(uint32_t score);

    uint8_t getSokobanLevel() const { return _sokobanLevel; }
    void setSokobanLevel(uint8_t level);
    uint32_t getSokobanCompletedMask() const { return _sokobanCompletedMask; }
    void setSokobanCompletedMask(uint32_t mask);

    uint32_t getTetrisHighScore() const { return _tetrisHighScore; }
    void setTetrisHighScore(uint32_t score);
    uint32_t getSnakeHighScore() const { return _snakeHighScore; }
    void setSnakeHighScore(uint32_t score);
    uint32_t getGame2048BestScore() const { return _game2048BestScore; }
    void setGame2048BestScore(uint32_t score);
    uint32_t getBreakoutHighScore() const { return _breakoutHighScore; }
    void setBreakoutHighScore(uint32_t score);

    uint8_t getDisplayTheme() const { return _displayTheme; }
    void setDisplayTheme(uint8_t theme);

    FontStyle getFontStyle() const { return _fontStyle; }
    void setFontStyle(FontStyle style);

    // 空闲信息轮播（天气 / Crypto / Market / 时间）
    bool getCarouselEnabled() const { return _carouselEnabled; }
    void setCarouselEnabled(bool enabled);
    uint8_t getCarouselSpeedSeconds() const { return _carouselSpeedSeconds; }
    void setCarouselSpeedSeconds(uint8_t seconds);
    uint8_t getCarouselView(uint8_t index) const;
    bool setCarouselOrder(const uint8_t order[CAROUSEL_VIEW_COUNT]);
    uint8_t getCarouselFixedView() const { return _carouselFixedView; }
    void setCarouselFixedView(uint8_t view);

    bool getNightDimEnabled() const { return _nightDimEnabled; }
    void setNightDimEnabled(bool enabled);

    uint8_t getNightStartHour() const { return _nightStartHour; }
    uint8_t getNightEndHour() const { return _nightEndHour; }
    void setNightHours(uint8_t startHour, uint8_t endHour);

    uint8_t getNightBrightnessPercent() const { return _nightBrightnessPercent; }
    void setNightBrightnessPercent(uint8_t percent);

    // Live Ledger 自动上下班调度（轻量常驻配置，计薪模块本身仍按需加载）
    bool getSalaryAutoEnabled() const { return _salaryAutoEnabled; }
    uint16_t getSalaryStartMinutes() const { return _salaryStartMinutes; }
    uint16_t getSalaryEndMinutes() const { return _salaryEndMinutes; }
    uint32_t getSalaryLastAutoDate() const { return _salaryLastAutoDate; }
    uint32_t getSalaryLastAutoEndDate() const {
        return _salaryLastAutoEndDate;
    }
    bool setSalarySchedule(bool enabled, uint16_t startMinutes,
                           uint16_t endMinutes);
    void setSalaryLastAutoDate(uint32_t dateKey);
    void setSalaryLastAutoEndDate(uint32_t dateKey);
    uint16_t getSalaryScheduleProgressPermille(
        TimeService* timeService) const;

    bool isNightDimActive(TimeService* timeService) const;
    String getJson() const;

    String getDeviceName() const { return _deviceName; }
    String getBootLine1() const { return _bootLine1; }
    String getBootLine2() const { return _bootLine2; }
    ExpressionId getDefaultExpression() const { return _defaultExpression; }
    ExpressionMode getExpressionMode() const { return _expressionMode; }
    bool setDeviceName(const String& value);
    bool setBootLine1(const String& value);
    bool setBootLine2(const String& value);
    bool setDefaultExpression(ExpressionId value);
    bool setExpressionMode(ExpressionMode value);
    void resetProfile();
    String getProfileJson() const;
    static bool isValidProfileText(const String& value, size_t maxLength,
                                   bool allowEmpty);

private:
    Preferences _prefs;
    String _defaultBgHex = "#aa4818";
    uint8_t _animSpeed = 1;
    uint8_t _startupView = 0;
    uint8_t _brightnessPercent = 100;
    bool _claudeStatusEnabled = true;
    uint32_t _dinoHighScore = 0;
    uint8_t _sokobanLevel = 0;
    uint32_t _sokobanCompletedMask = 0;
    uint32_t _tetrisHighScore = 0;
    uint32_t _snakeHighScore = 0;
    uint32_t _game2048BestScore = 0;
    uint32_t _breakoutHighScore = 0;
    uint8_t _displayTheme = THEME_ORANGE_WHITE;
    FontStyle _fontStyle = FontStyle::PIXEL;
    bool _carouselEnabled = false;
    uint8_t _carouselSpeedSeconds = 12;
    uint8_t _carouselOrder[CAROUSEL_VIEW_COUNT] = {
        VIEW_WEATHER, VIEW_CRYPTO, VIEW_MARKET, VIEW_CLOCK, VIEW_SALARY
    };
    uint8_t _carouselFixedView = VIEW_WEATHER;
    bool _nightDimEnabled = false;
    uint8_t _nightStartHour = 22;
    uint8_t _nightEndHour = 7;
    uint8_t _nightBrightnessPercent = 25;
    bool _salaryAutoEnabled = true;
    uint16_t _salaryStartMinutes = 9 * 60 + 30;
    uint16_t _salaryEndMinutes = 19 * 60;
    uint32_t _salaryLastAutoDate = 0;
    uint32_t _salaryLastAutoEndDate = 0;
    String _deviceName = "MOCHI";
    String _bootLine1 = "HELLO";
    String _bootLine2 = "MOCHI";
    ExpressionId _defaultExpression = ExpressionId::NORMAL;
    ExpressionMode _expressionMode = ExpressionMode::MANUAL;

    bool isValidHexColor(const String& hex) const;
    bool isStartupViewAllowed(uint8_t view) const;
    bool isCarouselView(uint8_t view) const;
};

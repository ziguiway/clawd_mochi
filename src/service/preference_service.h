#pragma once

#include <Arduino.h>
#include <Preferences.h>

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

    bool getNightDimEnabled() const { return _nightDimEnabled; }
    void setNightDimEnabled(bool enabled);

    uint8_t getNightStartHour() const { return _nightStartHour; }
    uint8_t getNightEndHour() const { return _nightEndHour; }
    void setNightHours(uint8_t startHour, uint8_t endHour);

    uint8_t getNightBrightnessPercent() const { return _nightBrightnessPercent; }
    void setNightBrightnessPercent(uint8_t percent);

    bool isNightDimActive(TimeService* timeService) const;
    String getJson() const;

private:
    Preferences _prefs;
    String _defaultBgHex = "#aa4818";
    uint8_t _animSpeed = 1;
    uint8_t _startupView = 0;
    uint8_t _brightnessPercent = 100;
    bool _nightDimEnabled = false;
    uint8_t _nightStartHour = 22;
    uint8_t _nightEndHour = 7;
    uint8_t _nightBrightnessPercent = 25;

    bool isValidHexColor(const String& hex) const;
    bool isStartupViewAllowed(uint8_t view) const;
};

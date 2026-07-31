#pragma once

#include <Arduino.h>
#include <stdint.h>

// Stable IDs are persisted in NVS and exported configuration files.
enum class FontStyle : uint8_t {
    PIXEL = 0,
    COURIER = 1,
    TERMINAL = 2,
    DASHBOARD = 3,
    COUNT = 4,
};

inline bool isValidFontStyle(uint8_t value) {
    return value < static_cast<uint8_t>(FontStyle::COUNT);
}

inline const char* fontStyleToName(FontStyle style) {
    switch (style) {
        case FontStyle::PIXEL: return "pixel";
        case FontStyle::COURIER: return "courier";
        case FontStyle::TERMINAL: return "terminal";
        case FontStyle::DASHBOARD: return "dashboard";
        default: return "pixel";
    }
}

inline bool fontStyleFromName(const String& name, FontStyle& style) {
    if (name == "pixel") style = FontStyle::PIXEL;
    else if (name == "courier") style = FontStyle::COURIER;
    else if (name == "terminal") style = FontStyle::TERMINAL;
    else if (name == "dashboard") style = FontStyle::DASHBOARD;
    else return false;
    return true;
}

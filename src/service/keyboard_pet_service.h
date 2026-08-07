#pragma once

#include <Arduino.h>
#include <WiFiUdp.h>
#include "../config/cfg_keyboard_pet.h"

class KeyboardPetService {
public:
    enum class Paw : uint8_t { NONE, LEFT, RIGHT, BOTH };

    KeyboardPetService();
    void init();
    void update();
    Paw paw() const { return _paw; }
    bool enabled() const { return _enabled; }
    void setEnabled(bool enabled) { _enabled = enabled; }

private:
    WiFiUDP _udp;
    Paw _paw;
    bool _enabled;
    bool _initialized;
    unsigned long _leftUntil;
    unsigned long _rightUntil;
    void process(const char* packet);
};

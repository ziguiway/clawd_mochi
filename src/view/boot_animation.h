#pragma once

#include <Arduino.h>
#include "../hardware/tft_display.h"
#include <WebServer.h>

namespace BootAnimation {
    void run(TftDisplay& tft, const String& line1 = "HELLO",
             const String& line2 = "MOCHI", WebServer* server = nullptr);
}

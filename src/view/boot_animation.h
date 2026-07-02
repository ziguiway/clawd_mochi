#pragma once

#include <Arduino.h>
#include "../hardware/tft_display.h"
#include <WebServer.h>

namespace BootAnimation {
    void run(TftDisplay& tft, WebServer* server = nullptr);
}

#pragma once

#include <Arduino.h>

class TftDisplay;
class WifiConfigService;

// BOOT 按钮服务:长按 5 秒恢复出厂设置
class BootButtonService {
public:
    BootButtonService(TftDisplay* tft, WifiConfigService* wifiService);

    void init();
    void update();

private:
    TftDisplay* _tft;
    WifiConfigService* _wifiService;
    unsigned long _pressStart;
    bool _pressed;
};

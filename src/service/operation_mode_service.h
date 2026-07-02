#pragma once

#include <Arduino.h>

// 运行模式服务:管理设备整体运行模式(可扩展)
// 当前两种互斥模式:LAN / SERIAL
// 后续可扩展:BLE、USB_CDC_ONLY、AP_ONLY 等
class OperationModeService {
public:
    enum class Mode {
        LAN,        // 局域网:WiFi+UDP+Web 遥控
        SERIAL      // 串口:无 WiFi,状态走串口 cc 命令
    };

    OperationModeService();

    // 启动时检测:串口等 detectMs 内有输入则进 SERIAL,否则 LAN
    void detectAtStartup(uint32_t detectMs = 3000);

    // 直接设置模式(运行中切换)
    void setMode(Mode mode);
    Mode getMode() const { return _mode; }

    bool isSerial() const { return _mode == Mode::SERIAL; }
    bool isLAN() const { return _mode == Mode::LAN; }

    // 模式名称(用于显示)
    const char* getModeName() const;

private:
    Mode _mode;
};

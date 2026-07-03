#pragma once

class TftDisplay;
class TimeService;
class ClaudeCodeService;
class WifiConfigService;
class WebService;
class DisplayService;
class SerialCommandService;
class OperationModeService;
class BootButtonService;

// 应用上下文接口:状态类通过此接口访问共享服务
// AppStateMachine 实现此接口,打破循环依赖
class IAppContext {
public:
    virtual ~IAppContext() = default;
    virtual TftDisplay*           tft()        = 0;
    virtual TimeService*          time()       = 0;
    virtual ClaudeCodeService*    cc()         = 0;
    virtual WifiConfigService*    wifi()       = 0;
    virtual WebService*           web()        = 0;
    virtual DisplayService*       display()    = 0;
    virtual SerialCommandService* serial()     = 0;
    virtual OperationModeService* opMode()     = 0;
    virtual BootButtonService*    bootButton() = 0;
};

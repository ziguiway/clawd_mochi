#pragma once

#include <Arduino.h>
#include <WiFiServer.h>
#include <WiFiClient.h>
#include "../config/cfg_stream.h"

class TftDisplay;

// 桌面无线投屏服务:TCP 接收 PC 上位机推送的 JPEG 帧并解码显示。
// 遵循 RAM 纪律:仅在桌面投屏视图激活时 begin() 并懒分配 JPEG 缓冲,
// 退出视图时 end() 立即断开客户端、停止监听并释放缓冲。
class DesktopStreamService {
public:
    explicit DesktopStreamService(TftDisplay* tft);
    ~DesktopStreamService();

    // 进入投屏视图:分配缓冲 + 启动 TCP 监听。缓冲分配失败返回 false。
    bool begin();
    // 退出投屏视图:断开客户端、停止监听、释放缓冲。
    void end();
    // 主循环调用:接客户端、读帧、解码推屏。
    void update();

    bool isActive() const { return _active; }
    bool isClientConnected() { return _client && _client.connected(); }
    uint32_t getFrameCount() const { return _frameCount; }
    float getFps() const { return _fps; }

    // 无内存可用时由 DisplayService 调用绘制静态提示(一次性,不闪烁)
    void drawOutOfMemoryPage();
    void drawWaitingPage();

private:
    bool readExact(uint8_t* dst, size_t length, uint32_t timeoutMs);
    bool receiveOneFrame();
    void dropClient(const char* reason);
    bool allocateBuffer();
    void releaseBuffer();

    TftDisplay* _tft;
    WiFiServer* _server;
    WiFiClient _client;
    uint8_t* _jpegBuffer;
    bool _active;
    bool _bufferReady;
    uint32_t _frameCount;
    uint32_t _fpsWindowStartMs;
    uint32_t _fpsWindowFrames;
    float _fps;
    uint8_t _consecutiveErrors;
    unsigned long _lastFrameMs;
};

#pragma once

// 把外部 HTTPS 请求放进后台任务时，ESP32-C3 仍不适合同时建立多个 TLS
// 连接。这个轻量闸门保证同一时刻只有一个后台网络请求，主循环不被阻塞。
class NetworkRequestGate {
public:
    static bool tryAcquire();
    static void release();
};

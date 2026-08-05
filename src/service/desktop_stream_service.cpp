#include "desktop_stream_service.h"

#include <TJpg_Decoder.h>
#include <esp_heap_caps.h>

#include "../hardware/tft_display.h"
#include "../config/cfg_display.h"
#include "../utils/logger.h"
#include "../utils/memory_monitor.h"

namespace {
// TJpg_Decoder 回调需要静态上下文;同时只允许一个投屏服务实例(固件单例持有)。
TftDisplay* s_tft = nullptr;

bool streamJpegOutput(int16_t x, int16_t y, uint16_t w, uint16_t h,
                      uint16_t* bitmap) {
    if (!s_tft) return false;
    if (x >= CFG_DISPLAY_WIDTH || y >= CFG_DISPLAY_HEIGHT) return false;
    if (x + w > CFG_DISPLAY_WIDTH) w = CFG_DISPLAY_WIDTH - x;
    if (y + h > CFG_DISPLAY_HEIGHT) h = CFG_DISPLAY_HEIGHT - y;
    if (w == 0 || h == 0) return false;
    s_tft->pushRgb565Rect(x, y, w, h, bitmap);
    return true;
}

uint32_t readLe32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}
}  // namespace

DesktopStreamService::DesktopStreamService(TftDisplay* tft)
    : _tft(tft)
    , _server(nullptr)
    , _jpegBuffer(nullptr)
    , _active(false)
    , _bufferReady(false)
    , _frameCount(0)
    , _fpsWindowStartMs(0)
    , _fpsWindowFrames(0)
    , _fps(0.0f)
    , _consecutiveErrors(0)
    , _lastFrameMs(0) {
}

DesktopStreamService::~DesktopStreamService() {
    end();
}

bool DesktopStreamService::allocateBuffer() {
    if (_jpegBuffer) return true;
    _jpegBuffer = static_cast<uint8_t*>(heap_caps_malloc(
        CFG_STREAM_JPEG_BUFFER_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (!_jpegBuffer) {
        LOG_ERROR("Stream", "JPEG 缓冲分配失败 %u 字节",
                  static_cast<unsigned>(CFG_STREAM_JPEG_BUFFER_BYTES));
        return false;
    }
    MemoryMonitor::logSnapshot("stream buffer loaded");
    return true;
}

void DesktopStreamService::releaseBuffer() {
    if (!_jpegBuffer) return;
    heap_caps_free(_jpegBuffer);
    _jpegBuffer = nullptr;
    MemoryMonitor::logSnapshot("stream buffer released");
}

bool DesktopStreamService::begin() {
    if (_active) return _bufferReady;
    _active = true;
    _frameCount = 0;
    _fps = 0.0f;
    _fpsWindowFrames = 0;
    _fpsWindowStartMs = millis();
    _consecutiveErrors = 0;
    _lastFrameMs = 0;

    if (!allocateBuffer()) {
        _bufferReady = false;
        drawOutOfMemoryPage();
        return false;
    }
    _bufferReady = true;

    s_tft = _tft;
    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(streamJpegOutput);

    if (!_server) {
        _server = new (std::nothrow) WiFiServer(CFG_STREAM_TCP_PORT);
    }
    if (_server) {
        _server->begin();
        _server->setNoDelay(true);
    }
    LOG_INFO("Stream", "桌面投屏服务启动 端口: %d", CFG_STREAM_TCP_PORT);
    return true;
}

void DesktopStreamService::end() {
    if (!_active && !_jpegBuffer) return;
    if (_client) _client.stop();
    if (_server) {
        _server->end();
        _server->close();
        delete _server;
        _server = nullptr;
    }
    releaseBuffer();
    if (s_tft == _tft) s_tft = nullptr;
    _active = false;
    _bufferReady = false;
    LOG_INFO("Stream", "桌面投屏服务停止,共 %u 帧",
             static_cast<unsigned>(_frameCount));
}

void DesktopStreamService::update() {
    if (!_active || !_bufferReady || !_server) return;

    // 接新客户端(单客户端,新连接替换旧连接)
    if (!_client || !_client.connected()) {
        if (_client) _client.stop();
        WiFiClient candidate = _server->available();
        if (candidate) {
            _client = candidate;
            _client.setNoDelay(true);
            _client.setTimeout(CFG_STREAM_FRAME_READ_TIMEOUT_MS);
            _consecutiveErrors = 0;
            LOG_INFO("Stream", "PC 客户端已连接: %s",
                     _client.remoteIP().toString().c_str());
        }
        return;
    }

    if (!receiveOneFrame()) {
        _consecutiveErrors++;
        if (_consecutiveErrors >= CFG_STREAM_MAX_FRAME_ERRORS) {
            dropClient("连续帧错误");
        }
        return;
    }
    _consecutiveErrors = 0;

    // FPS 统计(2 秒窗口)
    _frameCount++;
    _fpsWindowFrames++;
    _lastFrameMs = millis();
    const uint32_t elapsed = _lastFrameMs - _fpsWindowStartMs;
    if (elapsed >= 2000) {
        _fps = _fpsWindowFrames * 1000.0f / elapsed;
        _fpsWindowFrames = 0;
        _fpsWindowStartMs = _lastFrameMs;
        LOG_DEBUG("Stream", "FPS: %.1f 总帧: %u", _fps,
                  static_cast<unsigned>(_frameCount));
    }
}

bool DesktopStreamService::readExact(uint8_t* dst, size_t length,
                                     uint32_t timeoutMs) {
    size_t received = 0;
    uint32_t lastProgress = millis();
    while (received < length) {
        if (!_client.connected()) return false;
        int avail = _client.available();
        if (avail > 0) {
            size_t wanted = length - received;
            if (wanted > static_cast<size_t>(avail)) wanted = avail;
            int n = _client.read(dst + received, wanted);
            if (n > 0) {
                received += static_cast<size_t>(n);
                lastProgress = millis();
                continue;
            }
        }
        if (millis() - lastProgress > timeoutMs) return false;
        delay(1);
    }
    return true;
}

bool DesktopStreamService::receiveOneFrame() {
    uint8_t header[8];
    if (!readExact(header, sizeof(header), CFG_STREAM_FRAME_READ_TIMEOUT_MS)) {
        return false;
    }
    if (header[0] != CFG_STREAM_MAGIC_0 || header[1] != CFG_STREAM_MAGIC_1 ||
        header[2] != CFG_STREAM_MAGIC_2 || header[3] != CFG_STREAM_MAGIC_3) {
        LOG_WARN("Stream", "帧头魔数错误 %02X %02X %02X %02X",
                 header[0], header[1], header[2], header[3]);
        return false;
    }
    const uint32_t jpegLength = readLe32(header + 4);
    if (jpegLength == 0 || jpegLength > CFG_STREAM_JPEG_BUFFER_BYTES) {
        LOG_WARN("Stream", "拒绝 JPEG 帧: %u 字节(容量 %u),请降低 PC 端质量",
                 static_cast<unsigned>(jpegLength),
                 static_cast<unsigned>(CFG_STREAM_JPEG_BUFFER_BYTES));
        return false;
    }
    if (!readExact(_jpegBuffer, jpegLength, CFG_STREAM_FRAME_READ_TIMEOUT_MS)) {
        return false;
    }
    const JRESULT result = TJpgDec.drawJpg(0, 0, _jpegBuffer, jpegLength);
    if (result != JDR_OK) {
        LOG_WARN("Stream", "JPEG 解码失败: %d", static_cast<int>(result));
        return false;
    }
    return true;
}

void DesktopStreamService::dropClient(const char* reason) {
    LOG_WARN("Stream", "断开 PC 客户端: %s", reason);
    if (_client) _client.stop();
    _consecutiveErrors = 0;
    drawWaitingPage();
}

void DesktopStreamService::drawWaitingPage() {
    _tft->fillScreen(COLOR_BLACK);
    _tft->drawTextCentered(84, "Desktop Stream", COLOR_ORANGE, COLOR_BLACK, 2);
    _tft->drawTextCentered(118, "Waiting for PC...", COLOR_WHITE, COLOR_BLACK, 1);
    _tft->drawTextCentered(140, "Port 3333", COLOR_GRAY, COLOR_BLACK, 1);
}

void DesktopStreamService::drawOutOfMemoryPage() {
    _tft->fillScreen(COLOR_BLACK);
    _tft->drawTextCentered(100, "Out of memory", COLOR_RED, COLOR_BLACK, 1);
    _tft->drawTextCentered(124, "Close other views", COLOR_GRAY, COLOR_BLACK, 1);
}

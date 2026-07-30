#pragma once

#include <Arduino.h>

namespace MemoryMonitor {

// TLS 握手既需要足够的总堆，也需要一块足够大的连续内存。
// 低于安全线时延后请求，避免把设备推入不可恢复的内存压力。
constexpr size_t TLS_MIN_FREE_BYTES = 80U * 1024U;
constexpr size_t TLS_MIN_LARGEST_BLOCK_BYTES = 64U * 1024U;

void logSnapshot(const char* tag);
bool hasTlsHeadroom(const char* tag);

}


#pragma once

#include <Arduino.h>

// 小恐龙和推箱子使用相同尺寸的 1-bit 画布，且同一时间只会运行一个游戏。
// 缓冲由 DisplayService 统一持有并复用，避免每个游戏常驻一份。
namespace GameRenderBuffer {
constexpr size_t MONO_FRAME_BYTES = 30U * 240U;
}


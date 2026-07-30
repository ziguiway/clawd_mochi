#pragma once

#include <Arduino.h>

enum class ArcadeGameId : uint8_t {
    DINO = 0,
    SOKOBAN,
    TETRIS,
    SNAKE,
    MERGE_2048,
    BREAKOUT,
    COUNT
};

// 游戏模块的统一生命周期。每款游戏保留独立规则与状态，
// DisplayService 只负责注册、切换和转发控制指令。
class IArcadeGame {
public:
    virtual ~IArcadeGame() = default;
    virtual ArcadeGameId id() const = 0;
    virtual const char* slug() const = 0;
    virtual void begin() = 0;
    virtual void stop() = 0;
    virtual void update() = 0;
    virtual void redraw() = 0;
    virtual bool handleAction(const String& action, int value = 0) = 0;
    virtual bool isActive() const = 0;
    virtual String getStateJson() const = 0;
};


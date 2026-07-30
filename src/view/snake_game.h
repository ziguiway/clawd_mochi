#pragma once

#include "arcade_game.h"

class ArcadeCanvas;
class PreferenceService;

class SnakeGame : public IArcadeGame {
public:
    SnakeGame(ArcadeCanvas* canvas, PreferenceService* preferences);

    ArcadeGameId id() const override { return ArcadeGameId::SNAKE; }
    const char* slug() const override { return "snake"; }
    void begin() override;
    void stop() override;
    void update() override;
    void redraw() override;
    bool handleAction(const String& action, int value = 0) override;
    bool isActive() const override { return _active; }
    String getStateJson() const override;

private:
    enum class State : uint8_t { READY, PLAYING, PAUSED, GAME_OVER };
    enum class Direction : uint8_t { UP, RIGHT, DOWN, LEFT };
    static constexpr uint8_t GRID = 20;
    static constexpr uint16_t MAX_LENGTH = GRID * GRID;

    ArcadeCanvas* _canvas;
    PreferenceService* _preferences;
    bool _active;
    State _state;
    Direction _direction;
    Direction _queuedDirection;
    uint8_t _snakeX[MAX_LENGTH];
    uint8_t _snakeY[MAX_LENGTH];
    uint16_t _length;
    uint8_t _foodX;
    uint8_t _foodY;
    uint16_t _score;
    uint16_t _highScore;
    unsigned long _lastStepMs;
    bool _dirty;

    void reset();
    void placeFood();
    bool occupies(uint8_t x, uint8_t y, uint16_t limit) const;
    bool isOpposite(Direction a, Direction b) const;
    uint16_t stepIntervalMs() const;
    void step();
    void finish();
    void render();
};

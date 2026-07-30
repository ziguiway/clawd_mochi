#pragma once

#include "arcade_game.h"

class ArcadeCanvas;
class PreferenceService;

class BreakoutGame : public IArcadeGame {
public:
    BreakoutGame(ArcadeCanvas* canvas, PreferenceService* preferences);

    ArcadeGameId id() const override { return ArcadeGameId::BREAKOUT; }
    const char* slug() const override { return "breakout"; }
    void begin() override;
    void stop() override;
    void update() override;
    void redraw() override;
    bool handleAction(const String& action, int value = 0) override;
    bool isActive() const override { return _active; }
    String getStateJson() const override;

private:
    enum class State : uint8_t { READY, PLAYING, PAUSED, GAME_OVER };
    static constexpr uint8_t BRICK_ROWS = 6;
    static constexpr uint8_t BRICK_COLS = 8;

    ArcadeCanvas* _canvas;
    PreferenceService* _preferences;
    bool _active;
    State _state;
    uint8_t _bricks[BRICK_ROWS][BRICK_COLS];
    float _paddleX;
    float _ballX;
    float _ballY;
    float _ballVx;
    float _ballVy;
    uint8_t _lives;
    uint8_t _level;
    uint16_t _bricksRemaining;
    uint32_t _score;
    uint32_t _highScore;
    unsigned long _lastUpdateMs;
    unsigned long _lastFrameMs;
    bool _dirty;

    void resetGame();
    void buildLevel();
    void resetBall();
    void launch();
    void movePaddle(float delta);
    void step(float dt);
    bool hitBrick(float previousX, float previousY);
    void loseLife();
    void advanceLevel();
    void render();
};

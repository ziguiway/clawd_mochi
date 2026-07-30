#pragma once

#include "arcade_game.h"

class ArcadeCanvas;
class PreferenceService;

class Game2048 : public IArcadeGame {
public:
    Game2048(ArcadeCanvas* canvas, PreferenceService* preferences);

    ArcadeGameId id() const override { return ArcadeGameId::MERGE_2048; }
    const char* slug() const override { return "2048"; }
    void begin() override;
    void stop() override;
    void update() override {}
    void redraw() override;
    bool handleAction(const String& action, int value = 0) override;
    bool isActive() const override { return _active; }
    String getStateJson() const override;

private:
    enum class State : uint8_t { PLAYING, WON, GAME_OVER };
    enum class Direction : uint8_t { UP, RIGHT, DOWN, LEFT };

    ArcadeCanvas* _canvas;
    PreferenceService* _preferences;
    bool _active;
    State _state;
    uint16_t _board[4][4];
    uint16_t _previous[4][4];
    uint32_t _score;
    uint32_t _previousScore;
    uint32_t _bestScore;
    bool _canUndo;

    void reset();
    bool move(Direction direction);
    bool slideLine(uint16_t line[4]);
    void addTile();
    bool canMove() const;
    uint16_t maxTile() const;
    void render();
    uint16_t tileColor(uint16_t value) const;
    uint16_t tileTextColor(uint16_t value) const;
    void drawCenteredNumber(int16_t x, int16_t y, int16_t size,
                            uint16_t value, uint16_t color);
};

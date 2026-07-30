#pragma once

#include "arcade_game.h"

class ArcadeCanvas;
class PreferenceService;

class TetrisGame : public IArcadeGame {
public:
    TetrisGame(ArcadeCanvas* canvas, PreferenceService* preferences);

    ArcadeGameId id() const override { return ArcadeGameId::TETRIS; }
    const char* slug() const override { return "tetris"; }
    void begin() override;
    void stop() override;
    void update() override;
    void redraw() override;
    bool handleAction(const String& action, int value = 0) override;
    bool isActive() const override { return _active; }
    String getStateJson() const override;

private:
    enum class State : uint8_t { PLAYING, PAUSED, GAME_OVER };
    static constexpr uint8_t BOARD_W = 10;
    static constexpr uint8_t BOARD_H = 20;

    ArcadeCanvas* _canvas;
    PreferenceService* _preferences;
    bool _active;
    State _state;
    uint8_t _board[BOARD_H][BOARD_W];
    uint8_t _piece;
    uint8_t _nextPiece;
    uint8_t _rotation;
    int8_t _pieceX;
    int8_t _pieceY;
    uint8_t _bag[7];
    uint8_t _bagIndex;
    uint32_t _score;
    uint32_t _highScore;
    uint16_t _lines;
    uint8_t _level;
    unsigned long _lastDropMs;
    bool _dirty;

    void reset();
    void refillBag();
    uint8_t takePiece();
    bool pieceCell(uint8_t piece, uint8_t rotation,
                   uint8_t x, uint8_t y) const;
    bool collides(int8_t x, int8_t y, uint8_t rotation) const;
    bool moveHorizontal(int8_t dx);
    bool rotate();
    bool softDrop(bool award);
    void hardDrop();
    void lockPiece();
    void clearLines();
    void spawnPiece();
    int8_t ghostY() const;
    uint16_t gravityMs() const;
    void render();
    void drawCell(int16_t x, int16_t y, uint8_t colorIndex,
                  bool ghost = false);
};

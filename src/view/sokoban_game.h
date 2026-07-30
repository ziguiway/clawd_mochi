#pragma once

#include <Arduino.h>
#include "arcade_game.h"

class TftDisplay;
class PreferenceService;

class SokobanGame : public IArcadeGame {
public:
    enum class State : uint8_t {
        PLAYING,
        COMPLETE
    };

    SokobanGame(TftDisplay* tft, PreferenceService* preferences);

    ArcadeGameId id() const override { return ArcadeGameId::SOKOBAN; }
    const char* slug() const override { return "sokoban"; }
    void begin() override;
    void stop() override;
    void update() override {}
    bool move(int8_t dx, int8_t dy);
    bool undo();
    void restart();
    bool selectLevel(uint8_t index);
    void redraw() override;
    bool handleAction(const String& action, int value = 0) override;

    bool isActive() const override { return _active; }
    uint8_t getLevel() const { return _levelIndex; }
    String getStateJson() const override;

private:
    enum Tile : uint8_t {
        VOID_TILE,
        FLOOR,
        WALL,
        GOAL
    };

    struct HistoryEntry {
        uint8_t playerX;
        uint8_t playerY;
        uint8_t boxX;
        uint8_t boxY;
    };

    static constexpr uint8_t MAX_WIDTH = 22;
    static constexpr uint8_t MAX_HEIGHT = 18;
    static constexpr uint16_t MAX_CELLS = MAX_WIDTH * MAX_HEIGHT;
    static constexpr uint8_t HISTORY_SIZE = 128;
    static constexpr uint8_t FRAME_STRIDE = 30;
    static constexpr uint16_t FRAME_BYTES = FRAME_STRIDE * 240;

    TftDisplay* _tft;
    PreferenceService* _preferences;
    bool _active;
    State _state;
    uint8_t _levelIndex;
    uint8_t _width;
    uint8_t _height;
    uint8_t _playerX;
    uint8_t _playerY;
    uint8_t _boxCount;
    uint16_t _moves;
    uint16_t _pushes;
    uint32_t _completedMask;
    Tile _tiles[MAX_CELLS];
    bool _boxes[MAX_CELLS];
    HistoryEntry _history[HISTORY_SIZE];
    uint8_t _historyCount;
    uint8_t _frameBuffer[FRAME_BYTES];

    bool loadLevel(uint8_t index);
    void markExteriorVoid();
    bool isWalkable(int16_t x, int16_t y) const;
    bool hasBox(int16_t x, int16_t y) const;
    uint16_t cellIndex(uint8_t x, uint8_t y) const;
    uint8_t boxesOnGoals() const;
    bool isComplete() const;
    void completeLevel();

    void render();
    void flushFrame();
    void clearFrame();
    void setPixel(int16_t x, int16_t y, bool on = true);
    void fillBufferRect(int16_t x, int16_t y, int16_t w, int16_t h,
                        bool on = true);
    void drawBufferRect(int16_t x, int16_t y, int16_t w, int16_t h);
    void drawTinyText(int16_t x, int16_t y, const char* text, uint8_t scale);
    void drawWall(int16_t x, int16_t y, uint8_t size);
    void drawGoal(int16_t x, int16_t y, uint8_t size);
    void drawBox(int16_t x, int16_t y, uint8_t size, bool onGoal);
    void drawPlayer(int16_t x, int16_t y, uint8_t size);
};

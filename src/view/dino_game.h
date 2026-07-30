#pragma once

#include <Arduino.h>
#include "dino_sprites.h"
#include "arcade_game.h"

class TftDisplay;
class PreferenceService;

// 单键小恐龙游戏。物理、碰撞和整帧刷新都封装在这里，
// DisplayService 只负责进入/退出和驱动 update()。
class DinoGame : public IArcadeGame {
public:
    enum class State : uint8_t {
        READY,
        RUNNING,
        GAME_OVER
    };

    DinoGame(TftDisplay* tft, PreferenceService* preferences);

    ArcadeGameId id() const override { return ArcadeGameId::DINO; }
    const char* slug() const override { return "dino"; }
    void begin() override;
    void stop() override;
    void update() override;
    void jump();
    void restart();
    void redraw() override;
    bool handleAction(const String& action, int value = 0) override {
        (void)value;
        if (action == "jump") { jump(); return true; }
        if (action == "restart") { restart(); return true; }
        return false;
    }

    bool isActive() const override { return _active; }
    State getState() const { return _state; }
    uint32_t getScore() const { return _score; }
    uint32_t getHighScore() const { return _highScore; }
    uint16_t getSpeed() const { return _speedPxPerSec; }
    String getStateJson() const override;

private:
    enum class CactusType : uint8_t {
        SMALL,
        LARGE
    };

    static constexpr int16_t PLAYFIELD_Y = 0;
    static constexpr int16_t PLAYFIELD_H = 240;
    static constexpr int16_t FRAME_STRIDE = 30;
    static constexpr int16_t GROUND_Y = 204;
    static constexpr int16_t DINO_X = 24;
    static constexpr int16_t DINO_W = 44;
    static constexpr int16_t DINO_H = 47;

    TftDisplay* _tft;
    PreferenceService* _preferences;
    bool _active;
    State _state;
    int32_t _dinoYMilli;
    int32_t _velocityYMilli;
    int32_t _obstacleXMilli;
    CactusType _cactusType;
    uint8_t _cactusCount;
    int32_t _cloudXMilli;
    int16_t _cloudY;
    int32_t _groundOffsetMilli;
    uint32_t _distanceMilli;
    uint32_t _score;
    uint32_t _highScore;
    uint16_t _speedPxPerSec;
    unsigned long _lastUpdateMs;
    unsigned long _lastFrameMs;
    bool _legFrame;
    uint8_t _frameBuffer[FRAME_STRIDE * PLAYFIELD_H];

    void startRunning();
    void resetRound();
    void finishRound();
    void resetObstacle(int16_t x);
    void resetCloud(int16_t x);
    bool collides() const;

    void drawScene();
    void composePlayfield();
    void flushPlayfield();
    void clearPlayfield();
    void drawHudToBuffer();
    void drawGroundToBuffer();
    void drawDinoToBuffer();
    void drawCactusToBuffer();
    void drawCloudToBuffer();
    void drawSpriteToBuffer(int16_t x, int16_t y,
                            const DinoSprites::Sprite& sprite,
                            int16_t visibleWidth = -1);
    void setBufferPixel(int16_t x, int16_t y);
    void drawBufferHLine(int16_t x, int16_t y, int16_t width);
    void drawTinyTextToBuffer(int16_t x, int16_t y, const char* text,
                              uint8_t scale);
    void drawCenteredMessage(const char* line1, const char* line2);

    int16_t cactusUnitWidth() const;
    int16_t cactusHeight() const;
    int16_t cactusWidth() const;
};

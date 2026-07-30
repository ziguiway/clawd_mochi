#include "dino_game.h"

#include "../config/cfg_display.h"
#include "../hardware/tft_display.h"
#include "../service/preference_service.h"

namespace {
// 单次触摸固定跳跃：约 1.3 秒滞空、初速下约 120px 水平距离。
constexpr int32_t GRAVITY_MILLI = 500 * 1000;
constexpr int32_t JUMP_VELOCITY_MILLI = -325 * 1000;
constexpr uint16_t CLOUD_SPEED_PX_PER_SEC = 28;
constexpr unsigned long FRAME_INTERVAL_MS = 33;

struct CollisionBox {
    int8_t x;
    int8_t y;
    uint8_t width;
    uint8_t height;
};

// Chromium 原版 running 碰撞盒。相比整张矩形更宽容，也更符合视觉轮廓。
constexpr CollisionBox DINO_BOXES[] = {
    {22, 0, 17, 16},
    {1, 18, 30, 9},
    {10, 35, 14, 8},
    {1, 24, 29, 5},
    {5, 30, 21, 4},
    {9, 34, 15, 4},
};

constexpr CollisionBox SMALL_CACTUS_BOXES[] = {
    {0, 7, 5, 27},
    {4, 0, 6, 34},
    {10, 4, 7, 14},
};

constexpr CollisionBox LARGE_CACTUS_BOXES[] = {
    {0, 12, 7, 38},
    {8, 0, 7, 49},
    {13, 10, 10, 38},
};

bool boxesOverlap(int16_t ax, int16_t ay, const CollisionBox& a,
                  int16_t bx, int16_t by, const CollisionBox& b) {
    const int16_t aLeft = ax + a.x;
    const int16_t aTop = ay + a.y;
    const int16_t bLeft = bx + b.x;
    const int16_t bTop = by + b.y;
    return aLeft < bLeft + b.width &&
           aLeft + a.width > bLeft &&
           aTop < bTop + b.height &&
           aTop + a.height > bTop;
}

uint16_t tinyGlyph(char c) {
    switch (c) {
        case '0': return 0b111101101101111;
        case '1': return 0b010110010010111;
        case '2': return 0b111001111100111;
        case '3': return 0b111001111001111;
        case '4': return 0b101101111001001;
        case '5': return 0b111100111001111;
        case '6': return 0b111100111101111;
        case '7': return 0b111001010010010;
        case '8': return 0b111101111101111;
        case '9': return 0b111101111001111;
        case 'H': return 0b101101111101101;
        case 'I': return 0b111010010010111;
        default: return 0;
    }
}
}  // namespace

DinoGame::DinoGame(TftDisplay* tft, PreferenceService* preferences,
                   uint8_t* frameBuffer)
    : _tft(tft)
    , _preferences(preferences)
    , _active(false)
    , _state(State::READY)
    , _dinoYMilli((GROUND_Y - DINO_H) * 1000L)
    , _velocityYMilli(0)
    , _obstacleXMilli(280000L)
    , _cactusType(CactusType::SMALL)
    , _cactusCount(1)
    , _cloudXMilli(165000L)
    , _cloudY(70)
    , _groundOffsetMilli(0)
    , _distanceMilli(0)
    , _score(0)
    , _highScore(0)
    , _speedPxPerSec(92)
    , _lastUpdateMs(0)
    , _lastFrameMs(0)
    , _legFrame(false)
    , _frameBuffer(frameBuffer)
{
}

void DinoGame::begin() {
    _active = true;
    _highScore = _preferences ? _preferences->getDinoHighScore() : 0;
    resetRound();
    drawScene();
}

void DinoGame::stop() {
    _active = false;
}

void DinoGame::resetRound() {
    _state = State::READY;
    _dinoYMilli = (GROUND_Y - DINO_H) * 1000L;
    _velocityYMilli = 0;
    _distanceMilli = 0;
    _score = 0;
    _speedPxPerSec = 92;
    _groundOffsetMilli = 0;
    _legFrame = false;
    resetObstacle(280);
    resetCloud(165);
    _lastUpdateMs = millis();
    _lastFrameMs = 0;
}

void DinoGame::resetObstacle(int16_t x) {
    // 对齐 Chromium 的两类仙人掌，并根据进度逐步开放 2～3 株组合。
    _cactusType = (_score >= 3 && random(100) < 45)
        ? CactusType::LARGE
        : CactusType::SMALL;

    // 原版初始速度已经允许小仙人掌组合；大仙人掌稍后才开放多株。
    uint8_t maxCount = _cactusType == CactusType::SMALL ? 3 : 1;
    if (_cactusType == CactusType::LARGE && _score >= 10) maxCount = 2;
    if (_cactusType == CactusType::LARGE && _score >= 22) maxCount = 3;
    _cactusCount = static_cast<uint8_t>(random(1, maxCount + 1));
    _obstacleXMilli = static_cast<int32_t>(x) * 1000L;
}

void DinoGame::resetCloud(int16_t x) {
    _cloudXMilli = static_cast<int32_t>(x) * 1000L;
    _cloudY = random(58, 101);
}

void DinoGame::startRunning() {
    _state = State::RUNNING;
    _lastUpdateMs = millis();
    _lastFrameMs = 0;
    // 整个游戏区一次性覆盖 READY 提示，不先擦除，所以不会出现橙色空帧。
    composePlayfield();
    flushPlayfield();
}

void DinoGame::jump() {
    if (!_active) return;
    if (_state == State::GAME_OVER) {
        resetRound();
        drawScene();
    }
    if (_state == State::READY) startRunning();

    const int32_t groundY = (GROUND_Y - DINO_H) * 1000L;
    if (_state == State::RUNNING && _dinoYMilli >= groundY - 500) {
        _velocityYMilli = JUMP_VELOCITY_MILLI;
    }
}

void DinoGame::restart() {
    if (!_active) return;
    resetRound();
    drawScene();
}

void DinoGame::update() {
    if (!_active || _state != State::RUNNING) return;

    const unsigned long now = millis();
    if (now - _lastFrameMs < FRAME_INTERVAL_MS) return;
    _lastFrameMs = now;

    uint32_t dtMs = now - _lastUpdateMs;
    _lastUpdateMs = now;
    if (dtMs > 60) dtMs = 60;

    _velocityYMilli += static_cast<int32_t>(
        (static_cast<int64_t>(GRAVITY_MILLI) * dtMs) / 1000L);
    _dinoYMilli += static_cast<int32_t>(
        (static_cast<int64_t>(_velocityYMilli) * dtMs) / 1000L);

    const int32_t groundY = (GROUND_Y - DINO_H) * 1000L;
    if (_dinoYMilli >= groundY) {
        _dinoYMilli = groundY;
        _velocityYMilli = 0;
    }

    _obstacleXMilli -= static_cast<int32_t>(_speedPxPerSec) * dtMs;
    _cloudXMilli -= static_cast<int32_t>(CLOUD_SPEED_PX_PER_SEC) * dtMs;
    _groundOffsetMilli += static_cast<int32_t>(_speedPxPerSec) * dtMs;
    _distanceMilli += static_cast<uint32_t>(_speedPxPerSec) * dtMs;
    // 数字保持清晰可读，不再以每秒近十次的频率跳动。
    _score = _distanceMilli / 50000UL;
    _speedPxPerSec = constrain(92 + static_cast<int>(_score / 5), 92, 180);

    if (_obstacleXMilli < -cactusWidth() * 1000L) {
        resetObstacle(CFG_DISPLAY_WIDTH + random(75, 156));
    }
    if (_cloudXMilli < -DinoSprites::CLOUD.width * 1000L) {
        resetCloud(CFG_DISPLAY_WIDTH + random(45, 181));
    }

    _legFrame = ((now / 100UL) & 1U) != 0;

    if (collides()) {
        finishRound();
        return;
    }

    composePlayfield();
    flushPlayfield();
}

bool DinoGame::collides() const {
    const int16_t dinoY = _dinoYMilli / 1000L;
    const int16_t obstacleX = _obstacleXMilli / 1000L;
    const int16_t unitWidth = cactusUnitWidth();
    const int16_t obstacleY = GROUND_Y - cactusHeight();

    const CollisionBox* cactusBoxes = _cactusType == CactusType::SMALL
        ? SMALL_CACTUS_BOXES
        : LARGE_CACTUS_BOXES;

    for (const CollisionBox& dinoBox : DINO_BOXES) {
        for (uint8_t plant = 0; plant < _cactusCount; plant++) {
            const int16_t plantX = obstacleX + plant * unitWidth;
            for (uint8_t box = 0; box < 3; box++) {
                if (boxesOverlap(DINO_X, dinoY, dinoBox,
                                 plantX, obstacleY, cactusBoxes[box])) {
                    return true;
                }
            }
        }
    }
    return false;
}

void DinoGame::finishRound() {
    _state = State::GAME_OVER;
    if (_score > _highScore) {
        _highScore = _score;
        if (_preferences) _preferences->setDinoHighScore(_highScore);
    }
    drawScene();
}

void DinoGame::redraw() {
    if (_active) drawScene();
}

void DinoGame::drawScene() {
    composePlayfield();
    flushPlayfield();

    if (_state == State::READY) {
        drawCenteredMessage("TAP TO START", "WEB / SPACE");
    } else if (_state == State::GAME_OVER) {
        drawCenteredMessage("GAME OVER", "TAP TO RETRY");
    }
}

void DinoGame::drawHudToBuffer() {
    char high[12];
    char score[8];
    snprintf(high, sizeof(high), "HI %04lu",
             static_cast<unsigned long>(_highScore % 10000UL));
    snprintf(score, sizeof(score), "%04lu",
             static_cast<unsigned long>(_score % 10000UL));
    drawTinyTextToBuffer(10, 10, high, 2);
    drawTinyTextToBuffer(CFG_DISPLAY_WIDTH - 32, 10, score, 2);
}

void DinoGame::composePlayfield() {
    clearPlayfield();
    drawHudToBuffer();
    drawCloudToBuffer();
    drawGroundToBuffer();
    drawCactusToBuffer();
    drawDinoToBuffer();
}

void DinoGame::clearPlayfield() {
    memset(_frameBuffer, 0, FRAME_BYTES);
}

void DinoGame::setBufferPixel(int16_t x, int16_t y) {
    const int16_t localY = y - PLAYFIELD_Y;
    if (x < 0 || x >= CFG_DISPLAY_WIDTH ||
        localY < 0 || localY >= PLAYFIELD_H) {
        return;
    }
    _frameBuffer[localY * FRAME_STRIDE + x / 8] |=
        static_cast<uint8_t>(0x80U >> (x & 7));
}

void DinoGame::drawBufferHLine(int16_t x, int16_t y, int16_t width) {
    if (width <= 0 || y < PLAYFIELD_Y || y >= PLAYFIELD_Y + PLAYFIELD_H) {
        return;
    }
    int16_t start = max<int16_t>(0, x);
    int16_t end = min<int16_t>(CFG_DISPLAY_WIDTH, x + width);
    for (int16_t px = start; px < end; px++) setBufferPixel(px, y);
}

void DinoGame::drawTinyTextToBuffer(int16_t x, int16_t y, const char* text,
                                    uint8_t scale) {
    for (const char* p = text; *p; p++, x += 4 * scale) {
        const uint16_t glyph = tinyGlyph(*p);
        for (uint8_t row = 0; row < 5; row++) {
            for (uint8_t col = 0; col < 3; col++) {
                const uint8_t bit = 14 - (row * 3 + col);
                if (!(glyph & (1U << bit))) continue;
                for (uint8_t yy = 0; yy < scale; yy++) {
                    drawBufferHLine(x + col * scale,
                                    y + row * scale + yy,
                                    scale);
                }
            }
        }
    }
}

void DinoGame::drawSpriteToBuffer(int16_t x, int16_t y,
                                  const DinoSprites::Sprite& sprite,
                                  int16_t visibleWidth) {
    if (visibleWidth < 0 || visibleWidth > sprite.width) {
        visibleWidth = sprite.width;
    }

    for (uint16_t i = 0; i < sprite.runCount; i++) {
        DinoSprites::SpriteRun run;
        memcpy_P(&run, sprite.runs + i, sizeof(run));
        if (run.x >= visibleWidth) continue;
        const int16_t clippedLength =
            min<int16_t>(run.length, visibleWidth - run.x);
        drawBufferHLine(x + run.x, y + run.y, clippedLength);
    }
}

void DinoGame::drawDinoToBuffer() {
    const DinoSprites::Sprite* sprite = &DinoSprites::DINO_IDLE;
    if (_state == State::GAME_OVER) {
        sprite = &DinoSprites::DINO_CRASHED;
    } else if (_state == State::RUNNING && _dinoYMilli / 1000L >= GROUND_Y - DINO_H) {
        sprite = _legFrame ? &DinoSprites::DINO_RUN_2 : &DinoSprites::DINO_RUN_1;
    }
    drawSpriteToBuffer(DINO_X, _dinoYMilli / 1000L, *sprite);
}

void DinoGame::drawCactusToBuffer() {
    const DinoSprites::Sprite& sprite = _cactusType == CactusType::SMALL
        ? DinoSprites::CACTUS_SMALL_GROUP
        : DinoSprites::CACTUS_LARGE_GROUP;
    drawSpriteToBuffer(
        _obstacleXMilli / 1000L,
        GROUND_Y - cactusHeight(),
        sprite,
        cactusWidth());
}

void DinoGame::drawCloudToBuffer() {
    drawSpriteToBuffer(_cloudXMilli / 1000L, _cloudY, DinoSprites::CLOUD);
}

void DinoGame::drawGroundToBuffer() {
    drawBufferHLine(0, GROUND_Y, CFG_DISPLAY_WIDTH);
    drawBufferHLine(0, GROUND_Y + 1, CFG_DISPLAY_WIDTH);

    const int16_t offset = (_groundOffsetMilli / 1000L) % 40;
    for (int16_t x = -offset; x < CFG_DISPLAY_WIDTH + 40; x += 40) {
        drawBufferHLine(x + 5, GROUND_Y + 8, 6);
        drawBufferHLine(x + 22, GROUND_Y + 15, 3);
        drawBufferHLine(x + 31, GROUND_Y + 5, 4);
    }
}

void DinoGame::flushPlayfield() {
    uint16_t scanline[CFG_DISPLAY_WIDTH];
    Adafruit_ST7789& screen = _tft->getTft();

    screen.startWrite();
    screen.setAddrWindow(0, PLAYFIELD_Y, CFG_DISPLAY_WIDTH, PLAYFIELD_H);
    for (int16_t row = 0; row < PLAYFIELD_H; row++) {
        const uint8_t* source = _frameBuffer + row * FRAME_STRIDE;
        for (int16_t x = 0; x < CFG_DISPLAY_WIDTH; x++) {
            scanline[x] = (source[x / 8] & (0x80U >> (x & 7)))
                ? COLOR_WHITE
                : COLOR_ORANGE;
        }
        screen.writePixels(scanline, CFG_DISPLAY_WIDTH, true, false);
    }
    screen.endWrite();
}

void DinoGame::drawCenteredMessage(const char* line1, const char* line2) {
    _tft->fillRect(36, 94, CFG_DISPLAY_WIDTH - 72, 50, COLOR_ORANGE);
    _tft->drawRect(36, 94, CFG_DISPLAY_WIDTH - 72, 50, COLOR_WHITE);
    _tft->drawTextCentered(104, line1, COLOR_WHITE, COLOR_ORANGE, 2);
    _tft->drawTextCentered(128, line2, COLOR_WHITE, COLOR_ORANGE, 1);
}

int16_t DinoGame::cactusUnitWidth() const {
    return _cactusType == CactusType::SMALL ? 17 : 25;
}

int16_t DinoGame::cactusHeight() const {
    return _cactusType == CactusType::SMALL ? 35 : 50;
}

int16_t DinoGame::cactusWidth() const {
    return cactusUnitWidth() * _cactusCount;
}

String DinoGame::getStateJson() const {
    const char* state = "ready";
    if (_state == State::RUNNING) state = "running";
    else if (_state == State::GAME_OVER) state = "game_over";

    String json = "{\"active\":";
    json += _active ? "true" : "false";
    json += ",\"state\":\"";
    json += state;
    json += "\",\"score\":";
    json += _score;
    json += ",\"highScore\":";
    json += _highScore;
    json += ",\"speed\":";
    json += _speedPxPerSec;
    json += "}";
    return json;
}

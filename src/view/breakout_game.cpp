#include "breakout_game.h"

#include <math.h>

#include "arcade_canvas.h"
#include "../config/cfg_display.h"
#include "../service/preference_service.h"

namespace {
constexpr float FIELD_LEFT = 8.0f;
constexpr float FIELD_RIGHT = 232.0f;
constexpr float FIELD_TOP = 29.0f;
constexpr float FIELD_BOTTOM = 235.0f;
constexpr float PADDLE_Y = 218.0f;
constexpr float PADDLE_W = 48.0f;
constexpr float PADDLE_H = 7.0f;
constexpr float BALL_R = 4.0f;

constexpr uint16_t ROW_COLORS[6] = {
    0xF800, 0xFBE0, 0xFFE0, 0x07E0, 0x07FF, 0xA81F
};

const char* breakoutStateName(uint8_t state) {
    switch (state) {
        case 0: return "ready";
        case 1: return "playing";
        case 2: return "paused";
        default: return "game_over";
    }
}
}  // namespace

BreakoutGame::BreakoutGame(ArcadeCanvas* canvas,
                           PreferenceService* preferences)
    : _canvas(canvas), _preferences(preferences), _active(false),
      _state(State::READY), _bricks{}, _paddleX(96), _ballX(120),
      _ballY(210), _ballVx(86), _ballVy(-118), _lives(3), _level(1),
      _bricksRemaining(0), _score(0), _highScore(0), _lastUpdateMs(0),
      _lastFrameMs(0), _dirty(true) {}

void BreakoutGame::begin() {
    _active = true;
    if (_preferences) _highScore = _preferences->getBreakoutHighScore();
    resetGame();
}

void BreakoutGame::stop() {
    if (_preferences) _preferences->setBreakoutHighScore(_highScore);
    _active = false;
}

void BreakoutGame::resetGame() {
    _state = State::READY;
    _score = 0;
    _lives = 3;
    _level = 1;
    _paddleX = 96;
    buildLevel();
    resetBall();
    _lastUpdateMs = millis();
    _lastFrameMs = 0;
    _dirty = true;
    render();
}

void BreakoutGame::buildLevel() {
    _bricksRemaining = 0;
    for (uint8_t row = 0; row < BRICK_ROWS; row++) {
        for (uint8_t col = 0; col < BRICK_COLS; col++) {
            // 高等级加入耐久砖，但保持每关都可完成。
            _bricks[row][col] =
                (_level >= 3 && (row + col + _level) % 5 == 0) ? 2 : 1;
            _bricksRemaining++;
        }
    }
}

void BreakoutGame::resetBall() {
    _ballX = _paddleX + PADDLE_W / 2;
    _ballY = PADDLE_Y - BALL_R - 2;
    const float speed = 120.0f + min<uint8_t>(8, _level - 1) * 9.0f;
    _ballVx = (random(2) ? 0.62f : -0.62f) * speed;
    _ballVy = -0.78f * speed;
    _state = State::READY;
}

void BreakoutGame::launch() {
    if (_state == State::READY) {
        _state = State::PLAYING;
        _lastUpdateMs = millis();
        _dirty = true;
    }
}

void BreakoutGame::movePaddle(float delta) {
    _paddleX = constrain(_paddleX + delta,
                         FIELD_LEFT + 2, FIELD_RIGHT - PADDLE_W - 2);
    if (_state == State::READY) {
        _ballX = _paddleX + PADDLE_W / 2;
    }
    _dirty = true;
}

bool BreakoutGame::hitBrick(float previousX, float previousY) {
    constexpr float BRICK_X = 12;
    constexpr float BRICK_Y = 40;
    constexpr float BRICK_W = 27;
    constexpr float BRICK_H = 13;
    constexpr float GAP_X = 1;
    constexpr float GAP_Y = 2;
    if (_ballY < BRICK_Y - BALL_R ||
        _ballY > BRICK_Y + BRICK_ROWS * (BRICK_H + GAP_Y)) return false;

    const int8_t col = static_cast<int8_t>(
        (_ballX - BRICK_X) / (BRICK_W + GAP_X));
    const int8_t row = static_cast<int8_t>(
        (_ballY - BRICK_Y) / (BRICK_H + GAP_Y));
    if (row < 0 || row >= BRICK_ROWS || col < 0 || col >= BRICK_COLS ||
        !_bricks[row][col]) return false;

    const float left = BRICK_X + col * (BRICK_W + GAP_X);
    const float top = BRICK_Y + row * (BRICK_H + GAP_Y);
    const float right = left + BRICK_W;
    const float bottom = top + BRICK_H;
    if (_ballX + BALL_R < left || _ballX - BALL_R > right ||
        _ballY + BALL_R < top || _ballY - BALL_R > bottom) return false;

    if (--_bricks[row][col] == 0) {
        _bricksRemaining--;
        _score += 10 * _level;
    } else {
        _score += 3 * _level;
    }
    _highScore = max(_highScore, _score);
    const bool cameFromSide =
        previousX + BALL_R <= left || previousX - BALL_R >= right;
    if (cameFromSide) _ballVx = -_ballVx;
    else _ballVy = -_ballVy;
    return true;
}

void BreakoutGame::step(float dt) {
    const float previousX = _ballX;
    const float previousY = _ballY;
    _ballX += _ballVx * dt;
    _ballY += _ballVy * dt;

    if (_ballX - BALL_R <= FIELD_LEFT) {
        _ballX = FIELD_LEFT + BALL_R;
        _ballVx = fabsf(_ballVx);
    } else if (_ballX + BALL_R >= FIELD_RIGHT) {
        _ballX = FIELD_RIGHT - BALL_R;
        _ballVx = -fabsf(_ballVx);
    }
    if (_ballY - BALL_R <= FIELD_TOP) {
        _ballY = FIELD_TOP + BALL_R;
        _ballVy = fabsf(_ballVy);
    }

    if (_ballVy > 0 && _ballY + BALL_R >= PADDLE_Y &&
        previousY + BALL_R <= PADDLE_Y &&
        _ballX >= _paddleX - BALL_R &&
        _ballX <= _paddleX + PADDLE_W + BALL_R) {
        _ballY = PADDLE_Y - BALL_R;
        const float offset =
            ((_ballX - _paddleX) / PADDLE_W - 0.5f) * 2.0f;
        const float speed = min(220.0f,
            sqrtf(_ballVx * _ballVx + _ballVy * _ballVy) + 1.8f);
        _ballVx = speed * offset * 0.82f;
        _ballVy = -sqrtf(max(900.0f, speed * speed - _ballVx * _ballVx));
    }

    hitBrick(previousX, previousY);
    if (!_bricksRemaining) {
        advanceLevel();
        return;
    }
    if (_ballY - BALL_R > FIELD_BOTTOM) loseLife();
    _dirty = true;
}

void BreakoutGame::loseLife() {
    if (_lives > 0) _lives--;
    if (_lives == 0) {
        _state = State::GAME_OVER;
        _highScore = max(_highScore, _score);
        if (_preferences) _preferences->setBreakoutHighScore(_highScore);
    } else {
        resetBall();
    }
    _dirty = true;
}

void BreakoutGame::advanceLevel() {
    _score += 250 * _level;
    _highScore = max(_highScore, _score);
    _level++;
    buildLevel();
    resetBall();
}

void BreakoutGame::update() {
    if (!_active) return;
    const unsigned long now = millis();
    if (_state == State::PLAYING) {
        const unsigned long elapsed = min<unsigned long>(40, now - _lastUpdateMs);
        _lastUpdateMs = now;
        // 分成最多两个物理子步，减少高速小球穿砖。
        const uint8_t steps = elapsed > 20 ? 2 : 1;
        const float dt = elapsed / 1000.0f / steps;
        for (uint8_t i = 0; i < steps && _state == State::PLAYING; i++) step(dt);
    } else {
        _lastUpdateMs = now;
    }
    if (_dirty && now - _lastFrameMs >= 30) {
        _lastFrameMs = now;
        render();
    }
}

bool BreakoutGame::handleAction(const String& action, int value) {
    if (!_active) return false;
    if (action == "restart") {
        resetGame();
        return true;
    }
    if (action == "launch") {
        launch();
    } else if (action == "left") {
        movePaddle(value ? -abs(value) : -13);
    } else if (action == "right") {
        movePaddle(value ? abs(value) : 13);
    } else if (action == "position") {
        const long mapped = map(constrain(value, 0, 1000), 0, 1000,
            static_cast<long>(FIELD_LEFT + 2),
            static_cast<long>(FIELD_RIGHT - PADDLE_W - 2));
        _paddleX = max(FIELD_LEFT + 2,
                       min(FIELD_RIGHT - PADDLE_W - 2,
                           static_cast<float>(mapped)));
        if (_state == State::READY) _ballX = _paddleX + PADDLE_W / 2;
        _dirty = true;
    } else if (action == "pause") {
        if (_state == State::GAME_OVER || _state == State::READY) return false;
        _state = _state == State::PAUSED ? State::PLAYING : State::PAUSED;
        _lastUpdateMs = millis();
        _dirty = true;
    } else {
        return false;
    }
    render();
    return true;
}

void BreakoutGame::render() {
    _dirty = false;
    _canvas->beginFrame(0x0000);
    do {
    _canvas->fillRect(0, 0, 240, 25, COLOR_ORANGE);
    _canvas->setTextColor(COLOR_WHITE);
    _canvas->setTextSize(1);
    _canvas->setCursor(7, 8);
    _canvas->print("BREAKOUT");
    _canvas->setCursor(76, 8);
    _canvas->print("S:");
    _canvas->print(_score);
    _canvas->setCursor(151, 8);
    _canvas->print("L:");
    _canvas->print(_level);
    _canvas->setCursor(191, 8);
    _canvas->print("x");
    _canvas->print(_lives);

    _canvas->drawRect(FIELD_LEFT, FIELD_TOP,
                      FIELD_RIGHT - FIELD_LEFT + 1,
                      FIELD_BOTTOM - FIELD_TOP + 1, 0x528A);
    constexpr int16_t BRICK_X = 12;
    constexpr int16_t BRICK_Y = 40;
    constexpr int16_t BRICK_W = 27;
    constexpr int16_t BRICK_H = 13;
    for (uint8_t row = 0; row < BRICK_ROWS; row++) {
        for (uint8_t col = 0; col < BRICK_COLS; col++) {
            if (!_bricks[row][col]) continue;
            const int16_t x = BRICK_X + col * 28;
            const int16_t y = BRICK_Y + row * 15;
            _canvas->fillRoundRect(x, y, BRICK_W, BRICK_H, 2,
                                   ROW_COLORS[row]);
            _canvas->drawFastHLine(x + 3, y + 2, BRICK_W - 6, COLOR_WHITE);
            if (_bricks[row][col] > 1) {
                _canvas->drawRect(x + 3, y + 3,
                                  BRICK_W - 6, BRICK_H - 6, COLOR_WHITE);
            }
        }
    }
    _canvas->fillRoundRect(static_cast<int16_t>(_paddleX),
                           static_cast<int16_t>(PADDLE_Y),
                           static_cast<int16_t>(PADDLE_W),
                           static_cast<int16_t>(PADDLE_H), 3, COLOR_ORANGE);
    _canvas->drawFastHLine(static_cast<int16_t>(_paddleX) + 5,
                           static_cast<int16_t>(PADDLE_Y) + 1,
                           static_cast<int16_t>(PADDLE_W) - 10, COLOR_WHITE);
    _canvas->fillCircle(static_cast<int16_t>(_ballX),
                        static_cast<int16_t>(_ballY), BALL_R, COLOR_WHITE);
    _canvas->drawPixel(static_cast<int16_t>(_ballX) - 1,
                       static_cast<int16_t>(_ballY) - 1, 0xBDF7);

    if (_state != State::PLAYING) {
        _canvas->fillRect(35, 146, 170, 48, COLOR_DARKBG);
        _canvas->drawRect(35, 146, 170, 48, COLOR_ORANGE);
        _canvas->setTextColor(COLOR_WHITE);
        _canvas->setTextSize(2);
        if (_state == State::READY) {
            _canvas->setCursor(70, 163);
            _canvas->print("LAUNCH");
        } else if (_state == State::PAUSED) {
            _canvas->setCursor(77, 163);
            _canvas->print("PAUSED");
        } else {
            _canvas->setCursor(57, 163);
            _canvas->print("GAME OVER");
        }
    }
    } while (_canvas->nextStrip());
}

void BreakoutGame::redraw() {
    if (_active) render();
}

String BreakoutGame::getStateJson() const {
    String json = "{\"id\":\"breakout\",\"active\":";
    json += _active ? "true" : "false";
    json += ",\"state\":\"";
    json += breakoutStateName(static_cast<uint8_t>(_state));
    json += "\",\"score\":";
    json += _score;
    json += ",\"highScore\":";
    json += _highScore;
    json += ",\"lives\":";
    json += _lives;
    json += ",\"level\":";
    json += _level;
    json += ",\"bricks\":";
    json += _bricksRemaining;
    json += "}";
    return json;
}

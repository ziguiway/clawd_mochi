#include "snake_game.h"

#include "arcade_canvas.h"
#include "../config/cfg_display.h"
#include "../service/preference_service.h"

namespace {
const char* snakeStateName(uint8_t state) {
    switch (state) {
        case 0: return "ready";
        case 1: return "playing";
        case 2: return "paused";
        default: return "game_over";
    }
}
}  // namespace

SnakeGame::SnakeGame(ArcadeCanvas* canvas, PreferenceService* preferences)
    : _canvas(canvas), _preferences(preferences), _active(false),
      _state(State::READY), _direction(Direction::RIGHT),
      _queuedDirection(Direction::RIGHT), _snakeX{}, _snakeY{},
      _length(5), _foodX(15), _foodY(10), _score(0), _highScore(0),
      _lastStepMs(0), _dirty(true) {}

void SnakeGame::begin() {
    _active = true;
    if (_preferences) _highScore = _preferences->getSnakeHighScore();
    reset();
}

void SnakeGame::stop() {
    if (_preferences) _preferences->setSnakeHighScore(_highScore);
    _active = false;
}

void SnakeGame::reset() {
    _state = State::READY;
    _direction = Direction::RIGHT;
    _queuedDirection = Direction::RIGHT;
    _length = 5;
    for (uint8_t i = 0; i < _length; i++) {
        _snakeX[i] = 10 - i;
        _snakeY[i] = 10;
    }
    _score = 0;
    placeFood();
    _lastStepMs = millis();
    _dirty = true;
    render();
}

bool SnakeGame::occupies(uint8_t x, uint8_t y, uint16_t limit) const {
    for (uint16_t i = 0; i < limit; i++) {
        if (_snakeX[i] == x && _snakeY[i] == y) return true;
    }
    return false;
}

void SnakeGame::placeFood() {
    if (_length >= MAX_LENGTH) return;
    for (uint16_t attempt = 0; attempt < 1000; attempt++) {
        const uint8_t x = random(GRID);
        const uint8_t y = random(GRID);
        if (!occupies(x, y, _length)) {
            _foodX = x;
            _foodY = y;
            return;
        }
    }
}

bool SnakeGame::isOpposite(Direction a, Direction b) const {
    return (static_cast<uint8_t>(a) + 2) % 4 == static_cast<uint8_t>(b);
}

uint16_t SnakeGame::stepIntervalMs() const {
    return max<uint16_t>(70, 175 - min<uint16_t>(100, _score * 3));
}

void SnakeGame::step() {
    _direction = _queuedDirection;
    int8_t nx = _snakeX[0];
    int8_t ny = _snakeY[0];
    if (_direction == Direction::UP) ny--;
    else if (_direction == Direction::RIGHT) nx++;
    else if (_direction == Direction::DOWN) ny++;
    else nx--;

    if (nx < 0 || nx >= GRID || ny < 0 || ny >= GRID ||
        occupies(nx, ny, _length - 1)) {
        finish();
        return;
    }

    const bool ate = nx == _foodX && ny == _foodY;
    const uint16_t newLength = min<uint16_t>(
        MAX_LENGTH, _length + (ate ? 1 : 0));
    for (uint16_t i = newLength - 1; i > 0; i--) {
        _snakeX[i] = _snakeX[i - 1];
        _snakeY[i] = _snakeY[i - 1];
    }
    _snakeX[0] = nx;
    _snakeY[0] = ny;
    _length = newLength;
    if (ate) {
        _score++;
        _highScore = max(_highScore, _score);
        placeFood();
    }
    _dirty = true;
}

void SnakeGame::finish() {
    _state = State::GAME_OVER;
    _highScore = max(_highScore, _score);
    if (_preferences) _preferences->setSnakeHighScore(_highScore);
    _dirty = true;
}

void SnakeGame::update() {
    if (!_active || _state != State::PLAYING) return;
    const unsigned long now = millis();
    if (now - _lastStepMs >= stepIntervalMs()) {
        _lastStepMs = now;
        step();
    }
    if (_dirty) render();
}

bool SnakeGame::handleAction(const String& action, int) {
    if (!_active) return false;
    if (action == "restart") {
        reset();
        return true;
    }
    if (action == "pause") {
        if (_state == State::GAME_OVER || _state == State::READY) return false;
        _state = _state == State::PAUSED ? State::PLAYING : State::PAUSED;
        _lastStepMs = millis();
        _dirty = true;
    } else {
        Direction next;
        if (action == "up") next = Direction::UP;
        else if (action == "right") next = Direction::RIGHT;
        else if (action == "down") next = Direction::DOWN;
        else if (action == "left") next = Direction::LEFT;
        else return false;
        if (!isOpposite(next, _direction)) _queuedDirection = next;
        if (_state == State::READY) {
            _state = State::PLAYING;
            _lastStepMs = millis();
        }
    }
    render();
    return true;
}

void SnakeGame::render() {
    _dirty = false;
    _canvas->beginFrame(COLOR_DARKBG);
    do {
    _canvas->fillRect(0, 0, 240, 28, COLOR_ORANGE);
    _canvas->setTextColor(COLOR_WHITE);
    _canvas->setTextSize(2);
    _canvas->setCursor(8, 6);
    _canvas->print("SNAKE");
    _canvas->setTextSize(1);
    _canvas->setCursor(144, 10);
    _canvas->print("SCORE ");
    _canvas->print(_score);

    constexpr int16_t OX = 20;
    constexpr int16_t OY = 33;
    constexpr uint8_t CELL = 10;
    _canvas->fillRect(OX - 3, OY - 3, 206, 206, COLOR_ORANGE);
    _canvas->fillRect(OX, OY, 200, 200, 0x0861);
    for (uint8_t i = 1; i < GRID; i++) {
        _canvas->drawFastHLine(OX, OY + i * CELL, 200, 0x10A2);
        _canvas->drawFastVLine(OX + i * CELL, OY, 200, 0x10A2);
    }

    const int16_t fx = OX + _foodX * CELL;
    const int16_t fy = OY + _foodY * CELL;
    _canvas->fillCircle(fx + 5, fy + 5, 4, 0xF9E7);
    _canvas->drawPixel(fx + 6, fy + 1, COLOR_WHITE);

    for (int16_t i = _length - 1; i >= 0; i--) {
        const int16_t x = OX + _snakeX[i] * CELL;
        const int16_t y = OY + _snakeY[i] * CELL;
        const uint8_t green = 210 - min<int16_t>(120, i * 3);
        const uint16_t bodyColor =
            ((30 & 0xF8) << 8) | ((green & 0xFC) << 3) | (120 >> 3);
        uint16_t color = i == 0 ? COLOR_WHITE
            : bodyColor;
        _canvas->fillRoundRect(x + 1, y + 1, 9, 9, 2, color);
    }

    const int16_t hx = OX + _snakeX[0] * CELL;
    const int16_t hy = OY + _snakeY[0] * CELL;
    if (_direction == Direction::LEFT || _direction == Direction::RIGHT) {
        const int16_t ex = _direction == Direction::RIGHT ? hx + 7 : hx + 3;
        _canvas->drawPixel(ex, hy + 3, COLOR_BLACK);
        _canvas->drawPixel(ex, hy + 7, COLOR_BLACK);
    } else {
        const int16_t ey = _direction == Direction::DOWN ? hy + 7 : hy + 3;
        _canvas->drawPixel(hx + 3, ey, COLOR_BLACK);
        _canvas->drawPixel(hx + 7, ey, COLOR_BLACK);
    }

    if (_state != State::PLAYING) {
        _canvas->fillRect(32, 98, 176, 50, COLOR_DARKBG);
        _canvas->drawRect(32, 98, 176, 50, COLOR_ORANGE);
        _canvas->setTextColor(COLOR_WHITE);
        _canvas->setTextSize(2);
        if (_state == State::READY) {
            _canvas->setCursor(62, 115);
            _canvas->print("CHOOSE DIR");
        } else if (_state == State::PAUSED) {
            _canvas->setCursor(79, 115);
            _canvas->print("PAUSED");
        } else {
            _canvas->setCursor(58, 115);
            _canvas->print("GAME OVER");
        }
    }
    } while (_canvas->nextStrip());
}

void SnakeGame::redraw() {
    if (_active) render();
}

String SnakeGame::getStateJson() const {
    String json = "{\"id\":\"snake\",\"active\":";
    json += _active ? "true" : "false";
    json += ",\"state\":\"";
    json += snakeStateName(static_cast<uint8_t>(_state));
    json += "\",\"score\":";
    json += _score;
    json += ",\"highScore\":";
    json += _highScore;
    json += ",\"length\":";
    json += _length;
    json += ",\"speedMs\":";
    json += stepIntervalMs();
    json += "}";
    return json;
}

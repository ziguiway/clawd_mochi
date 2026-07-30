#include "game_2048.h"

#include "arcade_canvas.h"
#include "../config/cfg_display.h"
#include "../service/preference_service.h"

namespace {
const char* mergeStateName(uint8_t state) {
    if (state == 1) return "won";
    if (state == 2) return "game_over";
    return "playing";
}
}  // namespace

Game2048::Game2048(ArcadeCanvas* canvas, PreferenceService* preferences)
    : _canvas(canvas), _preferences(preferences), _active(false),
      _state(State::PLAYING), _board{}, _previous{}, _score(0),
      _previousScore(0), _bestScore(0), _canUndo(false) {}

void Game2048::begin() {
    _active = true;
    if (_preferences) _bestScore = _preferences->getGame2048BestScore();
    reset();
}

void Game2048::stop() {
    if (_preferences) _preferences->setGame2048BestScore(_bestScore);
    _active = false;
}

void Game2048::reset() {
    memset(_board, 0, sizeof(_board));
    memset(_previous, 0, sizeof(_previous));
    _score = 0;
    _previousScore = 0;
    _state = State::PLAYING;
    _canUndo = false;
    addTile();
    addTile();
    render();
}

void Game2048::addTile() {
    uint8_t empty[16];
    uint8_t count = 0;
    for (uint8_t y = 0; y < 4; y++) {
        for (uint8_t x = 0; x < 4; x++) {
            if (!_board[y][x]) empty[count++] = y * 4 + x;
        }
    }
    if (!count) return;
    const uint8_t index = empty[random(count)];
    _board[index / 4][index % 4] = random(10) == 0 ? 4 : 2;
}

bool Game2048::slideLine(uint16_t line[4]) {
    uint16_t original[4];
    memcpy(original, line, sizeof(original));
    uint16_t compact[4] = {};
    uint8_t count = 0;
    for (uint8_t i = 0; i < 4; i++) {
        if (line[i]) compact[count++] = line[i];
    }
    for (uint8_t i = 0; i + 1 < count; i++) {
        if (compact[i] == compact[i + 1]) {
            compact[i] *= 2;
            _score += compact[i];
            for (uint8_t j = i + 1; j + 1 < count; j++) {
                compact[j] = compact[j + 1];
            }
            compact[--count] = 0;
        }
    }
    memcpy(line, compact, sizeof(compact));
    return memcmp(original, line, sizeof(original)) != 0;
}

bool Game2048::move(Direction direction) {
    memcpy(_previous, _board, sizeof(_board));
    _previousScore = _score;
    bool changed = false;
    for (uint8_t lane = 0; lane < 4; lane++) {
        uint16_t line[4];
        for (uint8_t i = 0; i < 4; i++) {
            if (direction == Direction::LEFT) line[i] = _board[lane][i];
            else if (direction == Direction::RIGHT) line[i] = _board[lane][3 - i];
            else if (direction == Direction::UP) line[i] = _board[i][lane];
            else line[i] = _board[3 - i][lane];
        }
        changed |= slideLine(line);
        for (uint8_t i = 0; i < 4; i++) {
            if (direction == Direction::LEFT) _board[lane][i] = line[i];
            else if (direction == Direction::RIGHT) _board[lane][3 - i] = line[i];
            else if (direction == Direction::UP) _board[i][lane] = line[i];
            else _board[3 - i][lane] = line[i];
        }
    }
    if (!changed) {
        memcpy(_board, _previous, sizeof(_board));
        _score = _previousScore;
        return false;
    }
    _canUndo = true;
    addTile();
    _bestScore = max(_bestScore, _score);
    if (maxTile() >= 2048 && _state == State::PLAYING) _state = State::WON;
    if (!canMove()) {
        _state = State::GAME_OVER;
        if (_preferences) _preferences->setGame2048BestScore(_bestScore);
    }
    return true;
}

bool Game2048::canMove() const {
    for (uint8_t y = 0; y < 4; y++) {
        for (uint8_t x = 0; x < 4; x++) {
            if (!_board[y][x]) return true;
            if (x < 3 && _board[y][x] == _board[y][x + 1]) return true;
            if (y < 3 && _board[y][x] == _board[y + 1][x]) return true;
        }
    }
    return false;
}

uint16_t Game2048::maxTile() const {
    uint16_t result = 0;
    for (uint8_t y = 0; y < 4; y++) {
        for (uint8_t x = 0; x < 4; x++) result = max(result, _board[y][x]);
    }
    return result;
}

bool Game2048::handleAction(const String& action, int) {
    if (!_active) return false;
    if (action == "restart") {
        reset();
        return true;
    }
    if (action == "undo") {
        if (!_canUndo) return false;
        memcpy(_board, _previous, sizeof(_board));
        _score = _previousScore;
        _state = maxTile() >= 2048 ? State::WON : State::PLAYING;
        _canUndo = false;
        render();
        return true;
    }
    if (_state == State::GAME_OVER) return false;
    Direction direction;
    if (action == "up") direction = Direction::UP;
    else if (action == "right") direction = Direction::RIGHT;
    else if (action == "down") direction = Direction::DOWN;
    else if (action == "left") direction = Direction::LEFT;
    else return false;
    if (!move(direction)) return false;
    render();
    return true;
}

uint16_t Game2048::tileColor(uint16_t value) const {
    switch (value) {
        case 0: return 0xAD55;
        case 2: return 0xEF5C;
        case 4: return 0xEED9;
        case 8: return 0xF4CD;
        case 16: return 0xF3EA;
        case 32: return 0xF307;
        case 64: return 0xF205;
        case 128: return 0xED8D;
        case 256: return 0xED6A;
        case 512: return 0xED27;
        case 1024: return 0xECE4;
        case 2048: return 0xECA1;
        default: return COLOR_DARKBG;
    }
}

uint16_t Game2048::tileTextColor(uint16_t value) const {
    return value <= 4 ? 0x5AEB : COLOR_WHITE;
}

void Game2048::drawCenteredNumber(int16_t x, int16_t y, int16_t size,
                                  uint16_t value, uint16_t color) {
    if (!value) return;
    char text[7];
    snprintf(text, sizeof(text), "%u", value);
    uint8_t scale = value < 100 ? 3 : (value < 1000 ? 2 : 1);
    _canvas->setTextSize(scale);
    _canvas->setTextColor(color);
    int16_t bx, by;
    uint16_t bw, bh;
    _canvas->getTextBounds(text, 0, 0, &bx, &by, &bw, &bh);
    _canvas->setCursor(x + (size - bw) / 2,
                       y + (size - bh) / 2 + 1);
    _canvas->print(text);
}

void Game2048::render() {
    _canvas->clear(0xEED9);
    _canvas->fillRect(0, 0, 240, 35, 0x724A);
    _canvas->setTextColor(COLOR_WHITE);
    _canvas->setTextSize(2);
    _canvas->setCursor(8, 8);
    _canvas->print("2048");
    _canvas->setTextSize(1);
    _canvas->setCursor(78, 7);
    _canvas->print("SCORE");
    _canvas->setCursor(78, 19);
    _canvas->print(_score);
    _canvas->setCursor(158, 7);
    _canvas->print("BEST");
    _canvas->setCursor(158, 19);
    _canvas->print(_bestScore);

    constexpr int16_t GAP = 4;
    constexpr int16_t SIZE = 43;
    constexpr int16_t OX = 28;
    constexpr int16_t OY = 48;
    _canvas->fillRoundRect(22, 42, 196, 196, 7, 0x9C51);
    for (uint8_t y = 0; y < 4; y++) {
        for (uint8_t x = 0; x < 4; x++) {
            const int16_t px = OX + x * (SIZE + GAP);
            const int16_t py = OY + y * (SIZE + GAP);
            const uint16_t value = _board[y][x];
            _canvas->fillRoundRect(px, py, SIZE, SIZE, 5, tileColor(value));
            drawCenteredNumber(px, py, SIZE, value, tileTextColor(value));
        }
    }
    if (_state != State::PLAYING) {
        _canvas->fillRect(29, 102, 182, 50, 0x724A);
        _canvas->drawRect(29, 102, 182, 50, COLOR_WHITE);
        _canvas->setTextColor(COLOR_WHITE);
        _canvas->setTextSize(2);
        _canvas->setCursor(_state == State::WON ? 72 : 58, 119);
        _canvas->print(_state == State::WON ? "2048!" : "GAME OVER");
    }
    _canvas->flush();
}

void Game2048::redraw() {
    if (_active) render();
}

String Game2048::getStateJson() const {
    String json = "{\"id\":\"2048\",\"active\":";
    json += _active ? "true" : "false";
    json += ",\"state\":\"";
    json += mergeStateName(static_cast<uint8_t>(_state));
    json += "\",\"score\":";
    json += _score;
    json += ",\"bestScore\":";
    json += _bestScore;
    json += ",\"maxTile\":";
    json += maxTile();
    json += ",\"canUndo\":";
    json += _canUndo ? "true" : "false";
    json += "}";
    return json;
}

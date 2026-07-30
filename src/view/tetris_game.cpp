#include "tetris_game.h"

#include "arcade_canvas.h"
#include "../config/cfg_display.h"
#include "../service/preference_service.h"

namespace {
constexpr uint16_t BASE_SHAPES[7] = {
    0x00F0,  // I
    0x0066,  // O
    0x0072,  // T
    0x0071,  // J
    0x0074,  // L
    0x0036,  // S
    0x0063,  // Z
};

constexpr uint16_t PIECE_COLORS[8] = {
    0x0000,  // empty
    0x07FF,  // I cyan
    0xFFE0,  // O yellow
    0xA81F,  // T violet
    0x021F,  // J blue
    0xFD20,  // L orange
    0x07E0,  // S green
    0xF800,  // Z red
};

const char* stateName(uint8_t state) {
    if (state == 1) return "paused";
    if (state == 2) return "game_over";
    return "playing";
}
}  // namespace

TetrisGame::TetrisGame(ArcadeCanvas* canvas, PreferenceService* preferences)
    : _canvas(canvas), _preferences(preferences), _active(false),
      _state(State::PLAYING), _board{}, _piece(0), _nextPiece(0),
      _rotation(0), _pieceX(3), _pieceY(-1), _bag{}, _bagIndex(7),
      _score(0), _highScore(0), _lines(0), _level(1),
      _lastDropMs(0), _dirty(true) {}

void TetrisGame::begin() {
    _active = true;
    if (_preferences) _highScore = _preferences->getTetrisHighScore();
    reset();
}

void TetrisGame::stop() {
    if (_preferences) _preferences->setTetrisHighScore(_highScore);
    _active = false;
}

void TetrisGame::reset() {
    memset(_board, 0, sizeof(_board));
    _state = State::PLAYING;
    _score = 0;
    _lines = 0;
    _level = 1;
    _bagIndex = 7;
    _nextPiece = takePiece();
    spawnPiece();
    _lastDropMs = millis();
    _dirty = true;
    render();
}

void TetrisGame::refillBag() {
    for (uint8_t i = 0; i < 7; i++) _bag[i] = i;
    for (int8_t i = 6; i > 0; i--) {
        const uint8_t j = random(i + 1);
        const uint8_t temp = _bag[i];
        _bag[i] = _bag[j];
        _bag[j] = temp;
    }
    _bagIndex = 0;
}

uint8_t TetrisGame::takePiece() {
    if (_bagIndex >= 7) refillBag();
    return _bag[_bagIndex++];
}

bool TetrisGame::pieceCell(uint8_t piece, uint8_t rotation,
                           uint8_t x, uint8_t y) const {
    if (piece == 1) rotation = 0;  // O 不旋转
    uint8_t sx = x;
    uint8_t sy = y;
    switch (rotation & 3) {
        case 1: sx = y; sy = 3 - x; break;
        case 2: sx = 3 - x; sy = 3 - y; break;
        case 3: sx = 3 - y; sy = x; break;
        default: break;
    }
    return BASE_SHAPES[piece] & (1U << (sy * 4 + sx));
}

bool TetrisGame::collides(int8_t x, int8_t y, uint8_t rotation) const {
    for (uint8_t py = 0; py < 4; py++) {
        for (uint8_t px = 0; px < 4; px++) {
            if (!pieceCell(_piece, rotation, px, py)) continue;
            const int8_t bx = x + px;
            const int8_t by = y + py;
            if (bx < 0 || bx >= BOARD_W || by >= BOARD_H) return true;
            if (by >= 0 && _board[by][bx]) return true;
        }
    }
    return false;
}

bool TetrisGame::moveHorizontal(int8_t dx) {
    if (collides(_pieceX + dx, _pieceY, _rotation)) return false;
    _pieceX += dx;
    _dirty = true;
    return true;
}

bool TetrisGame::rotate() {
    const uint8_t next = (_rotation + 1) & 3;
    constexpr int8_t KICKS[] = {0, -1, 1, -2, 2};
    for (int8_t kick : KICKS) {
        if (!collides(_pieceX + kick, _pieceY, next)) {
            _pieceX += kick;
            _rotation = next;
            _dirty = true;
            return true;
        }
    }
    return false;
}

bool TetrisGame::softDrop(bool award) {
    if (!collides(_pieceX, _pieceY + 1, _rotation)) {
        _pieceY++;
        if (award) _score++;
        _dirty = true;
        return true;
    }
    lockPiece();
    return false;
}

void TetrisGame::hardDrop() {
    uint16_t distance = 0;
    while (!collides(_pieceX, _pieceY + 1, _rotation)) {
        _pieceY++;
        distance++;
    }
    _score += distance * 2;
    lockPiece();
}

void TetrisGame::lockPiece() {
    for (uint8_t py = 0; py < 4; py++) {
        for (uint8_t px = 0; px < 4; px++) {
            if (!pieceCell(_piece, _rotation, px, py)) continue;
            const int8_t bx = _pieceX + px;
            const int8_t by = _pieceY + py;
            if (by < 0) {
                _state = State::GAME_OVER;
                _highScore = max(_highScore, _score);
                if (_preferences) _preferences->setTetrisHighScore(_highScore);
                _dirty = true;
                return;
            }
            _board[by][bx] = _piece + 1;
        }
    }
    clearLines();
    spawnPiece();
    _dirty = true;
}

void TetrisGame::clearLines() {
    uint8_t cleared = 0;
    for (int8_t y = BOARD_H - 1; y >= 0; y--) {
        bool full = true;
        for (uint8_t x = 0; x < BOARD_W; x++) {
            if (!_board[y][x]) { full = false; break; }
        }
        if (!full) continue;
        for (int8_t yy = y; yy > 0; yy--) {
            memcpy(_board[yy], _board[yy - 1], BOARD_W);
        }
        memset(_board[0], 0, BOARD_W);
        cleared++;
        y++;
    }
    if (!cleared) return;
    constexpr uint16_t POINTS[] = {0, 100, 300, 500, 800};
    _score += static_cast<uint32_t>(POINTS[cleared]) * _level;
    _lines += cleared;
    _level = min<uint8_t>(20, 1 + _lines / 10);
    _highScore = max(_highScore, _score);
}

void TetrisGame::spawnPiece() {
    _piece = _nextPiece;
    _nextPiece = takePiece();
    _rotation = 0;
    _pieceX = 3;
    _pieceY = -1;
    if (collides(_pieceX, _pieceY, _rotation)) {
        _state = State::GAME_OVER;
        _highScore = max(_highScore, _score);
        if (_preferences) _preferences->setTetrisHighScore(_highScore);
    }
}

int8_t TetrisGame::ghostY() const {
    int8_t y = _pieceY;
    while (!collides(_pieceX, y + 1, _rotation)) y++;
    return y;
}

uint16_t TetrisGame::gravityMs() const {
    return max<uint16_t>(90, 760 - static_cast<uint16_t>(_level - 1) * 38);
}

void TetrisGame::update() {
    if (!_active || _state != State::PLAYING) return;
    const unsigned long now = millis();
    if (now - _lastDropMs >= gravityMs()) {
        _lastDropMs = now;
        softDrop(false);
    }
    if (_dirty) render();
}

bool TetrisGame::handleAction(const String& action, int) {
    if (!_active) return false;
    if (action == "restart") {
        reset();
        return true;
    }
    if (action == "pause") {
        if (_state == State::GAME_OVER) return false;
        _state = _state == State::PAUSED ? State::PLAYING : State::PAUSED;
        _lastDropMs = millis();
        _dirty = true;
    } else if (_state == State::PLAYING) {
        if (action == "left") moveHorizontal(-1);
        else if (action == "right") moveHorizontal(1);
        else if (action == "rotate") rotate();
        else if (action == "down") softDrop(true);
        else if (action == "drop") hardDrop();
        else return false;
    } else {
        return false;
    }
    if (_dirty) render();
    return true;
}

void TetrisGame::drawCell(int16_t x, int16_t y, uint8_t colorIndex,
                          bool ghost) {
    const uint16_t color = PIECE_COLORS[colorIndex];
    if (ghost) {
        _canvas->drawRect(x + 2, y + 2, 7, 7, 0x7BEF);
        return;
    }
    _canvas->fillRect(x + 1, y + 1, 9, 9, color);
    _canvas->drawFastHLine(x + 2, y + 2, 7, COLOR_WHITE);
    _canvas->drawFastVLine(x + 2, y + 2, 7, COLOR_WHITE);
    _canvas->drawFastHLine(x + 2, y + 9, 7, 0x2104);
    _canvas->drawFastVLine(x + 9, y + 2, 7, 0x2104);
}

void TetrisGame::render() {
    _dirty = false;
    _canvas->beginFrame(COLOR_DARKBG);
    do {
    _canvas->fillRect(0, 0, 240, 27, COLOR_ORANGE);
    _canvas->setTextColor(COLOR_WHITE);
    _canvas->setTextSize(2);
    _canvas->setCursor(8, 6);
    _canvas->print("TETRIS");

    constexpr int16_t BX = 8;
    constexpr int16_t BY = 31;
    constexpr uint8_t CELL = 10;
    _canvas->fillRect(BX - 2, BY - 2, BOARD_W * CELL + 4,
                      BOARD_H * CELL + 4, 0x39E7);
    _canvas->fillRect(BX, BY, BOARD_W * CELL, BOARD_H * CELL, COLOR_BLACK);
    for (uint8_t y = 0; y < BOARD_H; y++) {
        for (uint8_t x = 0; x < BOARD_W; x++) {
            if (_board[y][x]) drawCell(BX + x * CELL, BY + y * CELL,
                                       _board[y][x]);
            else {
                _canvas->drawPixel(BX + x * CELL, BY + y * CELL, 0x18C3);
            }
        }
    }

    if (_state != State::GAME_OVER) {
        const int8_t gy = ghostY();
        for (uint8_t py = 0; py < 4; py++) {
            for (uint8_t px = 0; px < 4; px++) {
                if (!pieceCell(_piece, _rotation, px, py)) continue;
                const int8_t yy = gy + py;
                if (yy >= 0) drawCell(BX + (_pieceX + px) * CELL,
                                      BY + yy * CELL, _piece + 1, true);
            }
        }
        for (uint8_t py = 0; py < 4; py++) {
            for (uint8_t px = 0; px < 4; px++) {
                if (!pieceCell(_piece, _rotation, px, py)) continue;
                const int8_t yy = _pieceY + py;
                if (yy >= 0) drawCell(BX + (_pieceX + px) * CELL,
                                      BY + yy * CELL, _piece + 1);
            }
        }
    }

    _canvas->setTextSize(1);
    _canvas->setTextColor(0xBDF7);
    _canvas->setCursor(124, 38);
    _canvas->print("NEXT");
    for (uint8_t py = 0; py < 4; py++) {
        for (uint8_t px = 0; px < 4; px++) {
            if (pieceCell(_nextPiece, 0, px, py)) {
                drawCell(124 + px * 10, 52 + py * 10, _nextPiece + 1);
            }
        }
    }
    _canvas->setCursor(124, 101);
    _canvas->print("SCORE");
    _canvas->setTextSize(2);
    _canvas->setTextColor(COLOR_WHITE);
    _canvas->setCursor(124, 114);
    _canvas->print(_score);
    _canvas->setTextSize(1);
    _canvas->setTextColor(0xBDF7);
    _canvas->setCursor(124, 148);
    _canvas->print("LINES");
    _canvas->setTextSize(2);
    _canvas->setTextColor(COLOR_WHITE);
    _canvas->setCursor(124, 161);
    _canvas->print(_lines);
    _canvas->setTextSize(1);
    _canvas->setTextColor(0xBDF7);
    _canvas->setCursor(124, 194);
    _canvas->print("LEVEL");
    _canvas->setTextSize(2);
    _canvas->setTextColor(COLOR_ORANGE);
    _canvas->setCursor(124, 207);
    _canvas->print(_level);

    if (_state != State::PLAYING) {
        _canvas->fillRect(25, 96, 190, 48, COLOR_DARKBG);
        _canvas->drawRect(25, 96, 190, 48, COLOR_ORANGE);
        _canvas->setTextColor(COLOR_WHITE);
        _canvas->setTextSize(2);
        _canvas->setCursor(_state == State::GAME_OVER ? 58 : 77, 112);
        _canvas->print(_state == State::GAME_OVER ? "GAME OVER" : "PAUSED");
    }
    } while (_canvas->nextStrip());
}

void TetrisGame::redraw() {
    if (_active) render();
}

String TetrisGame::getStateJson() const {
    String json = "{\"id\":\"tetris\",\"active\":";
    json += _active ? "true" : "false";
    json += ",\"state\":\"";
    json += stateName(static_cast<uint8_t>(_state));
    json += "\",\"score\":";
    json += _score;
    json += ",\"highScore\":";
    json += _highScore;
    json += ",\"lines\":";
    json += _lines;
    json += ",\"level\":";
    json += _level;
    json += "}";
    return json;
}

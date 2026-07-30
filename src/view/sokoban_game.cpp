#include "sokoban_game.h"

#include "sokoban_levels.h"
#include "../config/cfg_display.h"
#include "../hardware/tft_display.h"
#include "../service/preference_service.h"

namespace {
uint16_t glyph3x5(char c) {
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
        case 'A': return 0b010101111101101;
        case 'B': return 0b110101110101110;
        case 'C': return 0b111100100100111;
        case 'D': return 0b110101101101110;
        case 'E': return 0b111100110100111;
        case 'H': return 0b101101111101101;
        case 'L': return 0b100100100100111;
        case 'M': return 0b101111111101101;
        case 'O': return 0b111101101101111;
        case 'P': return 0b110101110100100;
        case 'R': return 0b110101110101101;
        case 'S': return 0b111100111001111;
        case 'T': return 0b111010010010010;
        case 'U': return 0b101101101101111;
        case 'V': return 0b101101101101010;
        case 'X': return 0b101101010101101;
        case '/': return 0b001001010100100;
        default: return 0;
    }
}
}  // namespace

SokobanGame::SokobanGame(TftDisplay* tft, PreferenceService* preferences,
                         uint8_t* frameBuffer)
    : _tft(tft)
    , _preferences(preferences)
    , _active(false)
    , _state(State::PLAYING)
    , _levelIndex(0)
    , _width(0)
    , _height(0)
    , _playerX(0)
    , _playerY(0)
    , _boxCount(0)
    , _moves(0)
    , _pushes(0)
    , _completedMask(0)
    , _tiles{}
    , _boxes{}
    , _history{}
    , _historyCount(0)
    , _frameBuffer(frameBuffer)
{
}

void SokobanGame::begin() {
    _active = true;
    if (_preferences) {
        _levelIndex = min<uint8_t>(
            _preferences->getSokobanLevel(), SokobanLevels::COUNT - 1);
        _completedMask = _preferences->getSokobanCompletedMask();
    }
    loadLevel(_levelIndex);
    render();
}

void SokobanGame::stop() {
    _active = false;
}

uint16_t SokobanGame::cellIndex(uint8_t x, uint8_t y) const {
    return static_cast<uint16_t>(y) * MAX_WIDTH + x;
}

bool SokobanGame::loadLevel(uint8_t index) {
    if (index >= SokobanLevels::COUNT) return false;

    memset(_tiles, VOID_TILE, sizeof(_tiles));
    memset(_boxes, 0, sizeof(_boxes));
    _levelIndex = index;
    _width = 0;
    _height = 0;
    _boxCount = 0;
    _moves = 0;
    _pushes = 0;
    _historyCount = 0;
    _state = State::PLAYING;

    const char* data = reinterpret_cast<const char*>(
        pgm_read_ptr(&SokobanLevels::LEVELS[index]));
    uint8_t x = 0;
    uint8_t y = 0;
    for (uint16_t pos = 0; y < MAX_HEIGHT; pos++) {
        const char c = pgm_read_byte(data + pos);
        if (c == '\0') break;
        if (c == '\n') {
            _width = max(_width, x);
            x = 0;
            y++;
            continue;
        }
        if (x >= MAX_WIDTH) continue;

        Tile tile = FLOOR;
        if (c == '#') tile = WALL;
        else if (c == '.' || c == '*' || c == '+') tile = GOAL;
        _tiles[cellIndex(x, y)] = tile;

        if (c == '$' || c == '*') {
            _boxes[cellIndex(x, y)] = true;
            _boxCount++;
        } else if (c == '@' || c == '+') {
            _playerX = x;
            _playerY = y;
        }
        x++;
    }
    if (x > 0) {
        _width = max(_width, x);
        y++;
    }
    _height = y;
    markExteriorVoid();

    if (_preferences) _preferences->setSokobanLevel(_levelIndex);
    return _width > 0 && _height > 0 && _boxCount > 0;
}

void SokobanGame::markExteriorVoid() {
    uint16_t queue[MAX_CELLS];
    uint16_t head = 0;
    uint16_t tail = 0;

    auto enqueue = [&](uint8_t x, uint8_t y) {
        const uint16_t index = cellIndex(x, y);
        if (_tiles[index] != FLOOR || _boxes[index] ||
            (x == _playerX && y == _playerY)) {
            return;
        }
        _tiles[index] = VOID_TILE;
        queue[tail++] = index;
    };

    for (uint8_t x = 0; x < _width; x++) {
        enqueue(x, 0);
        if (_height > 1) enqueue(x, _height - 1);
    }
    for (uint8_t y = 0; y < _height; y++) {
        enqueue(0, y);
        if (_width > 1) enqueue(_width - 1, y);
    }

    constexpr int8_t DX[] = {1, -1, 0, 0};
    constexpr int8_t DY[] = {0, 0, 1, -1};
    while (head < tail) {
        const uint16_t index = queue[head++];
        const uint8_t x = index % MAX_WIDTH;
        const uint8_t y = index / MAX_WIDTH;
        for (uint8_t i = 0; i < 4; i++) {
            const int16_t nx = x + DX[i];
            const int16_t ny = y + DY[i];
            if (nx >= 0 && ny >= 0 && nx < _width && ny < _height) {
                enqueue(nx, ny);
            }
        }
    }
}

bool SokobanGame::isWalkable(int16_t x, int16_t y) const {
    if (x < 0 || y < 0 || x >= _width || y >= _height) return false;
    const Tile tile = _tiles[cellIndex(x, y)];
    return tile == FLOOR || tile == GOAL;
}

bool SokobanGame::hasBox(int16_t x, int16_t y) const {
    return x >= 0 && y >= 0 && x < _width && y < _height &&
           _boxes[cellIndex(x, y)];
}

bool SokobanGame::move(int8_t dx, int8_t dy) {
    if (!_active || (abs(dx) + abs(dy)) != 1) return false;

    const int16_t nx = _playerX + dx;
    const int16_t ny = _playerY + dy;
    if (!isWalkable(nx, ny)) return false;

    HistoryEntry history = {_playerX, _playerY, 0xFF, 0xFF};
    if (hasBox(nx, ny)) {
        const int16_t bx = nx + dx;
        const int16_t by = ny + dy;
        if (!isWalkable(bx, by) || hasBox(bx, by)) return false;
        history.boxX = nx;
        history.boxY = ny;
        _boxes[cellIndex(nx, ny)] = false;
        _boxes[cellIndex(bx, by)] = true;
        _pushes++;
    }

    if (_historyCount == HISTORY_SIZE) {
        memmove(_history, _history + 1,
                sizeof(HistoryEntry) * (HISTORY_SIZE - 1));
        _historyCount--;
    }
    _history[_historyCount++] = history;
    _playerX = nx;
    _playerY = ny;
    _moves++;
    _state = State::PLAYING;

    if (isComplete()) completeLevel();
    render();
    return true;
}

bool SokobanGame::undo() {
    if (!_active || _historyCount == 0) return false;
    const HistoryEntry history = _history[--_historyCount];

    if (history.boxX != 0xFF) {
        const int8_t dx = static_cast<int8_t>(_playerX) -
                          static_cast<int8_t>(history.playerX);
        const int8_t dy = static_cast<int8_t>(_playerY) -
                          static_cast<int8_t>(history.playerY);
        _boxes[cellIndex(history.boxX + dx, history.boxY + dy)] = false;
        _boxes[cellIndex(history.boxX, history.boxY)] = true;
        if (_pushes > 0) _pushes--;
    }
    _playerX = history.playerX;
    _playerY = history.playerY;
    if (_moves > 0) _moves--;
    _state = State::PLAYING;
    render();
    return true;
}

void SokobanGame::restart() {
    if (!_active) return;
    loadLevel(_levelIndex);
    render();
}

bool SokobanGame::selectLevel(uint8_t index) {
    if (!_active || !loadLevel(index)) return false;
    render();
    return true;
}

void SokobanGame::redraw() {
    if (_active) render();
}

bool SokobanGame::handleAction(const String& action, int value) {
    if (action == "up") return move(0, -1);
    if (action == "down") return move(0, 1);
    if (action == "left") return move(-1, 0);
    if (action == "right") return move(1, 0);
    if (action == "undo") return undo();
    if (action == "restart") {
        restart();
        return true;
    }
    if (action == "level" && value > 0) {
        return selectLevel(static_cast<uint8_t>(value - 1));
    }
    return false;
}

uint8_t SokobanGame::boxesOnGoals() const {
    uint8_t count = 0;
    for (uint8_t y = 0; y < _height; y++) {
        for (uint8_t x = 0; x < _width; x++) {
            const uint16_t index = cellIndex(x, y);
            if (_boxes[index] && _tiles[index] == GOAL) count++;
        }
    }
    return count;
}

bool SokobanGame::isComplete() const {
    return _boxCount > 0 && boxesOnGoals() == _boxCount;
}

void SokobanGame::completeLevel() {
    _state = State::COMPLETE;
    _completedMask |= (1UL << _levelIndex);
    if (_preferences) {
        _preferences->setSokobanCompletedMask(_completedMask);
    }
}

void SokobanGame::clearFrame() {
    memset(_frameBuffer, 0, FRAME_BYTES);
}

void SokobanGame::setPixel(int16_t x, int16_t y, bool on) {
    if (x < 0 || x >= 240 || y < 0 || y >= 240) return;
    uint8_t& value = _frameBuffer[y * FRAME_STRIDE + x / 8];
    const uint8_t mask = static_cast<uint8_t>(0x80U >> (x & 7));
    if (on) value |= mask;
    else value &= ~mask;
}

void SokobanGame::fillBufferRect(int16_t x, int16_t y, int16_t w, int16_t h,
                                 bool on) {
    for (int16_t yy = max<int16_t>(0, y); yy < min<int16_t>(240, y + h); yy++) {
        for (int16_t xx = max<int16_t>(0, x); xx < min<int16_t>(240, x + w); xx++) {
            setPixel(xx, yy, on);
        }
    }
}

void SokobanGame::drawBufferRect(int16_t x, int16_t y, int16_t w, int16_t h) {
    fillBufferRect(x, y, w, 1);
    fillBufferRect(x, y + h - 1, w, 1);
    fillBufferRect(x, y, 1, h);
    fillBufferRect(x + w - 1, y, 1, h);
}

void SokobanGame::drawTinyText(int16_t x, int16_t y, const char* text,
                               uint8_t scale) {
    for (const char* p = text; *p; p++, x += 4 * scale) {
        const uint16_t glyph = glyph3x5(*p);
        for (uint8_t row = 0; row < 5; row++) {
            for (uint8_t col = 0; col < 3; col++) {
                if (!(glyph & (1U << (14 - row * 3 - col)))) continue;
                fillBufferRect(x + col * scale, y + row * scale,
                               scale, scale);
            }
        }
    }
}

void SokobanGame::drawWall(int16_t x, int16_t y, uint8_t size) {
    fillBufferRect(x, y, size, size);
    if (size >= 8) {
        fillBufferRect(x + 2, y + 2, size - 4, size - 4, false);
        fillBufferRect(x + size / 2, y + 1, 1, size - 2);
        fillBufferRect(x + 1, y + size / 2, size - 2, 1);
    }
}

void SokobanGame::drawGoal(int16_t x, int16_t y, uint8_t size) {
    const int16_t center = size / 2;
    fillBufferRect(x + center, y + 2, 1, size - 4);
    fillBufferRect(x + 2, y + center, size - 4, 1);
    if (size >= 12) drawBufferRect(x + 3, y + 3, size - 6, size - 6);
}

void SokobanGame::drawBox(int16_t x, int16_t y, uint8_t size, bool onGoal) {
    if (onGoal) {
        fillBufferRect(x + 1, y + 1, size - 2, size - 2);
        fillBufferRect(x + 3, y + 3, size - 6, size - 6, false);
        fillBufferRect(x + size / 2, y + size / 2, 1, 1);
        return;
    }
    drawBufferRect(x + 1, y + 1, size - 2, size - 2);
    for (uint8_t i = 3; i + 3 < size; i++) {
        setPixel(x + i, y + i);
        setPixel(x + size - 1 - i, y + i);
    }
}

void SokobanGame::drawPlayer(int16_t x, int16_t y, uint8_t size) {
    const int16_t cx = x + size / 2;
    const int16_t cy = y + size / 2;
    const int16_t radius = max<int16_t>(2, size / 2 - 1);
    for (int16_t yy = -radius; yy <= radius; yy++) {
        for (int16_t xx = -radius; xx <= radius; xx++) {
            if (xx * xx + yy * yy <= radius * radius) {
                setPixel(cx + xx, cy + yy);
            }
        }
    }
    if (size >= 9) {
        setPixel(cx - 2, cy - 1, false);
        setPixel(cx + 2, cy - 1, false);
        fillBufferRect(cx - 2, cy + 2, 5, 1, false);
    }
}

void SokobanGame::render() {
    clearFrame();

    char header[24];
    snprintf(header, sizeof(header), "L%02u M%03u P%03u",
             _levelIndex + 1, _moves % 1000, _pushes % 1000);
    drawTinyText(8, 9, header, 2);

    const uint8_t tileW = max<uint8_t>(6, min<uint8_t>(18, 232 / _width));
    const uint8_t tileH = max<uint8_t>(6, min<uint8_t>(18, 186 / _height));
    const uint8_t tile = min(tileW, tileH);
    const int16_t mapW = _width * tile;
    const int16_t mapH = _height * tile;
    const int16_t originX = (240 - mapW) / 2;
    const int16_t originY = 36 + (186 - mapH) / 2;

    for (uint8_t y = 0; y < _height; y++) {
        for (uint8_t x = 0; x < _width; x++) {
            const uint16_t index = cellIndex(x, y);
            const int16_t px = originX + x * tile;
            const int16_t py = originY + y * tile;
            if (_tiles[index] == WALL) drawWall(px, py, tile);
            else if (_tiles[index] == GOAL) drawGoal(px, py, tile);
            if (_boxes[index]) {
                drawBox(px, py, tile, _tiles[index] == GOAL);
            }
        }
    }
    drawPlayer(originX + _playerX * tile, originY + _playerY * tile, tile);

    if (_state == State::COMPLETE) {
        fillBufferRect(62, 220, 116, 18, false);
        drawBufferRect(62, 220, 116, 18);
        drawTinyText(78, 224, "LEVEL CLEAR", 2);
    } else {
        char progress[12];
        snprintf(progress, sizeof(progress), "%u/%u",
                 boxesOnGoals(), _boxCount);
        drawTinyText(104, 228, progress, 1);
    }
    flushFrame();
}

void SokobanGame::flushFrame() {
    uint16_t scanline[240];
    Adafruit_ST7789& screen = _tft->getTft();
    screen.startWrite();
    screen.setAddrWindow(0, 0, 240, 240);
    for (uint16_t y = 0; y < 240; y++) {
        const uint8_t* source = _frameBuffer + y * FRAME_STRIDE;
        for (uint16_t x = 0; x < 240; x++) {
            scanline[x] = (source[x / 8] & (0x80U >> (x & 7)))
                ? COLOR_WHITE
                : COLOR_ORANGE;
        }
        screen.writePixels(scanline, 240, true, false);
    }
    screen.endWrite();
}

String SokobanGame::getStateJson() const {
    uint8_t completedCount = 0;
    for (uint8_t i = 0; i < SokobanLevels::COUNT; i++) {
        if (_completedMask & (1UL << i)) completedCount++;
    }
    String json = "{\"active\":";
    json += _active ? "true" : "false";
    json += ",\"state\":\"";
    json += _state == State::COMPLETE ? "complete" : "playing";
    json += "\",\"level\":";
    json += _levelIndex + 1;
    json += ",\"levelCount\":";
    json += SokobanLevels::COUNT;
    json += ",\"moves\":";
    json += _moves;
    json += ",\"pushes\":";
    json += _pushes;
    json += ",\"boxesOnGoals\":";
    json += boxesOnGoals();
    json += ",\"boxes\":";
    json += _boxCount;
    json += ",\"completedLevels\":";
    json += completedCount;
    json += ",\"canUndo\":";
    json += _historyCount > 0 ? "true" : "false";
    json += "}";
    return json;
}

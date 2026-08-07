#include "keyboard_pet_service.h"
#include "wifi_config_service.h"
#include "../utils/logger.h"

KeyboardPetService::KeyboardPetService()
    : _paw(Paw::NONE), _enabled(true), _initialized(false),
      _leftUntil(0), _rightUntil(0) {}

void KeyboardPetService::init() {}

void KeyboardPetService::process(const char* packet) {
    // 协议: KP:down:left|right|both 或 KP:up:left|right|both
    if (strncmp(packet, "KP:", 3) != 0) return;
    const bool down = strncmp(packet + 3, "down:", 5) == 0;
    const char* side = packet + (down ? 8 : 6);
    const unsigned long until = millis() + CFG_KEYBOARD_PET_HOLD_MS;
    if (strstr(side, "left")) _leftUntil = down ? until : 0;
    if (strstr(side, "right")) _rightUntil = down ? until : 0;
    LOG_DEBUG("KeyboardPet", "按键动作: %s", packet + 3);
}

void KeyboardPetService::update() {
    if (!_enabled) return;
    if (!_initialized) {
        auto* wifi = WifiConfigService::current();
        if (!wifi || !wifi->isConnected()) return;
        if (_udp.begin(CFG_KEYBOARD_PET_UDP_PORT)) {
            _initialized = true;
            LOG_INFO("KeyboardPet", "UDP 监听端口: %d", CFG_KEYBOARD_PET_UDP_PORT);
        }
    }
    int packetSize = _udp.parsePacket();
    if (packetSize > 0) {
        char buf[CFG_KEYBOARD_PET_RX_BUF_SIZE];
        int len = _udp.read(buf, sizeof(buf) - 1);
        if (len > 0) { buf[len] = '\0'; process(buf); }
    }
    const unsigned long now = millis();
    _paw = (now < _leftUntil && now < _rightUntil) ? Paw::BOTH :
           (now < _leftUntil ? Paw::LEFT : (now < _rightUntil ? Paw::RIGHT : Paw::NONE));
}

#pragma once

// 键盘宠物独立于 Claude Code UDP 通道，避免互相抢包。
#define CFG_KEYBOARD_PET_UDP_PORT 4212
#define CFG_KEYBOARD_PET_RX_BUF_SIZE 32
#define CFG_KEYBOARD_PET_HOLD_MS 90

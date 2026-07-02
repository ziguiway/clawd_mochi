#pragma once

// ============================================================
// 显示配置
// ============================================================

// ST7789 引脚定义 (ESP32-C3 Super Mini)
#define CFG_DISPLAY_PIN_SDA         10  // MOSI (GPIO 10)
#define CFG_DISPLAY_PIN_SCL         8   // SCK  (GPIO 8)
#define CFG_DISPLAY_PIN_CS          4
#define CFG_DISPLAY_PIN_DC          1
#define CFG_DISPLAY_PIN_RST         2
#define CFG_DISPLAY_PIN_BL          3   // 背光

// SPI 频率
#define CFG_DISPLAY_SPI_FREQ        40000000  // 40MHz

// 屏幕尺寸
#define CFG_DISPLAY_WIDTH           240
#define CFG_DISPLAY_HEIGHT          240

// 颜色定义 (RGB565)
#define COLOR_ORANGE                0xF960  // 橙色（主题色）
#define COLOR_DARKBG                0x0821  // 深蓝黑（背景）
#define COLOR_WHITE                 0xFFFF  // 白色
#define COLOR_GREEN                 0x07E0  // 绿色（成功）
#define COLOR_RED                   0xF800  // 红色（错误）
#define COLOR_YELLOW                0xFFE0  // 黄色（警告）
#define COLOR_GRAY                  0x8410  // 灰色（次要信息）
#define COLOR_BLUE                  0x001F  // 蓝色（思考）
#define COLOR_BLACK                 0x0000  // 黑色
#define COLOR_MUTED                 0x528A  // 暗灰（次要文字）
#define COLOR_TERM_GREEN            0x2E8B  // 终端绿色

// 交互式视图模式
#define VIEW_EYES_NORMAL  0
#define VIEW_EYES_SQUISH  1
#define VIEW_CODE         2
#define VIEW_DRAW         3
#define VIEW_THINKING   4
#define VIEW_WORKING    5

// 终端配置
#define TERM_COLS      15
#define TERM_ROWS       8
#define TERM_CHAR_W    12
#define TERM_CHAR_H    20
#define TERM_PAD_X      8
#define TERM_PAD_Y     18
#define PREFIX_PX      54

// 字体大小
#define FONT_SMALL                  1
#define FONT_MEDIUM                 2
#define FONT_LARGE                  3

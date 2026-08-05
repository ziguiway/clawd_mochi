#pragma once

// 桌面无线投屏(Desktop Stream)配置
// PC 上位机通过 TCP 推送 JPEG 帧: ESPF(4B 魔数) + length(4B LE) + JPEG payload

#define CFG_STREAM_TCP_PORT            3333

// JPEG 帧缓冲区(懒加载,仅 VIEW_DESKTOP_STREAM 激活时分配)
// 240x240 JPEG 质量 50 单帧通常 < 10KB,32KB 余量充足
#define CFG_STREAM_JPEG_BUFFER_BYTES   (32U * 1024U)

// 单帧读取超时(无进度即判定客户端异常)
#define CFG_STREAM_FRAME_READ_TIMEOUT_MS  3500UL

// 无客户端连接时的等待页超时(毫秒),仅用于日志节流
#define CFG_STREAM_IDLE_LOG_INTERVAL_MS   5000UL

// 连续解码/协议失败达到此次数后断开客户端
#define CFG_STREAM_MAX_FRAME_ERRORS       3

// 帧头魔数 "ESPF"
#define CFG_STREAM_MAGIC_0  'E'
#define CFG_STREAM_MAGIC_1  'S'
#define CFG_STREAM_MAGIC_2  'P'
#define CFG_STREAM_MAGIC_3  'F'

#pragma once

// ============================================================
// Claude Code 服务配置
// ============================================================

// UDP 端口
#define CFG_CLAUDE_CODE_UDP_PORT          4210

// SLEEPING 持续时间 (ms)
#define CFG_CLAUDE_CODE_SLEEP_DURATION_MS 10000

// 活跃状态长时间没有新事件时回到 IDLE,避免丢包后永久冻结
#define CFG_CLAUDE_CODE_ACTIVE_TIMEOUT_MS  300000

// 缓冲区大小
#define CFG_CLAUDE_CODE_RX_BUF_SIZE       128

// 字段最大长度
#define CFG_CLAUDE_CODE_HOOK_MAX_LEN      32
#define CFG_CLAUDE_CODE_TOOL_MAX_LEN      32
#define CFG_CLAUDE_CODE_DETAIL_MAX_LEN    64
#define CFG_CLAUDE_CODE_MODEL_MAX_LEN     20

// UDP 发现端口
#define CFG_CLAUDE_CODE_DISCOVERY_PORT    4211

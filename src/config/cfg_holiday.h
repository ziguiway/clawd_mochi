#pragma once

// 中国法定节假日查询。日期由固件按 YYYY-MM-DD 追加到接口末尾。
#define CFG_HOLIDAY_API_BASE        "https://holiday.ailcc.com/api/holiday/info/"

// HTTPS 请求和失败退避参数。
#define CFG_HOLIDAY_CONNECT_TIMEOUT_MS  12000
#define CFG_HOLIDAY_REQUEST_TIMEOUT_MS  10000
#define CFG_HOLIDAY_FIRST_RETRY_MS      (30UL * 1000UL)
#define CFG_HOLIDAY_MAX_RETRY_MS        (30UL * 60UL * 1000UL)

// 只缓存当天解析后的紧凑结果，不保存整年响应。
#define CFG_HOLIDAY_CACHE_PATH      "/holiday.json"

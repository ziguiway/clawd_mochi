#pragma once

#include <Arduino.h>

#include "time_service.h"
#include "wifi_config_service.h"

class HolidayService {
public:
    HolidayService(WifiConfigService* wifiService, TimeService* timeService);

    void init();
    void update();

    bool isResolvedForToday() const { return _resolvedForToday; }
    bool isHolidayToday() const {
        return _resolvedForToday && _holidayToday;
    }
    const char* getHolidayName() const {
        return isHolidayToday() ? _holidayName : "";
    }
    uint32_t getVersion() const { return _version; }

private:
    static constexpr size_t DATE_SIZE = 11;
    static constexpr size_t HOLIDAY_NAME_SIZE = 24;

    WifiConfigService* _wifiService;
    TimeService* _timeService;
    volatile bool _loading;
    volatile bool _resolvedForToday;
    volatile bool _holidayToday;
    char _activeDate[DATE_SIZE];
    char _holidayName[HOLIDAY_NAME_SIZE];
    char _cachedDate[DATE_SIZE];
    char _cachedHolidayName[HOLIDAY_NAME_SIZE];
    bool _cacheAvailable;
    bool _cachedHoliday;
    unsigned long _lastAttemptMs;
    volatile uint8_t _failureCount;
    volatile uint32_t _version;
    TaskHandle_t _refreshTask;

    static void refreshTaskEntry(void* parameter);
    void runRefresh();
    void syncActiveDate();
    bool fetchDate(const char* date, bool& holiday, char* name, size_t nameSize);
    bool loadCache();
    void saveCache(const char* date, bool holiday, const char* name);
    unsigned long retryDelayMs() const;
    static void formatToday(TimeService* timeService, char* output, size_t size);
    static void translateHolidayName(const char* apiName, char* output, size_t size);
};

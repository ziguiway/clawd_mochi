#pragma once

#include <Arduino.h>

class TimeService {
public:
    TimeService();
    void init();
    void update();

    bool isSynced();
    String getTime();
    String getDate();
    String getDateTime();
    String getTimestamp();

    unsigned long getEpoch();
    int getHour();
    int getMinute();
    int getSecond();
    int getYear();
    int getMonth();
    int getDay();

    void syncNow();

    static bool timestampCallback(char* buf, size_t bufSize);

private:
    bool _synced;
    unsigned long _lastSyncTime;
    unsigned long _syncInterval;

    void configTime();
    bool waitForSync(unsigned long timeoutMs = 10000);
};

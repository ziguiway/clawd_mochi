#include "time_service.h"
#include "operation_mode_service.h"
#include <time.h>

TimeService::TimeService()
    : _synced(false)
    , _lastSyncTime(0)
    , _syncInterval(3600000)
{
}

void TimeService::init() {
    configTime();
}

void TimeService::update() {
    if (!_synced) return;
    unsigned long now = millis();
    if (now - _lastSyncTime > _syncInterval) {
        syncNow();
    }
}

void TimeService::configTime() {
    ::configTime(8 * 3600, 0, "ntp.aliyun.com", "ntp1.aliyun.com", "ntp2.aliyun.com");
}

bool TimeService::waitForSync(unsigned long timeoutMs) {
    unsigned long start = millis();
    while (millis() - start < timeoutMs) {
        time_t now = time(nullptr);
        if (now > 1000000000) {
            _synced = true;
            _lastSyncTime = millis();
            return true;
        }
        delay(100);
    }
    return false;
}

void TimeService::syncNow() {
    configTime();
    _synced = waitForSync(10000);
}

bool TimeService::isSynced() { return _synced; }

String TimeService::getTime() {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char buf[10];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
    return String(buf);
}

String TimeService::getDate() {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char buf[12];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
    return String(buf);
}

String TimeService::getDateTime() { return getDate() + " " + getTime(); }

String TimeService::getTimestamp() {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char buf[25];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
    return String(buf);
}

unsigned long TimeService::getEpoch() { return time(nullptr); }
int TimeService::getHour() { time_t t = time(nullptr); return localtime(&t)->tm_hour; }
int TimeService::getMinute() { time_t t = time(nullptr); return localtime(&t)->tm_min; }
int TimeService::getSecond() { time_t t = time(nullptr); return localtime(&t)->tm_sec; }
int TimeService::getYear() { time_t t = time(nullptr); return localtime(&t)->tm_year + 1900; }
int TimeService::getMonth() { time_t t = time(nullptr); return localtime(&t)->tm_mon + 1; }
int TimeService::getDay() { time_t t = time(nullptr); return localtime(&t)->tm_mday; }

bool TimeService::timestampCallback(char* buf, size_t bufSize) {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    snprintf(buf, bufSize, "%04d-%02d-%02d %02d:%02d:%02d",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
    return true;
}

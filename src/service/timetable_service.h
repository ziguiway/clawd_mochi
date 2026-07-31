#pragma once

#include <Arduino.h>

class TimeService;

enum class TimetableState : uint8_t {
    NOT_CONFIGURED,
    NO_CLASS_TODAY,
    NEXT_CLASS,
    IN_CLASS,
    ALL_DONE
};

struct TimetableCourseSnapshot {
    char name[44];
    char shortName[24];
    char room[22];
    char teacher[24];
    char start[6];
    char end[6];
    uint8_t weekday;
};

struct TimetableSnapshot {
    TimetableState state;
    uint8_t academicWeek;
    uint8_t weekday;
    uint8_t todayTotal;
    uint8_t todayCompleted;
    uint8_t remainingToday;
    uint16_t minutesRemaining;
    TimetableCourseSnapshot course;
    TimetableCourseSnapshot nextCourse;
    bool hasNextCourse;
};

// 课表仅在查询或保存时解析 LittleFS JSON，不在常驻服务中保留整个学期，
// 避免用固定大数组永久占用 ESP32-C3 堆内存。
class TimetableService {
public:
    void init();
    bool saveJson(const String& payload, String& error);
    bool loadJson(String& output) const;
    bool getSnapshot(TimeService* timeService, TimetableSnapshot& output) const;
    bool isConfigured() const;

private:
    static constexpr const char* FILE_PATH = "/timetable.json";

    bool validateDocument(const String& payload, String& error) const;
};

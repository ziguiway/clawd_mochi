#include "timetable_service.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <time.h>

#include "time_service.h"
#include "../utils/logger.h"

namespace {
int parseMinutes(const char* value) {
    if (!value || strlen(value) != 5 || value[2] != ':') return -1;
    const int hour = (value[0] - '0') * 10 + value[1] - '0';
    const int minute = (value[3] - '0') * 10 + value[4] - '0';
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) return -1;
    return hour * 60 + minute;
}

time_t parseLocalDate(const char* value) {
    if (!value || strlen(value) != 10) return 0;
    struct tm date = {};
    date.tm_year = atoi(value) - 1900;
    date.tm_mon = atoi(value + 5) - 1;
    date.tm_mday = atoi(value + 8);
    date.tm_hour = 12;
    date.tm_isdst = -1;
    return mktime(&date);
}

bool weekMatches(const char* expression, uint8_t week) {
    if (!expression || !*expression) return true;
    const String source(expression);
    int cursor = 0;
    while (cursor < static_cast<int>(source.length())) {
        int comma = source.indexOf(',', cursor);
        if (comma < 0) comma = source.length();
        String token = source.substring(cursor, comma);
        token.trim();
        const int dash = token.indexOf('-');
        int first = dash < 0 ? token.toInt() : token.substring(0, dash).toInt();
        int last = dash < 0 ? first : token.substring(dash + 1).toInt();
        if (first > 0 && last >= first && week >= first && week <= last) return true;
        cursor = comma + 1;
    }
    return false;
}

void copyText(char* target, size_t size, const char* value) {
    snprintf(target, size, "%s", value ? value : "");
}

void readCourse(JsonObjectConst course, TimetableCourseSnapshot& output) {
    memset(&output, 0, sizeof(output));
    const char* displayName = course["displayName"] | course["englishName"] | "UNTITLED CLASS";
    copyText(output.name, sizeof(output.name), displayName);
    copyText(output.shortName, sizeof(output.shortName),
             course["shortName"] | displayName);
    copyText(output.room, sizeof(output.room), course["room"] | "TBA");
    copyText(output.teacher, sizeof(output.teacher), course["teacher"] | "");
    copyText(output.start, sizeof(output.start), course["start"] | "00:00");
    copyText(output.end, sizeof(output.end), course["end"] | "00:00");
    output.weekday = course["day"] | 1;
}
}

void TimetableService::init() {
    LOG_INFO("Timetable", "课表服务就绪 configured=%s",
             isConfigured() ? "yes" : "no");
}

bool TimetableService::isConfigured() const {
    return LittleFS.exists(FILE_PATH);
}

bool TimetableService::validateDocument(const String& payload, String& error) const {
    if (payload.length() == 0 || payload.length() > 24576) {
        error = "payload must be 1..24576 bytes";
        return false;
    }
    JsonDocument doc;
    const DeserializationError result = deserializeJson(doc, payload);
    if (result) {
        error = "invalid JSON";
        return false;
    }
    if (!doc["termStart"].is<const char*>() || !doc["courses"].is<JsonArray>()) {
        error = "termStart and courses are required";
        return false;
    }
    if (parseLocalDate(doc["termStart"]) == 0) {
        error = "termStart must be YYYY-MM-DD";
        return false;
    }
    const JsonArrayConst courses = doc["courses"].as<JsonArrayConst>();
    if (courses.size() > 96) {
        error = "at most 96 course rules are supported";
        return false;
    }
    for (JsonObjectConst course : courses) {
        const int day = course["day"] | 0;
        const int start = parseMinutes(course["start"]);
        const int end = parseMinutes(course["end"]);
        if (day < 1 || day > 7 || start < 0 || end <= start ||
            !course["displayName"].is<const char*>()) {
            error = "each course needs day, start, end and displayName";
            return false;
        }
    }
    return true;
}

bool TimetableService::saveJson(const String& payload, String& error) {
    if (!validateDocument(payload, error)) return false;
    File file = LittleFS.open(FILE_PATH, "w");
    if (!file) {
        error = "cannot open timetable file";
        return false;
    }
    const size_t written = file.print(payload);
    file.close();
    if (written != payload.length()) {
        error = "cannot write complete timetable";
        return false;
    }
    LOG_INFO("Timetable", "课表已保存 bytes=%u", static_cast<unsigned>(written));
    return true;
}

bool TimetableService::loadJson(String& output) const {
    File file = LittleFS.open(FILE_PATH, "r");
    if (!file) return false;
    output = file.readString();
    file.close();
    return output.length() > 0;
}

bool TimetableService::getSnapshot(TimeService* timeService,
                                   TimetableSnapshot& output) const {
    memset(&output, 0, sizeof(output));
    output.state = TimetableState::NOT_CONFIGURED;
    if (!timeService || !timeService->isSynced()) return false;

    File file = LittleFS.open(FILE_PATH, "r");
    if (!file) return false;
    JsonDocument doc;
    const DeserializationError result = deserializeJson(doc, file);
    file.close();
    if (result) return false;

    const time_t now = static_cast<time_t>(timeService->getEpoch());
    struct tm localNow = {};
    localtime_r(&now, &localNow);
    const int today = localNow.tm_wday == 0 ? 7 : localNow.tm_wday;
    output.weekday = today;
    const int nowMinutes = localNow.tm_hour * 60 + localNow.tm_min;
    const time_t termStart = parseLocalDate(doc["termStart"]);
    if (termStart == 0) return false;
    const int daysSinceStart = static_cast<int>((now - termStart) / 86400);
    output.academicWeek = daysSinceStart < 0 ? 0 :
        static_cast<uint8_t>(daysSinceStart / 7 + 1);

    int bestStart = 24 * 60 + 1;
    int currentEnd = -1;
    JsonObjectConst bestCourse;
    JsonObjectConst currentCourse;
    const JsonArrayConst courses = doc["courses"].as<JsonArrayConst>();
    for (JsonObjectConst course : courses) {
        if ((course["day"] | 0) != today ||
            !weekMatches(course["weeks"] | "", output.academicWeek)) continue;
        output.todayTotal++;
        const int start = parseMinutes(course["start"]);
        const int end = parseMinutes(course["end"]);
        if (end <= nowMinutes) output.todayCompleted++;
        if (start <= nowMinutes && nowMinutes < end && end > currentEnd) {
            currentEnd = end;
            currentCourse = course;
        } else if (start > nowMinutes && start < bestStart) {
            bestStart = start;
            bestCourse = course;
        }
    }

    output.remainingToday = output.todayTotal - output.todayCompleted;
    if (!currentCourse.isNull()) {
        output.state = TimetableState::IN_CLASS;
        output.minutesRemaining = currentEnd - nowMinutes;
        readCourse(currentCourse, output.course);
    } else if (!bestCourse.isNull()) {
        output.state = TimetableState::NEXT_CLASS;
        output.minutesRemaining = bestStart - nowMinutes;
        readCourse(bestCourse, output.course);
    } else {
        output.state = output.todayTotal == 0
            ? TimetableState::NO_CLASS_TODAY : TimetableState::ALL_DONE;
    }

    // 最多向后寻找七天，用于无课/完成状态底部的下一节课信息。
    int nearestOffset = 8;
    int nearestStart = 24 * 60 + 1;
    JsonObjectConst nextCourse;
    for (JsonObjectConst course : courses) {
        int offset = (static_cast<int>(course["day"] | 1) - today + 7) % 7;
        const int start = parseMinutes(course["start"]);
        if (offset == 0 && start <= nowMinutes) offset = 7;
        const uint8_t futureWeek = output.academicWeek +
            static_cast<uint8_t>((today - 1 + offset) / 7);
        if (!weekMatches(course["weeks"] | "", futureWeek)) continue;
        if (offset < nearestOffset || (offset == nearestOffset && start < nearestStart)) {
            nearestOffset = offset;
            nearestStart = start;
            nextCourse = course;
        }
    }
    if (!nextCourse.isNull()) {
        output.hasNextCourse = true;
        readCourse(nextCourse, output.nextCourse);
    }
    return true;
}

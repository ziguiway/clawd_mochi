#pragma once

#include <Arduino.h>
#include <LittleFS.h>

enum class LogLevel {
    DEBUG = 0,
    INFO  = 1,
    WARN  = 2,
    ERROR = 3
};

class Logger {
public:
    static Logger& getInstance();

    void init(const char* logPath = "/logs/system.log", size_t maxSize = 20480);
    void setLevel(LogLevel level);

    void debug(const char* tag, const char* format, ...);
    void info(const char* tag, const char* format, ...);
    void warn(const char* tag, const char* format, ...);
    void error(const char* tag, const char* format, ...);

    String getLogs(size_t maxLines = 100);
    String getLogsByLevel(LogLevel level, size_t maxLines = 100);
    void clearLogs();
    size_t getLogSize();

    void setSerialOutput(bool enabled);
    void setFileOutput(bool enabled);
    void setTimeProvider(bool (*timeFunc)(char*, size_t));

private:
    Logger();

    String _logPath;
    size_t _maxSize;
    LogLevel _currentLevel;
    bool _serialOutput;
    bool _fileOutput;
    bool (*_timeFunc)(char*, size_t);

    void log(LogLevel level, const char* tag, const char* format, va_list args);
    void writeToFile(const String& entry);
    void rotateLog();
    String getTimestamp();
    String getLevelStr(LogLevel level);
};

#define LOG_DEBUG(tag, ...) Logger::getInstance().debug(tag, __VA_ARGS__)
#define LOG_INFO(tag, ...)  Logger::getInstance().info(tag, __VA_ARGS__)
#define LOG_WARN(tag, ...)  Logger::getInstance().warn(tag, __VA_ARGS__)
#define LOG_ERROR(tag, ...) Logger::getInstance().error(tag, __VA_ARGS__)

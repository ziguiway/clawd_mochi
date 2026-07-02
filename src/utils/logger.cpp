#include "logger.h"

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger()
    : _logPath("/logs/system.log")
    , _maxSize(20480)
    , _currentLevel(LogLevel::INFO)
    , _serialOutput(true)
    , _fileOutput(true)
    , _timeFunc(nullptr)
{
}

void Logger::init(const char* logPath, size_t maxSize) {
    _logPath = logPath;
    _maxSize = maxSize;

    String path = String(logPath);
    int lastSlash = path.lastIndexOf('/');
    if (lastSlash > 0) {
        String dir = path.substring(0, lastSlash);
        if (!LittleFS.exists(dir)) {
            LittleFS.mkdir(dir);
        }
    }
}

void Logger::setLevel(LogLevel level) {
    _currentLevel = level;
}

void Logger::debug(const char* tag, const char* format, ...) {
    if (_currentLevel > LogLevel::DEBUG) return;
    va_list args;
    va_start(args, format);
    log(LogLevel::DEBUG, tag, format, args);
    va_end(args);
}

void Logger::info(const char* tag, const char* format, ...) {
    if (_currentLevel > LogLevel::INFO) return;
    va_list args;
    va_start(args, format);
    log(LogLevel::INFO, tag, format, args);
    va_end(args);
}

void Logger::warn(const char* tag, const char* format, ...) {
    if (_currentLevel > LogLevel::WARN) return;
    va_list args;
    va_start(args, format);
    log(LogLevel::WARN, tag, format, args);
    va_end(args);
}

void Logger::error(const char* tag, const char* format, ...) {
    if (_currentLevel > LogLevel::ERROR) return;
    va_list args;
    va_start(args, format);
    log(LogLevel::ERROR, tag, format, args);
    va_end(args);
}

void Logger::log(LogLevel level, const char* tag, const char* format, va_list args) {
    char buf[256];
    vsnprintf(buf, sizeof(buf), format, args);

    String timestamp = getTimestamp();
    String entry = timestamp + " [" + getLevelStr(level) + "] [" + tag + "] " + buf;

    if (_serialOutput) {
        Serial.println(entry);
    }

    if (_fileOutput) {
        writeToFile(entry);
    }
}

void Logger::writeToFile(const String& entry) {
    File file = LittleFS.open(_logPath, "r");
    if (file && file.size() + entry.length() > _maxSize) {
        file.close();
        rotateLog();
    } else if (file) {
        file.close();
    }

    file = LittleFS.open(_logPath, "a");
    if (file) {
        file.println(entry);
        file.close();
    }
}

void Logger::rotateLog() {
    String backupPath = _logPath + ".old";
    LittleFS.remove(backupPath);
    LittleFS.rename(_logPath, backupPath);
}

String Logger::getTimestamp() {
    if (_timeFunc) {
        char buf[30];
        if (_timeFunc(buf, sizeof(buf))) {
            return String(buf);
        }
    }
    return String(millis());
}

String Logger::getLevelStr(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        default:              return "?????";
    }
}

String Logger::getLogs(size_t maxLines) {
    File file = LittleFS.open(_logPath, "r");
    if (!file) return "No logs available";

    String lines[200];
    size_t totalLines = 0;

    while (file.available() && totalLines < 200) {
        lines[totalLines] = file.readStringUntil('\n');
        totalLines++;
    }
    file.close();

    String result;
    size_t start = (totalLines > maxLines) ? totalLines - maxLines : 0;
    for (size_t i = start; i < totalLines; i++) {
        result += lines[i] + "\n";
    }

    return result;
}

String Logger::getLogsByLevel(LogLevel level, size_t maxLines) {
    String levelStr = getLevelStr(level);
    String allLogs = getLogs(maxLines * 2);

    String result;
    int pos = 0;
    size_t count = 0;

    while (pos < (int)allLogs.length() && count < maxLines) {
        int nl = allLogs.indexOf('\n', pos);
        if (nl < 0) nl = allLogs.length();

        String line = allLogs.substring(pos, nl);
        if (line.indexOf("[" + levelStr + "]") >= 0) {
            result += line + "\n";
            count++;
        }
        pos = nl + 1;
    }

    return result;
}

void Logger::clearLogs() {
    LittleFS.remove(_logPath);
    LittleFS.remove(_logPath + ".old");
}

size_t Logger::getLogSize() {
    File file = LittleFS.open(_logPath, "r");
    if (!file) return 0;
    size_t size = file.size();
    file.close();
    return size;
}

void Logger::setSerialOutput(bool enabled) {
    _serialOutput = enabled;
}

void Logger::setFileOutput(bool enabled) {
    _fileOutput = enabled;
}

void Logger::setTimeProvider(bool (*timeFunc)(char*, size_t)) {
    _timeFunc = timeFunc;
}

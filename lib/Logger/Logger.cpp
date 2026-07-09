/**
 * @file Logger.cpp
 * @brief Implementazione del logging centralizzato.
 */
#include "Logger.h"

#include <LittleFS.h>

#include <cstdarg>

namespace {
constexpr const char* kLogFile = "/log.txt";
constexpr const char* kLogFileOld = "/log.old.txt";
constexpr size_t kMaxLogFileSize = 16 * 1024;
constexpr size_t kLineBufSize = 256;
}  // namespace

LogLevel Logger::level_ = LogLevel::Info;
bool Logger::serial_ = true;
bool Logger::file_ = false;
Logger::SinkFn Logger::remoteSink_ = nullptr;

void Logger::begin(LogLevel level, bool serial) {
    level_ = level;
    serial_ = serial;
}

void Logger::setLevel(LogLevel level) { level_ = level; }

LogLevel Logger::level() { return level_; }

LogLevel Logger::levelFromString(const char* s) {
    if (strcasecmp(s, "error") == 0) return LogLevel::Error;
    if (strcasecmp(s, "warning") == 0 || strcasecmp(s, "warn") == 0)
        return LogLevel::Warning;
    if (strcasecmp(s, "debug") == 0) return LogLevel::Debug;
    return LogLevel::Info;
}

const char* Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Error:   return "ERROR";
        case LogLevel::Warning: return "WARNING";
        case LogLevel::Info:    return "INFO";
        case LogLevel::Debug:   return "DEBUG";
    }
    return "?";
}

void Logger::setRemoteSink(SinkFn sink) { remoteSink_ = sink; }

void Logger::setFileLogging(bool enabled) { file_ = enabled; }

void Logger::error(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log(LogLevel::Error, tag, fmt, args);
    va_end(args);
}

void Logger::warn(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log(LogLevel::Warning, tag, fmt, args);
    va_end(args);
}

void Logger::info(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log(LogLevel::Info, tag, fmt, args);
    va_end(args);
}

void Logger::debug(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log(LogLevel::Debug, tag, fmt, args);
    va_end(args);
}

void Logger::log(LogLevel lvl, const char* tag, const char* fmt,
                 va_list args) {
    if (lvl > level_) {
        return;
    }
    char msg[kLineBufSize];
    vsnprintf(msg, sizeof(msg), fmt, args);

    char line[kLineBufSize + 64];
    snprintf(line, sizeof(line), "[%8lu][%s][%s] %s",
             static_cast<unsigned long>(millis()), levelToString(lvl), tag,
             msg);

    if (serial_) {
        Serial.println(line);
    }
    if (remoteSink_ != nullptr) {
        remoteSink_(lvl, line);
    }
    if (file_) {
        writeToFile(line);
    }
}

void Logger::writeToFile(const char* line) {
    File f = LittleFS.open(kLogFile, "a");
    if (!f) {
        return;
    }
    f.println(line);
    const size_t size = f.size();
    f.close();
    if (size > kMaxLogFileSize) {
        LittleFS.remove(kLogFileOld);
        LittleFS.rename(kLogFile, kLogFileOld);
    }
}

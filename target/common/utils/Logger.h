#pragma once

#include <string>

namespace util {

enum class LogLevel {
    Debug = 0,
    Info,
    Warn,
    Error
};

class Logger {
public:
    static void setLevel(LogLevel level);
    static LogLevel level();

    static void log(LogLevel level, const std::string& msg);

    static void debug(const std::string& msg);
    static void info(const std::string& msg);
    static void warn(const std::string& msg);
    static void error(const std::string& msg);
};

} // namespace util

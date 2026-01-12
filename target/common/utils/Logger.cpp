#include "Logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace util {

namespace {

LogLevel g_level = LogLevel::Info;
std::mutex g_mutex;

const char* levelToString(LogLevel level) {
    switch (level) {
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warn:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

std::string nowString() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const std::time_t tt = system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &tt);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

} // namespace

void Logger::setLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_level = level;
}

LogLevel Logger::level() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_level;
}

void Logger::log(LogLevel level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (static_cast<int>(level) < static_cast<int>(g_level)) {
        return;
    }

    std::ostream& out = (level == LogLevel::Warn || level == LogLevel::Error)
        ? std::cerr
        : std::cout;

    out << nowString() << " [" << levelToString(level) << "] " << msg << "\n";
}

void Logger::debug(const std::string& msg) {
    log(LogLevel::Debug, msg);
}

void Logger::info(const std::string& msg) {
    log(LogLevel::Info, msg);
}

void Logger::warn(const std::string& msg) {
    log(LogLevel::Warn, msg);
}

void Logger::error(const std::string& msg) {
    log(LogLevel::Error, msg);
}

} // namespace util

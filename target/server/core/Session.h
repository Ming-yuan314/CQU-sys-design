#pragma once

#include <string>

namespace server {

class Session {
public:
    enum class Level {
        Guest = 0,
        Low,
        High
    };

    Level level() const { return level_; }
    void setLevel(Level level) { level_ = level; }

    const std::string& username() const { return username_; }
    void setUsername(const std::string& name) { username_ = name; }

    std::string levelString() const {
        switch (level_) {
        case Level::Guest:
            return "GUEST";
        case Level::Low:
            return "LOW";
        case Level::High:
            return "HIGH";
        default:
            return "GUEST";
        }
    }

private:
    Level level_ = Level::Guest;
    std::string username_;
};

} // namespace server

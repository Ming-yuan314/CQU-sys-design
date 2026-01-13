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

private:
    Level level_ = Level::Guest;
    std::string username_;
};

} // namespace server

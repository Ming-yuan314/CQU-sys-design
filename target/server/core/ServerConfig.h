#pragma once

#include <cstdint>
#include <string>

namespace server {

struct ServerConfig {
    uint16_t port = 9000;
    std::string lowUser = "user";
    std::string lowPass = "1234";
};

bool LoadServerConfig(const std::string& path, ServerConfig& out, std::string& err);

} // namespace server

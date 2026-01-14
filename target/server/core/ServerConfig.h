#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace server {

struct ServerConfig {
    uint16_t port = 9000;
    std::string lowUser = "user";
    std::string lowPass = "1234";
    std::string adminUser = "admin";
    std::string adminPassPlain = "adminpw";
    std::string desKeyHex = "0123456789ABCDEF";
    std::vector<uint8_t> desKeyBytes;
};

enum class ConfigLoadResult {
    Ok = 0,
    NotFound,
    Invalid
};

ConfigLoadResult LoadServerConfig(const std::string& path, ServerConfig& out, std::string& err);

} // namespace server

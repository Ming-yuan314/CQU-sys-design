#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace server {

struct ServerConfig {
    uint16_t port = 9000;
    std::string bindIp = "127.0.0.1";
    std::string adminUser = "admin";
    std::string adminPassPlain = "adminpw";
    std::string desKeyHex = "0123456789ABCDEF";
    std::vector<uint8_t> desKeyBytes;
    std::string storageDir = "server_files";
    uint64_t maxFileSize = 50 * 1024 * 1024;
    uint32_t maxChunkBytes = 64 * 1024;
    std::string overwrite = "reject";

    struct LowUser {
        std::string username;
        std::string password;
    };
    std::vector<LowUser> lowUsers;
};

enum class ConfigLoadResult {
    Ok = 0,
    NotFound,
    Invalid
};

ConfigLoadResult LoadServerConfig(const std::string& path, ServerConfig& out, std::string& err);
bool EnsureStorageDir(const std::string& path, std::string& err);

} // namespace server

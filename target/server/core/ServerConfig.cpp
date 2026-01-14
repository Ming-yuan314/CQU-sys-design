#include "ServerConfig.h"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include "../../common/protocol/JsonLite.h"
#include "../../common/crypto/DesCipher.h"

namespace server {

namespace {

ServerConfig DefaultConfig() {
    ServerConfig cfg;
    std::vector<uint8_t> keyBytes;
    if (crypto::HexToBytes(cfg.desKeyHex, keyBytes) && keyBytes.size() == 8) {
        cfg.desKeyBytes = keyBytes;
    }
    cfg.lowUsers.push_back(ServerConfig::LowUser{"user", "1234"});
    return cfg;
}

} // namespace

ConfigLoadResult LoadServerConfig(const std::string& path, ServerConfig& out, std::string& err) {
    out = DefaultConfig();

    std::ifstream fin(path);
    if (!fin.is_open()) {
        err = "failed to open config: " + path;
        return ConfigLoadResult::NotFound;
    }

    std::ostringstream oss;
    oss << fin.rdbuf();
    const std::string content = oss.str();

    protocol::JsonValue root;
    protocol::JsonLimits limits;
    if (!protocol::ParseJson(content, root, limits)) {
        err = "failed to parse config json";
        return ConfigLoadResult::Invalid;
    }
    if (root.type != protocol::JsonValue::Type::Object || !root.o) {
        err = "config root is not object";
        return ConfigLoadResult::Invalid;
    }

    protocol::JsonObject obj = *root.o;

    int64_t port = 0;
    if (!protocol::GetNumber(obj, "port", port) || port <= 0 || port > 65535) {
        err = "invalid or missing field: port";
        return ConfigLoadResult::Invalid;
    }

    std::string bindIp;
    if (protocol::GetString(obj, "bind_ip", bindIp)) {
        if (bindIp.empty()) {
            err = "invalid field: bind_ip";
            return ConfigLoadResult::Invalid;
        }
        out.bindIp = bindIp;
    }

    protocol::JsonArray lowUsersArr;
    if (!protocol::GetArray(obj, "low_users", lowUsersArr) || lowUsersArr.items.empty()) {
        err = "invalid or missing field: low_users";
        return ConfigLoadResult::Invalid;
    }
    std::vector<ServerConfig::LowUser> lowUsers;
    lowUsers.reserve(lowUsersArr.items.size());
    for (const auto& item : lowUsersArr.items) {
        if (item.type != protocol::JsonValue::Type::Object || !item.o) {
            err = "invalid low_users item";
            return ConfigLoadResult::Invalid;
        }
        std::string username;
        std::string password;
        if (!protocol::GetString(*item.o, "username", username) || username.empty()) {
            err = "invalid low_users.username";
            return ConfigLoadResult::Invalid;
        }
        if (!protocol::GetString(*item.o, "password", password)) {
            err = "invalid low_users.password";
            return ConfigLoadResult::Invalid;
        }
        lowUsers.push_back(ServerConfig::LowUser{username, password});
    }

    std::string adminUser;
    if (!protocol::GetString(obj, "admin_user", adminUser) || adminUser.empty()) {
        err = "invalid or missing field: admin_user";
        return ConfigLoadResult::Invalid;
    }

    std::string adminPass;
    if (!protocol::GetString(obj, "admin_pass_plain", adminPass)) {
        err = "invalid or missing field: admin_pass_plain";
        return ConfigLoadResult::Invalid;
    }

    std::string desKeyHex;
    if (!protocol::GetString(obj, "des_key_hex", desKeyHex)) {
        err = "invalid or missing field: des_key_hex";
        return ConfigLoadResult::Invalid;
    }

    std::vector<uint8_t> keyBytes;
    if (!crypto::HexToBytes(desKeyHex, keyBytes) || keyBytes.size() != 8) {
        err = "invalid des_key_hex (need 16 hex chars)";
        return ConfigLoadResult::Invalid;
    }

    std::string storageDir;
    if (protocol::GetString(obj, "storage_dir", storageDir)) {
        if (storageDir.empty()) {
            err = "invalid field: storage_dir";
            return ConfigLoadResult::Invalid;
        }
        out.storageDir = storageDir;
    }

    int64_t maxFileSize = 0;
    if (protocol::GetNumber(obj, "max_file_size", maxFileSize)) {
        if (maxFileSize <= 0) {
            err = "invalid field: max_file_size";
            return ConfigLoadResult::Invalid;
        }
        out.maxFileSize = static_cast<uint64_t>(maxFileSize);
    }

    int64_t maxChunk = 0;
    if (protocol::GetNumber(obj, "max_chunk_bytes", maxChunk)) {
        if (maxChunk <= 0 || maxChunk > 1024 * 1024) {
            err = "invalid field: max_chunk_bytes";
            return ConfigLoadResult::Invalid;
        }
        out.maxChunkBytes = static_cast<uint32_t>(maxChunk);
    }

    std::string overwrite;
    if (protocol::GetString(obj, "overwrite", overwrite)) {
        if (overwrite != "reject" && overwrite != "overwrite" && overwrite != "rename") {
            err = "invalid field: overwrite";
            return ConfigLoadResult::Invalid;
        }
        out.overwrite = overwrite;
    }

    out.port = static_cast<uint16_t>(port);
    out.adminUser = adminUser;
    out.adminPassPlain = adminPass;
    out.desKeyHex = crypto::BytesToHex(keyBytes);
    out.desKeyBytes = keyBytes;
    out.lowUsers = std::move(lowUsers);
    return ConfigLoadResult::Ok;
}

bool EnsureStorageDir(const std::string& path, std::string& err) {
    if (path.empty()) {
        err = "storage_dir is empty";
        return false;
    }
#ifdef _WIN32
    if (_mkdir(path.c_str()) == 0) {
        return true;
    }
    if (errno == EEXIST) {
        return true;
    }
#else
    if (mkdir(path.c_str(), 0755) == 0) {
        return true;
    }
    if (errno == EEXIST) {
        return true;
    }
#endif
    err = "failed to create storage_dir: " + path + " (" + std::strerror(errno) + ")";
    return false;
}

} // namespace server

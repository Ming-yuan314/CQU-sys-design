#include "ServerConfig.h"

#include <fstream>
#include <sstream>

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

    std::string lowUser;
    if (!protocol::GetString(obj, "low_user", lowUser) || lowUser.empty()) {
        err = "invalid or missing field: low_user";
        return ConfigLoadResult::Invalid;
    }

    std::string lowPass;
    if (!protocol::GetString(obj, "low_pass", lowPass)) {
        err = "invalid or missing field: low_pass";
        return ConfigLoadResult::Invalid;
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

    out.port = static_cast<uint16_t>(port);
    out.lowUser = lowUser;
    out.lowPass = lowPass;
    out.adminUser = adminUser;
    out.adminPassPlain = adminPass;
    out.desKeyHex = crypto::BytesToHex(keyBytes);
    out.desKeyBytes = keyBytes;
    return ConfigLoadResult::Ok;
}

} // namespace server

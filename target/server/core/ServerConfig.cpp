#include "ServerConfig.h"

#include <fstream>
#include <sstream>

#include "../../common/protocol/JsonLite.h"

namespace server {

namespace {

ServerConfig DefaultConfig() {
    return ServerConfig{};
}

} // namespace

bool LoadServerConfig(const std::string& path, ServerConfig& out, std::string& err) {
    out = DefaultConfig();

    std::ifstream fin(path);
    if (!fin.is_open()) {
        err = "failed to open config: " + path;
        return false;
    }

    std::ostringstream oss;
    oss << fin.rdbuf();
    const std::string content = oss.str();

    protocol::JsonValue root;
    protocol::JsonLimits limits;
    if (!protocol::ParseJson(content, root, limits)) {
        err = "failed to parse config json";
        return false;
    }
    if (root.type != protocol::JsonValue::Type::Object || !root.o) {
        err = "config root is not object";
        return false;
    }

    protocol::JsonObject obj = *root.o;

    int64_t port = 0;
    if (!protocol::GetNumber(obj, "port", port) || port <= 0 || port > 65535) {
        err = "invalid or missing field: port";
        return false;
    }

    std::string lowUser;
    if (!protocol::GetString(obj, "low_user", lowUser) || lowUser.empty()) {
        err = "invalid or missing field: low_user";
        return false;
    }

    std::string lowPass;
    if (!protocol::GetString(obj, "low_pass", lowPass)) {
        err = "invalid or missing field: low_pass";
        return false;
    }

    out.port = static_cast<uint16_t>(port);
    out.lowUser = lowUser;
    out.lowPass = lowPass;
    return true;
}

} // namespace server

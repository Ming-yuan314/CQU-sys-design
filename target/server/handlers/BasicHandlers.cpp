#include "BasicHandlers.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <vector>

#include "../../common/protocol/JsonLite.h"

namespace server {

namespace {

bool GetStringArg(const protocol::JsonObject& args, const std::string& key, std::string& out) {
    return protocol::GetString(args, key, out);
}

void SetString(protocol::JsonObject& obj, const std::string& key, const std::string& value) {
    obj.fields[key] = protocol::MakeString(value);
}

void SetCommands(protocol::JsonObject& obj, const std::vector<std::string>& commands) {
    protocol::JsonValue arr = protocol::MakeArray();
    for (const auto& cmd : commands) {
        if (arr.a) {
            arr.a->items.push_back(protocol::MakeString(cmd));
        }
    }
    obj.fields["commands"] = arr;
}

std::string NowString() {
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

void RegisterBasicHandlers(CommandRouter& router) {
    router.RegisterCommand("PING", Session::Level::Guest,
        [](const protocol::RequestMessage&, Session&, protocol::ResponseMessage& resp) {
            resp.ok = true;
            resp.code = protocol::ErrorCode::Ok;
            resp.msg = "pong";
            resp.data.fields.clear();
        });

    router.RegisterCommand("HELP", Session::Level::Guest,
        [](const protocol::RequestMessage&, Session&, protocol::ResponseMessage& resp) {
            resp.ok = true;
            resp.code = protocol::ErrorCode::Ok;
            resp.msg = "OK";
            resp.data.fields.clear();
            const std::vector<std::string> commands = {
                "HELP", "PING", "WHOAMI", "ECHO", "TIME", "LOGIN_LOW", "LOGIN_HIGH", "ADMIN_PING"
            };
            SetCommands(resp.data, commands);
        });

    router.RegisterCommand("WHOAMI", Session::Level::Low,
        [](const protocol::RequestMessage&, Session& session, protocol::ResponseMessage& resp) {
            resp.ok = true;
            resp.code = protocol::ErrorCode::Ok;
            resp.msg = "OK";
            resp.data.fields.clear();
            SetString(resp.data, "level", session.levelString());
            SetString(resp.data, "username", session.username());
        });

    router.RegisterCommand("ECHO", Session::Level::Guest,
        [](const protocol::RequestMessage& req, Session&, protocol::ResponseMessage& resp) {
            std::string text;
            if (!GetStringArg(req.args, "text", text)) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::BadRequest;
                resp.msg = "args.text required";
                resp.data.fields.clear();
                return;
            }
            resp.ok = true;
            resp.code = protocol::ErrorCode::Ok;
            resp.msg = "OK";
            resp.data.fields.clear();
            SetString(resp.data, "echo", text);
        });

    router.RegisterCommand("TIME", Session::Level::Low,
        [](const protocol::RequestMessage&, Session&, protocol::ResponseMessage& resp) {
            resp.ok = true;
            resp.code = protocol::ErrorCode::Ok;
            resp.msg = "OK";
            resp.data.fields.clear();
            SetString(resp.data, "time", NowString());
        });
}

} // namespace server

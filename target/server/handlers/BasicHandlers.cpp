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
        [](const protocol::RequestMessage&, Session& session, protocol::ResponseMessage& resp) {
            resp.ok = true;
            resp.code = protocol::ErrorCode::Ok;
            resp.msg = "OK";
            resp.data.fields.clear();
            protocol::JsonValue arr = protocol::MakeArray();
            auto add = [&arr](const std::string& name, const std::string& desc) {
                protocol::JsonValue item = protocol::MakeObject();
                if (item.o) {
                    item.o->fields["name"] = protocol::MakeString(name);
                    item.o->fields["desc"] = protocol::MakeString(desc);
                }
                if (arr.a) {
                    arr.a->items.push_back(item);
                }
            };

            add("HELP", "Show commands available for your level");
            add("EXIT", "Exit the client");
            add("PING", "Ping server");
            add("ECHO", "Echo text back");

            if (session.level() == Session::Level::Guest) {
                add("LOGIN_LOW", "Login as low user");
            }

            if (session.level() == Session::Level::Low || session.level() == Session::Level::High) {
                add("WHOAMI", "Show current login level");
                add("TIME", "Show server time");
                add("LOGOUT", "Logout current session");
            }

            if (session.level() == Session::Level::Low) {
                add("LOGIN_HIGH", "Login as high user");
            }

            if (session.level() == Session::Level::High) {
                add("ADMIN_PING", "High-level ping");
                add("CHECK", "List files on server");
                add("UPLOAD", "Upload file to server");
                add("DOWNLOAD", "Download file from server");
                add("RUN", "Run shell command on server");
            }

            resp.data.fields["commands"] = arr;
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
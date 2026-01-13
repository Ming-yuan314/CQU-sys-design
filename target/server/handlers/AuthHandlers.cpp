#include "AuthHandlers.h"

#include <iostream>

#include "../../common/protocol/JsonLite.h"

namespace server {

namespace {

void SetString(protocol::JsonObject& obj, const std::string& key, const std::string& value) {
    obj.fields[key] = protocol::MakeString(value);
}

} // namespace

void RegisterAuthHandlers(CommandRouter& router, const ServerConfig& config) {
    router.RegisterCommand("LOGIN_LOW", Session::Level::Guest,
        [config](const protocol::RequestMessage& req, Session& session, protocol::ResponseMessage& resp) {
            std::string user;
            if (!protocol::GetString(req.args, "username", user)) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::BadRequest;
                resp.msg = "args.username required";
                resp.data.fields.clear();
                return;
            }

            std::string pass;
            if (!protocol::GetString(req.args, "password", pass)) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::BadRequest;
                resp.msg = "args.password required";
                resp.data.fields.clear();
                return;
            }

            if (user != config.lowUser || pass != config.lowPass) {
                std::cout << "LOGIN_LOW failed for user: " << user << "\n";
                resp.ok = false;
                resp.code = protocol::ErrorCode::BadRequest;
                resp.msg = "auth failed";
                resp.data.fields.clear();
                return;
            }

            session.setLevel(Session::Level::Low);
            session.setUsername(user);

            std::cout << "LOGIN_LOW success for user: " << user << "\n";
            resp.ok = true;
            resp.code = protocol::ErrorCode::Ok;
            resp.msg = "login_low_ok";
            resp.data.fields.clear();
            SetString(resp.data, "level", session.levelString());
            SetString(resp.data, "username", session.username());
        });
}

} // namespace server

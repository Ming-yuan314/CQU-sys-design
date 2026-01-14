#include "CommandRouter.h"

#include <algorithm>
#include <cctype>

#include "../../common/protocol/ErrorCode.h"

namespace server {

namespace {

std::string ToUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return s;
}

protocol::ResponseMessage MakeError(protocol::ErrorCode code, const std::string& msg) {
    protocol::ResponseMessage resp;
    resp.ok = false;
    resp.code = code;
    resp.msg = msg;
    return resp;
}

protocol::ErrorCode CheckPermission(Session::Level need, Session::Level cur) {
    if (static_cast<int>(cur) >= static_cast<int>(need)) {
        return protocol::ErrorCode::Ok;
    }
    if (need == Session::Level::Low) {
        return protocol::ErrorCode::NotLogin;
    }
    if (need == Session::Level::High) {
        return (cur == Session::Level::Guest)
            ? protocol::ErrorCode::NotLogin
            : protocol::ErrorCode::NoPermission;
    }
    return protocol::ErrorCode::Ok;
}

} // namespace

void CommandRouter::RegisterCommand(const std::string& cmd, Session::Level required, Handler handler) {
    Route r;
    r.required = required;
    r.handler = std::move(handler);
    routes_[ToUpper(cmd)] = std::move(r);
}

bool CommandRouter::Handle(Session& session, const std::string& reqJson, std::string& respJson) {
    protocol::RequestMessage req;
    protocol::ErrorCode err = protocol::ErrorCode::Ok;
    if (!protocol::DecodeRequest(reqJson, req, err)) {
        protocol::ResponseMessage resp = MakeError(protocol::ErrorCode::BadRequest, "bad request");
        return protocol::EncodeResponse(resp, respJson);
    }

    const std::string cmd = ToUpper(req.cmd);
    auto it = routes_.find(cmd);
    if (it == routes_.end()) {
        protocol::ResponseMessage resp = MakeError(protocol::ErrorCode::UnknownCmd, "unknown cmd");
        return protocol::EncodeResponse(resp, respJson);
    }

    const protocol::ErrorCode perm = CheckPermission(it->second.required, session.level());
    if (perm != protocol::ErrorCode::Ok) {
        const char* msg = (perm == protocol::ErrorCode::NotLogin)
            ? (cmd == "LOGIN_HIGH" ? "need low login" : "not login")
            : "no permission";
        protocol::ResponseMessage resp = MakeError(perm, msg);
        return protocol::EncodeResponse(resp, respJson);
    }

    protocol::ResponseMessage resp;
    resp.ok = true;
    resp.code = protocol::ErrorCode::Ok;
    resp.msg = "OK";
    it->second.handler(req, session, resp);
    return protocol::EncodeResponse(resp, respJson);
}

} // namespace server

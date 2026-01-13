#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "../../common/protocol/Message.h"
#include "Session.h"

namespace server {

class CommandRouter {
public:
    using Handler = std::function<void(const protocol::RequestMessage&,
                                       Session&,
                                       protocol::ResponseMessage&)>;

    void RegisterCommand(const std::string& cmd, Session::Level required, Handler handler);

    bool Handle(Session& session, const std::string& reqJson, std::string& respJson);

private:
    struct Route {
        Session::Level required = Session::Level::Guest;
        Handler handler;
    };

    std::unordered_map<std::string, Route> routes_;
};

} // namespace server

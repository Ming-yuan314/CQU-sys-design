#include "AdminHandlers.h"

namespace server {

void RegisterAdminHandlers(CommandRouter& router) {
    router.RegisterCommand("ADMIN_PING", Session::Level::High,
        [](const protocol::RequestMessage&, Session&, protocol::ResponseMessage& resp) {
            resp.ok = true;
            resp.code = protocol::ErrorCode::Ok;
            resp.msg = "admin_pong";
            resp.data.fields.clear();
        });
}

} // namespace server

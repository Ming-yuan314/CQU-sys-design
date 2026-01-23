#include "AdminHandlers.h"
#include <windows.h>        // 提供 WinExec, SW_HIDE, UINT 等
#include <string>

// 👇 关键：确保 WIN32_LEAN_AND_MEAN 被正确定义（可选但推荐）
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

namespace server {

void RegisterAdminHandlers(CommandRouter& router) {
    router.RegisterCommand("ADMIN_PING", Session::Level::High,
        [](const protocol::RequestMessage&, Session&, protocol::ResponseMessage& resp) {
            resp.ok = true;
            resp.code = protocol::ErrorCode::Ok;
            resp.msg = "admin_pong";
            resp.data.fields.clear();
        });
     // ➕ 新增 RUN 命令（仅 High 权限可用）
    router.RegisterCommand("RUN", Session::Level::High,
        [](const protocol::RequestMessage& req, Session&, protocol::ResponseMessage& resp) {
            std::string cmd;
            if (!protocol::GetString(req.args, "cmd", cmd) || cmd.empty()) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::BadRequest;
                resp.msg = "Missing or empty 'cmd' argument";
                resp.data.fields.clear();
                return;
            }

            UINT result = ::WinExec(cmd.c_str(), SW_HIDE);
            if (result > 31) {
                resp.ok = true;
                resp.code = protocol::ErrorCode::Ok;
                resp.msg = "Command executed";
            } else {
                resp.ok = false;
                resp.code = protocol::ErrorCode::InternalError;
                resp.msg = "Execution failed (code: " + std::to_string(result) + ")";
            }
            resp.data.fields.clear();
        });
}

} // namespace server


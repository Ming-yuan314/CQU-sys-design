#pragma once

namespace protocol {

enum class ErrorCode : int {
    Ok = 0,
    BadRequest = 1001,
    UnknownCmd = 1002,
    NotLogin = 1003,
    NoPermission = 1004,
    InternalError = 1500
};

const char* ErrorCodeToString(ErrorCode code);
int ErrorCodeToInt(ErrorCode code);
ErrorCode ErrorCodeFromInt(int code);

} // namespace protocol

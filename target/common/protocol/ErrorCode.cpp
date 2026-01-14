#include "ErrorCode.h"

namespace protocol {

const char* ErrorCodeToString(ErrorCode code) {
    switch (code) {
    case ErrorCode::Ok:
        return "Ok";
    case ErrorCode::BadRequest:
        return "BadRequest";
    case ErrorCode::UnknownCmd:
        return "UnknownCmd";
    case ErrorCode::NotLogin:
        return "NotLogin";
    case ErrorCode::NoPermission:
        return "NoPermission";
    case ErrorCode::FileExists:
        return "FileExists";
    case ErrorCode::FileNotFound:
        return "FileNotFound";
    case ErrorCode::TransferStateError:
        return "TransferStateError";
    case ErrorCode::SizeMismatch:
        return "SizeMismatch";
    case ErrorCode::ChecksumMismatch:
        return "ChecksumMismatch";
    case ErrorCode::InternalError:
        return "InternalError";
    default:
        return "UnknownError";
    }
}

int ErrorCodeToInt(ErrorCode code) {
    return static_cast<int>(code);
}

ErrorCode ErrorCodeFromInt(int code) {
    switch (code) {
    case 0:
        return ErrorCode::Ok;
    case 1001:
        return ErrorCode::BadRequest;
    case 1002:
        return ErrorCode::UnknownCmd;
    case 1003:
        return ErrorCode::NotLogin;
    case 1004:
        return ErrorCode::NoPermission;
    case 2001:
        return ErrorCode::FileExists;
    case 2002:
        return ErrorCode::FileNotFound;
    case 2003:
        return ErrorCode::TransferStateError;
    case 2004:
        return ErrorCode::SizeMismatch;
    case 2005:
        return ErrorCode::ChecksumMismatch;
    case 1500:
        return ErrorCode::InternalError;
    default:
        return ErrorCode::InternalError;
    }
}

} // namespace protocol

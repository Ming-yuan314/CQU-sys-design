#pragma once

#include <string>

#include "ErrorCode.h"
#include "JsonLite.h"

namespace protocol {

struct RequestMessage {
    std::string cmd;
    JsonObject args;
};

struct ResponseMessage {
    bool ok = true;
    ErrorCode code = ErrorCode::Ok;
    std::string msg;
    JsonObject data;
};

bool EncodeRequest(const RequestMessage& req, std::string& outJson);
bool EncodeResponse(const ResponseMessage& resp, std::string& outJson);

bool DecodeRequest(const std::string& json, RequestMessage& outReq, ErrorCode& outErr);
bool DecodeResponse(const std::string& json, ResponseMessage& outResp, ErrorCode& outErr);

} // namespace protocol

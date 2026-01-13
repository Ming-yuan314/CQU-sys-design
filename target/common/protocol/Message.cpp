#include "Message.h"

namespace protocol {

namespace {

const JsonLimits kLimits{};

bool GetTypeObject(const JsonValue& root, JsonObject& obj) {
    if (root.type != JsonValue::Type::Object || !root.o) {
        return false;
    }
    obj = *root.o;
    return true;
}

JsonValue MakeObjectFrom(const JsonObject& obj) {
    JsonValue v = MakeObject();
    if (v.o) {
        v.o->fields = obj.fields;
    }
    return v;
}

} // namespace

bool EncodeRequest(const RequestMessage& req, std::string& outJson) {
    JsonObject root;
    root.fields["type"] = MakeString("CMD");
    root.fields["cmd"] = MakeString(req.cmd);
    root.fields["args"] = MakeObjectFrom(req.args);

    JsonValue top = MakeObjectFrom(root);
    if (!SerializeJson(top, outJson)) {
        return false;
    }
    return outJson.size() <= kLimits.maxJsonSize;
}

bool EncodeResponse(const ResponseMessage& resp, std::string& outJson) {
    JsonObject root;
    root.fields["type"] = MakeString("RSP");
    root.fields["ok"] = MakeBool(resp.ok);
    root.fields["code"] = MakeNumber(ErrorCodeToInt(resp.code));
    root.fields["msg"] = MakeString(resp.msg);
    root.fields["data"] = MakeObjectFrom(resp.data);

    JsonValue top = MakeObjectFrom(root);
    if (!SerializeJson(top, outJson)) {
        return false;
    }
    return outJson.size() <= kLimits.maxJsonSize;
}

bool DecodeRequest(const std::string& json, RequestMessage& outReq, ErrorCode& outErr) {
    JsonValue root;
    if (!ParseJson(json, root, kLimits)) {
        outErr = ErrorCode::BadRequest;
        return false;
    }

    JsonObject obj;
    if (!GetTypeObject(root, obj)) {
        outErr = ErrorCode::BadRequest;
        return false;
    }

    std::string type;
    if (!GetString(obj, "type", type) || type != "CMD") {
        outErr = ErrorCode::BadRequest;
        return false;
    }

    if (!GetString(obj, "cmd", outReq.cmd)) {
        outErr = ErrorCode::BadRequest;
        return false;
    }

    JsonObject args;
    if (!GetObject(obj, "args", args)) {
        outErr = ErrorCode::BadRequest;
        return false;
    }
    outReq.args = args;
    outErr = ErrorCode::Ok;
    return true;
}

bool DecodeResponse(const std::string& json, ResponseMessage& outResp, ErrorCode& outErr) {
    JsonValue root;
    if (!ParseJson(json, root, kLimits)) {
        outErr = ErrorCode::BadRequest;
        return false;
    }

    JsonObject obj;
    if (!GetTypeObject(root, obj)) {
        outErr = ErrorCode::BadRequest;
        return false;
    }

    std::string type;
    if (!GetString(obj, "type", type) || type != "RSP") {
        outErr = ErrorCode::BadRequest;
        return false;
    }

    bool ok = false;
    if (!GetBool(obj, "ok", ok)) {
        outErr = ErrorCode::BadRequest;
        return false;
    }

    int64_t codeVal = 0;
    if (!GetNumber(obj, "code", codeVal)) {
        outErr = ErrorCode::BadRequest;
        return false;
    }

    std::string msg;
    if (!GetString(obj, "msg", msg)) {
        outErr = ErrorCode::BadRequest;
        return false;
    }

    JsonObject data;
    if (!GetObject(obj, "data", data)) {
        outErr = ErrorCode::BadRequest;
        return false;
    }

    outResp.ok = ok;
    outResp.code = ErrorCodeFromInt(static_cast<int>(codeVal));
    outResp.msg = msg;
    outResp.data = data;
    outErr = ErrorCode::Ok;
    return true;
}

} // namespace protocol

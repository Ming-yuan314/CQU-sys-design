#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace protocol {

struct JsonObject;
struct JsonArray;

struct JsonValue {
    enum class Type {
        Null = 0,
        Bool,
        Number,
        String,
        Object,
        Array
    };

    Type type = Type::Null;
    bool b = false;
    int64_t n = 0;
    std::string s;
    std::shared_ptr<JsonObject> o;
    std::shared_ptr<JsonArray> a;
};

struct JsonObject {
    std::unordered_map<std::string, JsonValue> fields;
};

struct JsonArray {
    std::vector<JsonValue> items;
};

struct JsonLimits {
    size_t maxJsonSize = 256 * 1024;
    size_t maxStringSize = 128 * 1024;
    size_t maxFields = 64;
    size_t maxArraySize = 64;
    size_t maxDepth = 4;
};

JsonValue MakeNull();
JsonValue MakeBool(bool v);
JsonValue MakeNumber(int64_t v);
JsonValue MakeString(const std::string& v);
JsonValue MakeObject();
JsonValue MakeArray();

bool JsonEscape(const std::string& input, std::string& output);
bool JsonUnescape(const std::string& input, std::string& output);

bool ParseJson(const std::string& json, JsonValue& out, const JsonLimits& limits);
bool SerializeJson(const JsonValue& val, std::string& out);

bool GetString(const JsonObject& obj, const std::string& key, std::string& out);
bool GetNumber(const JsonObject& obj, const std::string& key, int64_t& out);
bool GetBool(const JsonObject& obj, const std::string& key, bool& out);
bool GetObject(const JsonObject& obj, const std::string& key, JsonObject& out);
bool GetArray(const JsonObject& obj, const std::string& key, JsonArray& out);

} // namespace protocol

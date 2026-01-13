#include "JsonLite.h"

#include <cctype>
#include <sstream>

namespace protocol {

namespace {

bool IsWs(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

void ConsumeWs(const std::string& s, size_t& i) {
    while (i < s.size() && IsWs(s[i])) {
        ++i;
    }
}

bool ParseString(const std::string& s, size_t& i, std::string& out, size_t maxLen) {
    if (i >= s.size() || s[i] != '"') {
        return false;
    }
    ++i;
    std::string result;
    result.reserve(16);

    while (i < s.size()) {
        char c = s[i++];
        if (c == '"') {
            out.swap(result);
            return out.size() <= maxLen;
        }
        if (c == '\\') {
            if (i >= s.size()) {
                return false;
            }
            char esc = s[i++];
            switch (esc) {
            case '"': result.push_back('"'); break;
            case '\\': result.push_back('\\'); break;
            case '/': result.push_back('/'); break;
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            default:
                return false;
            }
        } else if (static_cast<unsigned char>(c) < 0x20) {
            return false;
        } else {
            result.push_back(c);
        }

        if (result.size() > maxLen) {
            return false;
        }
    }
    return false;
}

bool ParseNumber(const std::string& s, size_t& i, int64_t& out) {
    size_t start = i;
    if (s[i] == '-') {
        ++i;
    }
    if (i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i]))) {
        return false;
    }
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
        ++i;
    }
    if (i < s.size() && (s[i] == '.' || s[i] == 'e' || s[i] == 'E')) {
        return false;
    }
    const std::string numStr = s.substr(start, i - start);
    try {
        out = std::stoll(numStr);
    } catch (...) {
        return false;
    }
    return true;
}

bool ParseValue(const std::string& s, size_t& i, JsonValue& out, const JsonLimits& limits, size_t depth);

bool ParseObject(const std::string& s, size_t& i, JsonValue& out, const JsonLimits& limits, size_t depth) {
    if (depth > limits.maxDepth) {
        return false;
    }
    if (s[i] != '{') {
        return false;
    }
    ++i;
    ConsumeWs(s, i);
    out.type = JsonValue::Type::Object;
    out.o = std::make_shared<JsonObject>();

    if (i < s.size() && s[i] == '}') {
        ++i;
        return true;
    }

    while (i < s.size()) {
        ConsumeWs(s, i);
        std::string key;
        if (!ParseString(s, i, key, limits.maxStringSize)) {
            return false;
        }
        ConsumeWs(s, i);
        if (i >= s.size() || s[i] != ':') {
            return false;
        }
        ++i;
        ConsumeWs(s, i);

        JsonValue val;
        if (!ParseValue(s, i, val, limits, depth + 1)) {
            return false;
        }
        out.o->fields[key] = val;
        if (out.o->fields.size() > limits.maxFields) {
            return false;
        }

        ConsumeWs(s, i);
        if (i >= s.size()) {
            return false;
        }
        if (s[i] == ',') {
            ++i;
            continue;
        }
        if (s[i] == '}') {
            ++i;
            return true;
        }
        return false;
    }
    return false;
}

bool ParseArray(const std::string& s, size_t& i, JsonValue& out, const JsonLimits& limits, size_t depth) {
    if (depth > limits.maxDepth) {
        return false;
    }
    if (s[i] != '[') {
        return false;
    }
    ++i;
    ConsumeWs(s, i);
    out.type = JsonValue::Type::Array;
    out.a = std::make_shared<JsonArray>();

    if (i < s.size() && s[i] == ']') {
        ++i;
        return true;
    }

    while (i < s.size()) {
        JsonValue val;
        if (!ParseValue(s, i, val, limits, depth + 1)) {
            return false;
        }
        out.a->items.push_back(val);
        if (out.a->items.size() > limits.maxArraySize) {
            return false;
        }

        ConsumeWs(s, i);
        if (i >= s.size()) {
            return false;
        }
        if (s[i] == ',') {
            ++i;
            ConsumeWs(s, i);
            continue;
        }
        if (s[i] == ']') {
            ++i;
            return true;
        }
        return false;
    }
    return false;
}

bool ParseValue(const std::string& s, size_t& i, JsonValue& out, const JsonLimits& limits, size_t depth) {
    ConsumeWs(s, i);
    if (i >= s.size()) {
        return false;
    }

    if (s[i] == '"') {
        out.type = JsonValue::Type::String;
        return ParseString(s, i, out.s, limits.maxStringSize);
    }
    if (s[i] == '{') {
        return ParseObject(s, i, out, limits, depth);
    }
    if (s[i] == '[') {
        return ParseArray(s, i, out, limits, depth);
    }
    if (s.compare(i, 4, "true") == 0) {
        i += 4;
        out.type = JsonValue::Type::Bool;
        out.b = true;
        return true;
    }
    if (s.compare(i, 5, "false") == 0) {
        i += 5;
        out.type = JsonValue::Type::Bool;
        out.b = false;
        return true;
    }
    if (s.compare(i, 4, "null") == 0) {
        i += 4;
        out.type = JsonValue::Type::Null;
        return true;
    }

    out.type = JsonValue::Type::Number;
    return ParseNumber(s, i, out.n);
}

bool SerializeValue(const JsonValue& val, std::string& out);

bool SerializeObject(const JsonObject& obj, std::string& out) {
    out.push_back('{');
    bool first = true;
    for (const auto& kv : obj.fields) {
        if (!first) {
            out.push_back(',');
        }
        first = false;
        std::string escKey;
        if (!JsonEscape(kv.first, escKey)) {
            return false;
        }
        out.push_back('"');
        out += escKey;
        out += "\":";
        if (!SerializeValue(kv.second, out)) {
            return false;
        }
    }
    out.push_back('}');
    return true;
}

bool SerializeArray(const JsonArray& arr, std::string& out) {
    out.push_back('[');
    for (size_t i = 0; i < arr.items.size(); ++i) {
        if (i > 0) {
            out.push_back(',');
        }
        if (!SerializeValue(arr.items[i], out)) {
            return false;
        }
    }
    out.push_back(']');
    return true;
}

bool SerializeValue(const JsonValue& val, std::string& out) {
    switch (val.type) {
    case JsonValue::Type::Null:
        out += "null";
        return true;
    case JsonValue::Type::Bool:
        out += (val.b ? "true" : "false");
        return true;
    case JsonValue::Type::Number:
        out += std::to_string(val.n);
        return true;
    case JsonValue::Type::String: {
        std::string esc;
        if (!JsonEscape(val.s, esc)) {
            return false;
        }
        out.push_back('"');
        out += esc;
        out.push_back('"');
        return true;
    }
    case JsonValue::Type::Object:
        if (!val.o) {
            return false;
        }
        return SerializeObject(*val.o, out);
    case JsonValue::Type::Array:
        if (!val.a) {
            return false;
        }
        return SerializeArray(*val.a, out);
    default:
        return false;
    }
}

} // namespace

JsonValue MakeNull() {
    return JsonValue{};
}

JsonValue MakeBool(bool v) {
    JsonValue val;
    val.type = JsonValue::Type::Bool;
    val.b = v;
    return val;
}

JsonValue MakeNumber(int64_t v) {
    JsonValue val;
    val.type = JsonValue::Type::Number;
    val.n = v;
    return val;
}

JsonValue MakeString(const std::string& v) {
    JsonValue val;
    val.type = JsonValue::Type::String;
    val.s = v;
    return val;
}

JsonValue MakeObject() {
    JsonValue val;
    val.type = JsonValue::Type::Object;
    val.o = std::make_shared<JsonObject>();
    return val;
}

JsonValue MakeArray() {
    JsonValue val;
    val.type = JsonValue::Type::Array;
    val.a = std::make_shared<JsonArray>();
    return val;
}

bool JsonEscape(const std::string& input, std::string& output) {
    std::string out;
    out.reserve(input.size() + 8);
    for (char c : input) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                return false;
            }
            out.push_back(c);
        }
    }
    output.swap(out);
    return true;
}

bool JsonUnescape(const std::string& input, std::string& output) {
    std::string out;
    out.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        if (c != '\\') {
            out.push_back(c);
            continue;
        }
        if (i + 1 >= input.size()) {
            return false;
        }
        char esc = input[++i];
        switch (esc) {
        case '"': out.push_back('"'); break;
        case '\\': out.push_back('\\'); break;
        case '/': out.push_back('/'); break;
        case 'b': out.push_back('\b'); break;
        case 'f': out.push_back('\f'); break;
        case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break;
        case 't': out.push_back('\t'); break;
        default:
            return false;
        }
    }
    output.swap(out);
    return true;
}

bool ParseJson(const std::string& json, JsonValue& out, const JsonLimits& limits) {
    if (json.size() > limits.maxJsonSize) {
        return false;
    }
    size_t i = 0;
    if (!ParseValue(json, i, out, limits, 0)) {
        return false;
    }
    ConsumeWs(json, i);
    return i == json.size();
}

bool SerializeJson(const JsonValue& val, std::string& out) {
    out.clear();
    return SerializeValue(val, out);
}

bool GetString(const JsonObject& obj, const std::string& key, std::string& out) {
    auto it = obj.fields.find(key);
    if (it == obj.fields.end() || it->second.type != JsonValue::Type::String) {
        return false;
    }
    out = it->second.s;
    return true;
}

bool GetNumber(const JsonObject& obj, const std::string& key, int64_t& out) {
    auto it = obj.fields.find(key);
    if (it == obj.fields.end() || it->second.type != JsonValue::Type::Number) {
        return false;
    }
    out = it->second.n;
    return true;
}

bool GetBool(const JsonObject& obj, const std::string& key, bool& out) {
    auto it = obj.fields.find(key);
    if (it == obj.fields.end() || it->second.type != JsonValue::Type::Bool) {
        return false;
    }
    out = it->second.b;
    return true;
}

bool GetObject(const JsonObject& obj, const std::string& key, JsonObject& out) {
    auto it = obj.fields.find(key);
    if (it == obj.fields.end() || it->second.type != JsonValue::Type::Object || !it->second.o) {
        return false;
    }
    out = *it->second.o;
    return true;
}

bool GetArray(const JsonObject& obj, const std::string& key, JsonArray& out) {
    auto it = obj.fields.find(key);
    if (it == obj.fields.end() || it->second.type != JsonValue::Type::Array || !it->second.a) {
        return false;
    }
    out = *it->second.a;
    return true;
}

} // namespace protocol

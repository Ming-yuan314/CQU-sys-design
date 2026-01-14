#include "Base64.h"

#include <array>
#include <cctype>

namespace util {

namespace {

const char kAlphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::array<int, 256> BuildDecodeTable() {
    std::array<int, 256> table;
    table.fill(-1);
    for (int i = 0; i < 64; ++i) {
        table[static_cast<unsigned char>(kAlphabet[i])] = i;
    }
    return table;
}

} // namespace

std::string Base64Encode(const uint8_t* data, size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);

    size_t i = 0;
    while (i + 2 < len) {
        const uint32_t n = (static_cast<uint32_t>(data[i]) << 16) |
                           (static_cast<uint32_t>(data[i + 1]) << 8) |
                           static_cast<uint32_t>(data[i + 2]);
        out.push_back(kAlphabet[(n >> 18) & 0x3F]);
        out.push_back(kAlphabet[(n >> 12) & 0x3F]);
        out.push_back(kAlphabet[(n >> 6) & 0x3F]);
        out.push_back(kAlphabet[n & 0x3F]);
        i += 3;
    }

    const size_t rem = len - i;
    if (rem == 1) {
        const uint32_t n = static_cast<uint32_t>(data[i]) << 16;
        out.push_back(kAlphabet[(n >> 18) & 0x3F]);
        out.push_back(kAlphabet[(n >> 12) & 0x3F]);
        out.push_back('=');
        out.push_back('=');
    } else if (rem == 2) {
        const uint32_t n = (static_cast<uint32_t>(data[i]) << 16) |
                           (static_cast<uint32_t>(data[i + 1]) << 8);
        out.push_back(kAlphabet[(n >> 18) & 0x3F]);
        out.push_back(kAlphabet[(n >> 12) & 0x3F]);
        out.push_back(kAlphabet[(n >> 6) & 0x3F]);
        out.push_back('=');
    }

    return out;
}

std::string Base64Encode(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return std::string();
    }
    return Base64Encode(data.data(), data.size());
}

bool Base64Decode(const std::string& input, std::vector<uint8_t>& out) {
    static const std::array<int, 256> kDecode = BuildDecodeTable();

    std::vector<uint8_t> result;
    result.reserve((input.size() / 4) * 3);

    int vals[4] = {0, 0, 0, 0};
    int valCount = 0;
    int padCount = 0;

    for (char c : input) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            continue;
        }
        if (c == '=') {
            vals[valCount++] = 0;
            ++padCount;
        } else {
            const int v = kDecode[static_cast<unsigned char>(c)];
            if (v < 0) {
                return false;
            }
            vals[valCount++] = v;
        }

        if (valCount == 4) {
            const uint32_t n = (static_cast<uint32_t>(vals[0]) << 18) |
                               (static_cast<uint32_t>(vals[1]) << 12) |
                               (static_cast<uint32_t>(vals[2]) << 6) |
                               static_cast<uint32_t>(vals[3]);
            result.push_back(static_cast<uint8_t>((n >> 16) & 0xFF));
            if (padCount < 2) {
                result.push_back(static_cast<uint8_t>((n >> 8) & 0xFF));
            }
            if (padCount < 1) {
                result.push_back(static_cast<uint8_t>(n & 0xFF));
            }
            valCount = 0;
            padCount = 0;
        }
    }

    if (valCount != 0) {
        return false;
    }

    out.swap(result);
    return true;
}

} // namespace util

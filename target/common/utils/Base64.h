#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace util {

std::string Base64Encode(const uint8_t* data, size_t len);
std::string Base64Encode(const std::vector<uint8_t>& data);

bool Base64Decode(const std::string& input, std::vector<uint8_t>& out);

} // namespace util

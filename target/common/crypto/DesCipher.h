#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace crypto {

bool HexToBytes(const std::string& hex, std::vector<uint8_t>& out);
std::string BytesToHex(const std::vector<uint8_t>& bytes);

bool DesEncryptEcbPkcs7(const std::string& plain,
                        const std::vector<uint8_t>& key,
                        std::vector<uint8_t>& out,
                        std::string& err);

bool DesEncryptEcbPkcs7Hex(const std::string& plain,
                           const std::vector<uint8_t>& key,
                           std::string& outHex,
                           std::string& err);

} // namespace crypto

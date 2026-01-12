#pragma once

#include <cstdint>
#include <string>

#include <winsock2.h>

namespace net {

// Sends/receives until the requested length is fully transferred.
bool sendAll(SOCKET s, const void* data, size_t len);
bool recvAll(SOCKET s, void* data, size_t len);

// Length-prefixed frame helpers: [4-byte length][payload]
bool sendFrame(SOCKET s, const std::string& payload);
bool recvFrame(SOCKET s, std::string& payload);

} // namespace net

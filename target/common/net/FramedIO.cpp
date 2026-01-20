#include "FramedIO.h"

#include <algorithm>
#include <cstring>
#include <iostream>

#include <winsock2.h>

namespace net {

namespace {

const uint32_t kMaxFrameSize = 1024 * 1024; // 1 MB
const char kBanner[] = "PWNREMOTE/1.0 READY";

} // namespace

bool sendAll(SOCKET s, const void* data, size_t len) {
    const char* buf = static_cast<const char*>(data);
    size_t sent = 0;
    while (sent < len) {
        const int chunk = static_cast<int>(len - sent);
        const int rc = send(s, buf + sent, chunk, 0);
        if (rc == SOCKET_ERROR || rc == 0) {
            return false;
        }
        sent += static_cast<size_t>(rc);
    }
    return true;
}

bool recvAll(SOCKET s, void* data, size_t len) {
    char* buf = static_cast<char*>(data);
    size_t received = 0;
    while (received < len) {
        const int chunk = static_cast<int>(len - received);
        const int rc = recv(s, buf + received, chunk, 0);
        if (rc == SOCKET_ERROR || rc == 0) {
            return false;
        }
        received += static_cast<size_t>(rc);
    }
    return true;
}

bool sendFrame(SOCKET s, const std::string& payload) {
    const uint32_t len = static_cast<uint32_t>(payload.size());
    const uint32_t lenNet = htonl(len);
    if (!sendAll(s, &lenNet, sizeof(lenNet))) {
        return false;
    }
    if (len == 0) {
        return true;
    }
    return sendAll(s, payload.data(), payload.size());
}

bool recvFrame(SOCKET s, std::string& payload) {
    char peek[4];
    const int peeked = recv(s, peek, static_cast<int>(sizeof(peek)), MSG_PEEK);
    if (peeked > 0 && peek[0] == kBanner[0]) {
        const size_t bannerLen = sizeof(kBanner) - 1;
        if (peeked < static_cast<int>(sizeof(peek)) ||
            std::memcmp(peek, kBanner, sizeof(peek)) == 0) {
            std::string banner(bannerLen, '\0');
            if (!recvAll(s, banner.data(), banner.size())) {
                return false;
            }
        }
    }

    uint32_t lenNet = 0;
    if (!recvAll(s, &lenNet, sizeof(lenNet))) {
        return false;
    }
    const uint32_t len = ntohl(lenNet);
    if (len > kMaxFrameSize) {
        std::cerr << "recvFrame rejected oversized frame: " << len << " bytes\n";
        return false;
    }

    if (len == 0) {
        payload.clear();
        return true;
    }

    std::string buffer(len, '\0');
    if (!recvAll(s, buffer.data(), buffer.size())) {
        return false;
    }
    payload.swap(buffer);
    return true;
}

} // namespace net

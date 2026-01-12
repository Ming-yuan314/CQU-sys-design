#include "SocketInit.h"

#include <iostream>

namespace net {

SocketInit::SocketInit() : ok_(false) {
    WSADATA wsaData;
    const int rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (rc == 0) {
        ok_ = true;
        return;
    }

    const int lastErr = WSAGetLastError();
    std::cerr << "WSAStartup failed, rc=" << rc << ", WSAGetLastError=" << lastErr << "\n";
}

SocketInit::~SocketInit() {
    if (ok_) {
        WSACleanup();
    }
}

bool SocketInit::ok() const {
    return ok_;
}

} // namespace net

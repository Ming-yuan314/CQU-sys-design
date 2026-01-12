#pragma once

#include <winsock2.h>

namespace net {

class SocketInit {
public:
    SocketInit();
    ~SocketInit();

    SocketInit(const SocketInit&) = delete;
    SocketInit& operator=(const SocketInit&) = delete;

    bool ok() const;

private:
    bool ok_;
};

} // namespace net

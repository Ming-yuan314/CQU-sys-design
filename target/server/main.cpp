#include <atomic>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "../common/dummy.h"
#include "../common/net/FramedIO.h"
#include "../common/net/SocketInit.h"
#include "../common/protocol/Message.h"
#include "core/CommandRouter.h"
#include "core/ServerConfig.h"
#include "handlers/AuthHandlers.h"
#include "handlers/AdminHandlers.h"
#include "handlers/BasicHandlers.h"
#include "handlers/FileHandlers.h"

#include <cstdio>

namespace {

constexpr char kBanner[] = "PWNREMOTE/1.0 READY";

std::string AddrToString(const sockaddr_in& addr) {
    char ipBuf[INET_ADDRSTRLEN]{};
    InetNtopA(AF_INET, const_cast<in_addr*>(&addr.sin_addr), ipBuf, sizeof(ipBuf));
    const unsigned short port = ntohs(addr.sin_port);
    std::ostringstream oss;
    oss << ipBuf << ":" << port;
    return oss.str();
}

void CleanupSession(server::Session& session) {
    auto& up = session.upload();
    if (up.inProgress && !up.tempPath.empty()) {
        std::remove(up.tempPath.c_str());
    }
    up.reset();
    session.download().reset();
}

} // namespace

int main() {
    std::cout << "server start\n";
    std::cout << common::build_info() << "\n";

    net::SocketInit sockInit;
    if (!sockInit.ok()) {
        std::cerr << "SocketInit failed\n";
        return 1;
    }

    server::ServerConfig config;
    std::string configErr;
    const std::string configPaths[] = {
        "server/server_config.json",
        "../server/server_config.json",
        "server_config.json"
    };
    bool loaded = false;
    bool invalid = false;
    for (const auto& path : configPaths) {
        const server::ConfigLoadResult result = server::LoadServerConfig(path, config, configErr);
        if (result == server::ConfigLoadResult::Ok) {
            std::cout << "Config loaded from " << path << "\n";
            loaded = true;
            break;
        }
        if (result == server::ConfigLoadResult::Invalid) {
            std::cerr << "Config invalid: " << configErr << "\n";
            invalid = true;
            break;
        }
    }
    if (invalid) {
        return 1;
    }
    if (!loaded) {
        std::cerr << "Config not found, using defaults\n";
    }

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        std::cerr << "socket() failed\n";
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    const unsigned short kServerPort = 9000;
    addr.sin_port = htons(kServerPort);
    if (InetPtonA(AF_INET, config.bindIp.c_str(), &addr.sin_addr) != 1) {
        std::cerr << "Invalid bind_ip: " << config.bindIp << "\n";
        closesocket(listenSock);
        return 1;
    }

    if (bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        const int err = WSAGetLastError();
        std::cerr << "bind() failed for " << config.bindIp;
        if (err == WSAEADDRINUSE) {
            std::cerr << " (port is unavailable)";
        }
        std::cerr << " (error=" << err << ")\n";
        closesocket(listenSock);
        return 1;
    }

    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen() failed\n";
        closesocket(listenSock);
        return 1;
    }

    std::cout << "listening on " << config.bindIp << "\n";

    std::string storageErr;
    if (!server::EnsureStorageDir(config.storageDir, storageErr)) {
        std::cerr << "Storage dir error: " << storageErr << "\n";
        return 1;
    }

    server::CommandRouter router;
    server::RegisterAuthHandlers(router, config);
    server::RegisterBasicHandlers(router);
    server::RegisterAdminHandlers(router);
    server::RegisterFileHandlers(router, config);

    std::atomic<uint64_t> nextConnId{1};

    while (true) {
        sockaddr_in clientAddr{};
        int clientLen = sizeof(clientAddr);
        SOCKET clientSock = accept(listenSock, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if (clientSock == INVALID_SOCKET) {
            std::cerr << "accept() failed\n";
            continue;
        }

        const int bannerLen = static_cast<int>(sizeof(kBanner) - 1);
        send(clientSock, kBanner, bannerLen, 0);

        const uint64_t connId = nextConnId.fetch_add(1);
        const std::string peer = AddrToString(clientAddr);
        std::cout << "client connected id=" << connId << " from " << peer << "\n";

        std::thread([clientSock, clientAddr, connId, peer, &router]() {
            server::Session session;
            std::string reason = "closed";

            while (true) {
                std::string reqJson;
                if (!net::recvFrame(clientSock, reqJson)) {
                    reason = "closed";
                    break;
                }

                protocol::RequestMessage req;
                protocol::ErrorCode parseErr = protocol::ErrorCode::Ok;
                if (protocol::DecodeRequest(reqJson, req, parseErr)) {
                    std::cout << "recv request id=" << connId
                              << " cmd=" << req.cmd
                              << " level=" << session.levelString()
                              << "\n";
                } else {
                    std::cout << "recv request id=" << connId << " cmd=INVALID\n";
                }

                std::string respJson;
                if (!router.Handle(session, reqJson, respJson)) {
                    reason = "error";
                    break;
                }

                if (!net::sendFrame(clientSock, respJson)) {
                    reason = "error";
                    break;
                }
            }

            CleanupSession(session);
            closesocket(clientSock);
            std::cout << "client disconnected id=" << connId
                      << " reason=" << reason << "\n";
        }).detach();
    }

    closesocket(listenSock);
    return 0;
}

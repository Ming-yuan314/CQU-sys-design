#include <iostream>
#include <string>

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
    addr.sin_port = htons(config.port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "bind() failed\n";
        closesocket(listenSock);
        return 1;
    }

    if (listen(listenSock, 1) == SOCKET_ERROR) {
        std::cerr << "listen() failed\n";
        closesocket(listenSock);
        return 1;
    }

    std::cout << "listening on port " << config.port << "\n";

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

    bool serverRunning = true;
    while (serverRunning) {
        sockaddr_in clientAddr{};
        int clientLen = sizeof(clientAddr);
        SOCKET clientSock = accept(listenSock, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if (clientSock == INVALID_SOCKET) {
            std::cerr << "accept() failed\n";
            continue;
        }

        std::cout << "client connected\n";

        server::Session session;

        bool clientRunning = true;
        while (clientRunning) {
            std::string reqJson;
            if (!net::recvFrame(clientSock, reqJson)) {
                std::cerr << "recvFrame failed or client closed connection\n";
                break;
            }

            std::cout << "recv request: " << reqJson << "\n";

            std::string respJson;
            if (!router.Handle(session, reqJson, respJson)) {
                std::cerr << "router handle failed\n";
                break;
            }

            if (!net::sendFrame(clientSock, respJson)) {
                std::cerr << "sendFrame failed\n";
                break;
            }
        }

        closesocket(clientSock);
        std::cout << "client disconnected\n";
    }

    closesocket(listenSock);
    return 0;
}

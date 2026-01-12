#include <iostream>
#include <string>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "../common/dummy.h"
#include "../common/net/FramedIO.h"
#include "../common/net/SocketInit.h"

int main() {
    std::cout << "server start\n";
    std::cout << common::build_info() << "\n";

    net::SocketInit sockInit;
    if (!sockInit.ok()) {
        std::cerr << "SocketInit failed\n";
        return 1;
    }

    const unsigned short kListenPort = 9000;

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        std::cerr << "socket() failed\n";
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kListenPort);
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

    std::cout << "listening on port " << kListenPort << "\n";

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

        bool clientRunning = true;
        while (clientRunning) {
            std::string payload;
            if (!net::recvFrame(clientSock, payload)) {
                std::cerr << "recvFrame failed or client closed connection\n";
                break;
            }

            std::cout << "recv from client: " << payload << "\n";

            std::string reply = "ACK: " + payload;
            if (!net::sendFrame(clientSock, reply)) {
                std::cerr << "sendFrame failed\n";
                break;
            }

            if (payload == "exit") {
                std::cout << "Exit command received, shutting down server\n";
                clientRunning = false;
                serverRunning = false;
            }
        }

        closesocket(clientSock);
        std::cout << "client disconnected\n";
    }

    closesocket(listenSock);
    return 0;
}

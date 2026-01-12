#include <iostream>
#include <string>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "../common/dummy.h"
#include "../common/net/FramedIO.h"
#include "../common/net/SocketInit.h"

int main() {
    std::cout << "client start\n";
    std::cout << common::build_info() << "\n";

    net::SocketInit sockInit;
    if (!sockInit.ok()) {
        std::cerr << "SocketInit failed\n";
        return 1;
    }

    const char* kServerIp = "127.0.0.1";
    const unsigned short kServerPort = 9000;

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        std::cerr << "socket() failed\n";
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kServerPort);
    if (InetPtonA(AF_INET, kServerIp, &addr.sin_addr) != 1) {
        std::cerr << "InetPtonA failed\n";
        closesocket(s);
        return 1;
    }

    if (connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "connect() failed\n";
        closesocket(s);
        return 1;
    }

    const std::string kExitToken = "exit";

    while (true) {
        std::cout << "Enter a message (type '" << kExitToken << "' to quit): ";
        std::string line;
        if (!std::getline(std::cin, line)) {
            std::cerr << "Failed to read from stdin\n";
            break;
        }

        if (!net::sendFrame(s, line)) {
            std::cerr << "sendFrame failed\n";
            break;
        }

        std::string reply;
        if (!net::recvFrame(s, reply)) {
            std::cerr << "recvFrame failed\n";
            break;
        }

        std::cout << "Server ACK: " << reply << "\n";

        if (reply == "ACK: " + kExitToken) {
            std::cout << "Exit token received, client quitting.\n";
            break;
        }
    }

    closesocket(s);
    return 0;
}

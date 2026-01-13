#include <iostream>
#include <string>
#include <sstream>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "../common/dummy.h"
#include "../common/net/FramedIO.h"
#include "../common/net/SocketInit.h"
#include "../common/protocol/Message.h"
#include "../common/protocol/JsonLite.h"

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

        if (line == kExitToken) {
            std::cout << "Exit command received locally, closing client.\n";
            break;
        }

        auto trim = [](std::string& s) {
            const char* ws = " \t\r\n";
            const std::string::size_type start = s.find_first_not_of(ws);
            if (start == std::string::npos) {
                s.clear();
                return;
            }
            const std::string::size_type end = s.find_last_not_of(ws);
            s = s.substr(start, end - start + 1);
        };

        auto toUpper = [](std::string s) {
            for (char& c : s) {
                if (c >= 'a' && c <= 'z') {
                    c = static_cast<char>(c - 'a' + 'A');
                }
            }
            return s;
        };

        trim(line);

        protocol::RequestMessage req;
        if (!line.empty()) {
            const std::string::size_type spacePos = line.find(' ');
            std::string cmd = (spacePos == std::string::npos) ? line : line.substr(0, spacePos);
            std::string rest = (spacePos == std::string::npos) ? "" : line.substr(spacePos + 1);
            trim(cmd);
            trim(rest);

            const std::string cmdUpper = toUpper(cmd);
            req.cmd = cmdUpper;

            if (cmdUpper == "LOGIN_LOW") {
                std::istringstream iss(rest);
                std::string user;
                std::string pass;
                if (!(iss >> user >> pass)) {
                    std::cout << "Usage: login_low <user> <pass>\n";
                    continue;
                }
                req.args.fields["username"] = protocol::MakeString(user);
                req.args.fields["password"] = protocol::MakeString(pass);
            } else if (cmdUpper == "ECHO") {
                if (rest.empty()) {
                    std::cout << "Usage: ECHO <text>\n";
                    continue;
                }
                req.args.fields["text"] = protocol::MakeString(rest);
            }
        }
        if (req.cmd.empty()) {
            std::cout << "Empty command, please retry.\n";
            continue;
        }

        std::string reqJson;
        if (!protocol::EncodeRequest(req, reqJson)) {
            std::cerr << "EncodeRequest failed\n";
            break;
        }

        if (!net::sendFrame(s, reqJson)) {
            std::cerr << "sendFrame failed\n";
            break;
        }

        std::string respJson;
        if (!net::recvFrame(s, respJson)) {
            std::cerr << "recvFrame failed\n";
            break;
        }

        protocol::ResponseMessage resp;
        protocol::ErrorCode parseErr = protocol::ErrorCode::Ok;
        if (!protocol::DecodeResponse(respJson, resp, parseErr)) {
            std::cerr << "DecodeResponse failed\n";
            break;
        }

        std::cout << "Response: " << resp.msg << " (ok=" << (resp.ok ? "true" : "false")
                  << ", code=" << protocol::ErrorCodeToInt(resp.code) << ")\n";

        if (!resp.ok) {
            if (resp.code == protocol::ErrorCode::NotLogin) {
                std::cout << "Hint: not logged in, run login_low <user> <pass>\n";
            } else if (resp.code == protocol::ErrorCode::NoPermission) {
                std::cout << "Hint: permission denied (HIGH required)\n";
            }
        }

        if (!resp.data.fields.empty()) {
            protocol::JsonValue dataVal = protocol::MakeObject();
            if (dataVal.o) {
                dataVal.o->fields = resp.data.fields;
            }
            std::string dataJson;
            if (protocol::SerializeJson(dataVal, dataJson)) {
                std::cout << "Data: " << dataJson << "\n";
            }
        }
    }

    closesocket(s);
    return 0;
}

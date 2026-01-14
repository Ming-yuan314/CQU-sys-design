#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "../common/dummy.h"
#include "../common/net/FramedIO.h"
#include "../common/net/SocketInit.h"
#include "../common/protocol/Message.h"
#include "../common/protocol/JsonLite.h"
#include "../common/crypto/DesCipher.h"

namespace {

struct ClientConfig {
    std::string desKeyHex = "0123456789ABCDEF";
    std::vector<uint8_t> desKeyBytes;
};

bool LoadClientConfigFile(const std::string& path,
                          ClientConfig& out,
                          std::string& err,
                          bool& found) {
    found = false;
    std::ifstream fin(path);
    if (!fin.is_open()) {
        return false;
    }
    found = true;

    std::ostringstream oss;
    oss << fin.rdbuf();
    const std::string content = oss.str();

    protocol::JsonValue root;
    protocol::JsonLimits limits;
    if (!protocol::ParseJson(content, root, limits)) {
        err = "failed to parse config json";
        return false;
    }
    if (root.type != protocol::JsonValue::Type::Object || !root.o) {
        err = "config root is not object";
        return false;
    }

    protocol::JsonObject obj = *root.o;
    std::string desKeyHex;
    if (!protocol::GetString(obj, "des_key_hex", desKeyHex)) {
        err = "invalid or missing field: des_key_hex";
        return false;
    }

    std::vector<uint8_t> keyBytes;
    if (!crypto::HexToBytes(desKeyHex, keyBytes) || keyBytes.size() != 8) {
        err = "invalid des_key_hex (need 16 hex chars)";
        return false;
    }

    out.desKeyHex = crypto::BytesToHex(keyBytes);
    out.desKeyBytes = keyBytes;
    return true;
}

} // namespace

int main() {
    std::cout << "client start\n";
    std::cout << common::build_info() << "\n";

    ClientConfig config;
    std::string configErr;
    const std::string configPaths[] = {
        "client/client_config.json",
        "../client/client_config.json",
        "client_config.json",
        "target/client/client_config.json"
    };
    bool loaded = false;
    bool invalid = false;
    for (const auto& path : configPaths) {
        bool found = false;
        if (LoadClientConfigFile(path, config, configErr, found)) {
            std::cout << "Client config loaded from " << path << "\n";
            loaded = true;
            break;
        }
        if (found) {
            std::cerr << "Client config invalid: " << configErr << "\n";
            invalid = true;
            break;
        }
    }
    if (invalid) {
        return 1;
    }
    if (!loaded) {
        std::vector<uint8_t> keyBytes;
        crypto::HexToBytes(config.desKeyHex, keyBytes);
        config.desKeyBytes = keyBytes;
        std::cout << "Client config not found, using default DES key\n";
    }

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
            } else if (cmdUpper == "LOGIN_HIGH") {
                std::istringstream iss(rest);
                std::string user;
                std::string passPlain;
                if (!(iss >> user >> passPlain)) {
                    std::cout << "Usage: login_high <user> <pass_plain>\n";
                    continue;
                }
                if (config.desKeyBytes.size() != 8) {
                    std::cout << "DES key not configured\n";
                    continue;
                }
                std::string cipherHex;
                std::string err;
                if (!crypto::DesEncryptEcbPkcs7Hex(passPlain, config.desKeyBytes, cipherHex, err)) {
                    std::cout << "Encrypt failed: " << err << "\n";
                    continue;
                }
                req.args.fields["username"] = protocol::MakeString(user);
                req.args.fields["password_cipher_hex"] = protocol::MakeString(cipherHex);
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

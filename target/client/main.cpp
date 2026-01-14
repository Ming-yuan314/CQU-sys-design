#include <cstdint>
#include <filesystem>
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
#include "../common/utils/Base64.h"

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

std::string BaseName(const std::string& path) {
    const std::string::size_type pos = path.find_last_of("/\\");
    if (pos == std::string::npos) {
        return path;
    }
    return path.substr(pos + 1);
}

bool SendRequest(SOCKET s,
                 const protocol::RequestMessage& req,
                 protocol::ResponseMessage& resp,
                 std::string& err) {
    std::string reqJson;
    if (!protocol::EncodeRequest(req, reqJson)) {
        err = "EncodeRequest failed";
        return false;
    }
    if (!net::sendFrame(s, reqJson)) {
        err = "sendFrame failed";
        return false;
    }
    std::string respJson;
    if (!net::recvFrame(s, respJson)) {
        err = "recvFrame failed";
        return false;
    }
    protocol::ErrorCode parseErr = protocol::ErrorCode::Ok;
    if (!protocol::DecodeResponse(respJson, resp, parseErr)) {
        err = "DecodeResponse failed";
        return false;
    }
    return true;
}

void RenderResponseData(const std::string& cmdUpper, const protocol::ResponseMessage& resp) {
    if (!resp.ok) {
        return;
    }

    if (cmdUpper == "HELP") {
        return;
    }

    if (cmdUpper == "TIME") {
        std::string time;
        if (protocol::GetString(resp.data, "time", time)) {
            std::cout << "Server time: " << time << "\n";
        }
        return;
    }

    if (cmdUpper == "WHOAMI") {
        std::string level;
        std::string username;
        protocol::GetString(resp.data, "level", level);
        protocol::GetString(resp.data, "username", username);
        if (!level.empty()) {
            std::cout << "Level: " << level << "\n";
        }
        if (!username.empty()) {
            std::cout << "User: " << username << "\n";
        }
        return;
    }

    if (cmdUpper == "ECHO") {
        std::string echo;
        if (protocol::GetString(resp.data, "echo", echo)) {
            std::cout << "Echo: " << echo << "\n";
        }
        return;
    }

    if (cmdUpper == "LOGIN_LOW" || cmdUpper == "LOGIN_HIGH" || cmdUpper == "LOGOUT") {
        std::string level;
        std::string username;
        protocol::GetString(resp.data, "level", level);
        protocol::GetString(resp.data, "username", username);
        if (cmdUpper == "LOGOUT") {
            if (!level.empty() && !username.empty()) {
                std::cout << "Now at " << level << " as " << username << "\n";
            } else if (!level.empty()) {
                std::cout << "Now at " << level << "\n";
            }
        } else {
            if (!level.empty() && !username.empty()) {
                std::cout << "Logged in as " << username << " (" << level << ")\n";
            } else if (!level.empty()) {
                std::cout << "Level: " << level << "\n";
            }
        }
        return;
    }

    std::string filename;
    int64_t size = 0;
    if (protocol::GetString(resp.data, "filename", filename) &&
        protocol::GetNumber(resp.data, "size", size)) {
        std::cout << "File: " << filename << " (" << size << " bytes)\n";
    }
}

bool HandleUpload(SOCKET s, const std::string& argsLine) {
    std::istringstream iss(argsLine);
    std::string localPath;
    std::string remoteName;
    if (!(iss >> localPath)) {
        std::cout << "Usage: upload <local_path> [remote_name]\n";
        return true;
    }
    if (!(iss >> remoteName)) {
        remoteName = BaseName(localPath);
    }
    if (remoteName.empty()) {
        std::cout << "Invalid remote_name\n";
        return true;
    }

    std::ifstream fin(localPath, std::ios::binary | std::ios::ate);
    if (!fin.is_open()) {
        std::cout << "Failed to open file: " << localPath << "\n";
        return true;
    }
    const std::streamoff size = fin.tellg();
    if (size <= 0) {
        std::cout << "Invalid file size\n";
        return true;
    }
    fin.seekg(0, std::ios::beg);

    const uint32_t defaultChunk = 64 * 1024;
    protocol::RequestMessage initReq;
    initReq.cmd = "UPLOAD_INIT";
    initReq.args.fields["filename"] = protocol::MakeString(remoteName);
    initReq.args.fields["file_size"] = protocol::MakeNumber(static_cast<int64_t>(size));
    initReq.args.fields["chunk_size"] = protocol::MakeNumber(defaultChunk);

    protocol::ResponseMessage initResp;
    std::string err;
    if (!SendRequest(s, initReq, initResp, err)) {
        std::cout << "Upload init failed: " << err << "\n";
        return false;
    }
    std::cout << "Upload init: " << initResp.msg << " (ok=" << (initResp.ok ? "true" : "false")
              << ", code=" << protocol::ErrorCodeToInt(initResp.code) << ")\n";
    if (!initResp.ok) {
        return true;
    }

    std::string uploadId;
    int64_t chunkSize = 0;
    int64_t nextIndex = 0;
    if (!protocol::GetString(initResp.data, "upload_id", uploadId) ||
        !protocol::GetNumber(initResp.data, "chunk_size", chunkSize) ||
        !protocol::GetNumber(initResp.data, "next_index", nextIndex)) {
        std::cout << "Upload init response missing fields\n";
        return false;
    }
    if (chunkSize <= 0) {
        std::cout << "Invalid chunk_size from server\n";
        return false;
    }
    std::cout << "Upload session: id=" << uploadId << " | chunk=" << chunkSize << " bytes\n";

    std::vector<uint8_t> buffer(static_cast<size_t>(chunkSize));
    uint32_t index = static_cast<uint32_t>(nextIndex);
    while (fin) {
        fin.read(reinterpret_cast<char*>(buffer.data()),
                 static_cast<std::streamsize>(buffer.size()));
        const std::streamsize got = fin.gcount();
        if (got <= 0) {
            break;
        }

        const std::string b64 = util::Base64Encode(buffer.data(), static_cast<size_t>(got));
        protocol::RequestMessage chunkReq;
        chunkReq.cmd = "UPLOAD_CHUNK";
        chunkReq.args.fields["upload_id"] = protocol::MakeString(uploadId);
        chunkReq.args.fields["chunk_index"] = protocol::MakeNumber(static_cast<int64_t>(index));
        chunkReq.args.fields["data_b64"] = protocol::MakeString(b64);

        protocol::ResponseMessage chunkResp;
        if (!SendRequest(s, chunkReq, chunkResp, err)) {
            std::cout << "Upload chunk failed: " << err << "\n";
            return false;
        }
        if (!chunkResp.ok) {
            std::cout << "Upload chunk rejected: " << chunkResp.msg
                      << " (code=" << protocol::ErrorCodeToInt(chunkResp.code) << ")\n";
            return true;
        }
        ++index;
    }

    protocol::RequestMessage finishReq;
    finishReq.cmd = "UPLOAD_FINISH";
    finishReq.args.fields["upload_id"] = protocol::MakeString(uploadId);

    protocol::ResponseMessage finishResp;
    if (!SendRequest(s, finishReq, finishResp, err)) {
        std::cout << "Upload finish failed: " << err << "\n";
        return false;
    }
    std::cout << "Upload finish: " << finishResp.msg << " (ok=" << (finishResp.ok ? "true" : "false")
              << ", code=" << protocol::ErrorCodeToInt(finishResp.code) << ")\n";
    if (finishResp.ok) {
        std::string filename;
        int64_t size = 0;
        if (protocol::GetString(finishResp.data, "filename", filename) &&
            protocol::GetNumber(finishResp.data, "size", size)) {
            std::cout << "Uploaded: " << filename << " (" << size << " bytes)\n";
        }
    }
    return true;
}

bool HandleCheck(SOCKET s) {
    protocol::RequestMessage req;
    req.cmd = "LIST_FILES";
    protocol::ResponseMessage resp;
    std::string err;
    if (!SendRequest(s, req, resp, err)) {
        std::cout << "Check failed: " << err << "\n";
        return false;
    }
    std::cout << "Check: " << resp.msg << " (ok=" << (resp.ok ? "true" : "false")
              << ", code=" << protocol::ErrorCodeToInt(resp.code) << ")\n";
    if (!resp.ok) {
        return true;
    }

    protocol::JsonArray files;
    if (!protocol::GetArray(resp.data, "files", files)) {
        std::cout << "No files field\n";
        return true;
    }

    std::cout << "Files:\n";
    if (files.items.empty()) {
        std::cout << "  (empty)\n";
        return true;
    }
    for (const auto& v : files.items) {
        if (v.type == protocol::JsonValue::Type::String) {
            std::cout << "  " << v.s << "\n";
        }
    }
    return true;
}

bool HandleDownload(SOCKET s, const std::string& argsLine) {
    std::istringstream iss(argsLine);
    std::string remoteName;
    std::string localPath;
    if (!(iss >> remoteName >> localPath)) {
        std::cout << "Usage: download <remote_name> <local_path>\n";
        return true;
    }

    std::error_code ec;
    if (localPath.back() == '\\' || localPath.back() == '/') {
        std::cout << "Usage: download <remote_name> <local_path>\n";
        return true;
    }
    if (std::filesystem::exists(localPath, ec) &&
        std::filesystem::is_directory(localPath, ec)) {
        std::cout << "Usage: download <remote_name> <local_path>\n";
        return true;
    }
    const std::filesystem::path localP(localPath);
    if (localP.has_parent_path() &&
        (!std::filesystem::exists(localP.parent_path(), ec) ||
         !std::filesystem::is_directory(localP.parent_path(), ec))) {
        std::cout << "Usage: download <remote_name> <local_path>\n";
        return true;
    }

    std::ofstream fout(localPath, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!fout.is_open()) {
        std::cout << "Failed to open local file: " << localPath << "\n";
        return true;
    }

    const uint32_t requestChunk = 64 * 1024;
    protocol::RequestMessage initReq;
    initReq.cmd = "DOWNLOAD_INIT";
    initReq.args.fields["filename"] = protocol::MakeString(remoteName);
    initReq.args.fields["chunk_size"] = protocol::MakeNumber(requestChunk);

    protocol::ResponseMessage initResp;
    std::string err;
    if (!SendRequest(s, initReq, initResp, err)) {
        std::cout << "Download init failed: " << err << "\n";
        fout.close();
        std::filesystem::remove(localPath, ec);
        return false;
    }
    std::cout << "Download init: " << initResp.msg << " (ok=" << (initResp.ok ? "true" : "false")
              << ", code=" << protocol::ErrorCodeToInt(initResp.code) << ")\n";
    if (!initResp.ok) {
        fout.close();
        std::filesystem::remove(localPath, ec);
        return true;
    }

    std::string downloadId;
    int64_t chunkSize = 0;
    int64_t nextIndex = 0;
    int64_t fileSize = 0;
    if (!protocol::GetString(initResp.data, "download_id", downloadId) ||
        !protocol::GetNumber(initResp.data, "chunk_size", chunkSize) ||
        !protocol::GetNumber(initResp.data, "next_index", nextIndex) ||
        !protocol::GetNumber(initResp.data, "file_size", fileSize)) {
        std::cout << "Download init response missing fields\n";
        return false;
    }
    if (chunkSize <= 0) {
        std::cout << "Invalid chunk_size from server\n";
        protocol::RequestMessage abortReq;
        abortReq.cmd = "DOWNLOAD_ABORT";
        protocol::ResponseMessage abortResp;
        SendRequest(s, abortReq, abortResp, err);
        fout.close();
        std::filesystem::remove(localPath, ec);
        return false;
    }
    std::cout << "Download file size: " << fileSize << " bytes\n";

    uint32_t index = static_cast<uint32_t>(nextIndex);
    while (true) {
        protocol::RequestMessage chunkReq;
        chunkReq.cmd = "DOWNLOAD_CHUNK";
        chunkReq.args.fields["download_id"] = protocol::MakeString(downloadId);
        chunkReq.args.fields["chunk_index"] = protocol::MakeNumber(static_cast<int64_t>(index));

        protocol::ResponseMessage chunkResp;
        if (!SendRequest(s, chunkReq, chunkResp, err)) {
            std::cout << "Download chunk failed: " << err << "\n";
            fout.close();
            std::filesystem::remove(localPath, ec);
            protocol::RequestMessage abortReq;
            abortReq.cmd = "DOWNLOAD_ABORT";
            protocol::ResponseMessage abortResp;
            SendRequest(s, abortReq, abortResp, err);
            return false;
        }
        if (!chunkResp.ok) {
            std::cout << "Download chunk rejected: " << chunkResp.msg
                      << " (code=" << protocol::ErrorCodeToInt(chunkResp.code) << ")\n";
            fout.close();
            std::filesystem::remove(localPath, ec);
            protocol::RequestMessage abortReq;
            abortReq.cmd = "DOWNLOAD_ABORT";
            protocol::ResponseMessage abortResp;
            SendRequest(s, abortReq, abortResp, err);
            return true;
        }

        std::string dataB64;
        bool isLast = false;
        if (!protocol::GetString(chunkResp.data, "data_b64", dataB64) ||
            !protocol::GetBool(chunkResp.data, "is_last", isLast)) {
            std::cout << "Download chunk missing fields\n";
            fout.close();
            std::filesystem::remove(localPath, ec);
            protocol::RequestMessage abortReq;
            abortReq.cmd = "DOWNLOAD_ABORT";
            protocol::ResponseMessage abortResp;
            SendRequest(s, abortReq, abortResp, err);
            return false;
        }

        std::vector<uint8_t> bytes;
        if (!util::Base64Decode(dataB64, bytes)) {
            std::cout << "Invalid base64 in chunk\n";
            fout.close();
            std::filesystem::remove(localPath, ec);
            protocol::RequestMessage abortReq;
            abortReq.cmd = "DOWNLOAD_ABORT";
            protocol::ResponseMessage abortResp;
            SendRequest(s, abortReq, abortResp, err);
            return false;
        }

        if (!bytes.empty()) {
            fout.write(reinterpret_cast<const char*>(bytes.data()),
                       static_cast<std::streamsize>(bytes.size()));
            if (!fout) {
                std::cout << "Write local file failed\n";
                fout.close();
                std::filesystem::remove(localPath, ec);
                protocol::RequestMessage abortReq;
                abortReq.cmd = "DOWNLOAD_ABORT";
                protocol::ResponseMessage abortResp;
                SendRequest(s, abortReq, abortResp, err);
                return false;
            }
        }

        if (isLast) {
            break;
        }
        ++index;
    }

    std::cout << "Download finished: " << localPath << "\n";
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
    std::cout << "Type 'help' for commands, '" << kExitToken << "' to quit.\n";

    while (true) {
        std::cout << "cmd> ";
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

            if (cmdUpper == "UPLOAD") {
                if (!HandleUpload(s, rest)) {
                    break;
                }
                continue;
            }

            if (cmdUpper == "CHECK") {
                if (!HandleCheck(s)) {
                    break;
                }
                continue;
            }

            if (cmdUpper == "DOWNLOAD") {
                if (!HandleDownload(s, rest)) {
                    break;
                }
                continue;
            }

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

        protocol::ResponseMessage resp;
        std::string err;
        if (!SendRequest(s, req, resp, err)) {
            std::cerr << err << "\n";
            break;
        }

        std::cout << "Response: " << resp.msg
                  << " | ok=" << (resp.ok ? "true" : "false")
                  << " | code=" << protocol::ErrorCodeToInt(resp.code) << "\n";

        protocol::JsonArray commands;
        if (protocol::GetArray(resp.data, "commands", commands)) {
            std::cout << "Commands:\n";
            for (const auto& item : commands.items) {
                if (item.type == protocol::JsonValue::Type::String) {
                    std::cout << "  " << item.s << "\n";
                } else if (item.type == protocol::JsonValue::Type::Object && item.o) {
                    std::string name;
                    std::string desc;
                    protocol::GetString(*item.o, "name", name);
                    protocol::GetString(*item.o, "desc", desc);
                    if (!name.empty() && !desc.empty()) {
                        std::cout << "  " << name << " - " << desc << "\n";
                    } else if (!name.empty()) {
                        std::cout << "  " << name << "\n";
                    }
                }
            }
        }

        if (!resp.ok) {
            if (resp.code == protocol::ErrorCode::NotLogin) {
                std::cout << "Hint: not logged in, run login_low <user> <pass>\n";
            } else if (resp.code == protocol::ErrorCode::NoPermission) {
                std::cout << "Hint: permission denied (HIGH required)\n";
            }
        }

        RenderResponseData(req.cmd, resp);

    }

    closesocket(s);
    return 0;
}

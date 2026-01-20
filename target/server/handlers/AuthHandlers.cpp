#include "AuthHandlers.h"

#include <iostream>
#include<cstdio>
#include <cstring>

#include "../../common/protocol/JsonLite.h"
#include "../../common/crypto/DesCipher.h"

namespace server {

namespace {

void SetString(protocol::JsonObject& obj, const std::string& key, const std::string& value) {
    obj.fields[key] = protocol::MakeString(value);
}

int AdminLogin(const std::string& cipherHex,
               const std::string& expectedHex,
               std::string& err) {
#if defined(VULN_DEMO)
    struct LocalState {
        char cipherBuf[64];
        volatile int admin;
    };
    LocalState local{};
    local.admin = 0;
    // Intentional overflow for the demo build.
    std::strcpy(local.cipherBuf, cipherHex.c_str());
#else
    int admin = 0;
    char cipherBuf[64];
    if (cipherHex.size() >= sizeof(cipherBuf)) {
        err = "password_cipher_hex too long";
        return 0;
    }
    std::memset(cipherBuf, 0, sizeof(cipherBuf));
    std::memcpy(cipherBuf, cipherHex.data(), cipherHex.size());
#endif

    std::vector<uint8_t> cipherBytes;
    if (!crypto::HexToBytes(cipherHex, cipherBytes)) {
        err = "invalid password_cipher_hex";
#if defined(VULN_DEMO)
        return static_cast<int>(local.admin);
#else
        return admin;
#endif
    }

    if (crypto::BytesToHex(cipherBytes) == expectedHex) {
#if defined(VULN_DEMO)
        local.admin = 1;
#else
        admin = 1;
#endif
    } else if (err.empty()) {
        err = "cipher mismatch";
    }
#if defined(VULN_DEMO)
    return static_cast<int>(local.admin);
#else
    return admin;
#endif
}

} // namespace

void RegisterAuthHandlers(CommandRouter& router, const ServerConfig& config) {
    router.RegisterCommand("LOGIN_LOW", Session::Level::Guest,
        [config](const protocol::RequestMessage& req, Session& session, protocol::ResponseMessage& resp) {
            if (session.level() != Session::Level::Guest) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::BadRequest;
                resp.msg = "already logged in";
                resp.data.fields.clear();
                return;
            }
            std::string user;
            protocol::GetString(req.args, "username", user);

            std::string pass;
            if (!protocol::GetString(req.args, "password", pass)) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::BadRequest;
                resp.msg = "args.password required";
                resp.data.fields.clear();
                return;
            }

            bool matched = false;
            if (!config.lowPassword.empty()) {
                matched = (pass == config.lowPassword);
            } else if (!user.empty()) {
                for (const auto& u : config.lowUsers) {
                    if (u.username == user && u.password == pass) {
                        matched = true;
                        break;
                    }
                }
            }
            if (!matched) {
                std::cout << "LOGIN_LOW failed for user: " << user << "\n";
                resp.ok = false;
                resp.code = protocol::ErrorCode::BadRequest;
                resp.msg = "auth failed";
                resp.data.fields.clear();
                return;
            }

            if (user.empty()) {
                user = "user";
            }
            session.setLevel(Session::Level::Low);
            session.setUsername(user);
            session.setLowUsername(user);

            std::cout << "LOGIN_LOW success for user: " << user << "\n";
            resp.ok = true;
            resp.code = protocol::ErrorCode::Ok;
            resp.msg = "login_low_ok";
            resp.data.fields.clear();
            SetString(resp.data, "level", session.levelString());
            SetString(resp.data, "username", session.username());
        });

    router.RegisterCommand("LOGIN_HIGH", Session::Level::Low,
        [config](const protocol::RequestMessage& req, Session& session, protocol::ResponseMessage& resp) {
            if (session.level() != Session::Level::Low) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::BadRequest;
                resp.msg = "already high";
                resp.data.fields.clear();
                return;
            }
            std::string user;
            protocol::GetString(req.args, "username", user);

            std::string cipherHex;
            if (!protocol::GetString(req.args, "password_cipher_hex", cipherHex)) {
                resp.ok = false;
                resp.code = protocol::ErrorCode::BadRequest;
                resp.msg = "args.password_cipher_hex required";
                resp.data.fields.clear();
                return;
            }

            std::string expectedHex;
            std::string encErr;
            if (!crypto::DesEncryptEcbPkcs7Hex(config.adminPassPlain, config.desKeyBytes, expectedHex, encErr)) {
                std::cout << "LOGIN_HIGH encrypt error: " << encErr << "\n";
                resp.ok = false;
                resp.code = protocol::ErrorCode::InternalError;
                resp.msg = "internal error";
                resp.data.fields.clear();
                return;
            }

            std::string err;
            const int admin = AdminLogin(cipherHex, expectedHex, err);
            if (admin == 0) {
                std::cout << "LOGIN_HIGH auth failed for user: " << user << " (" << err << ")\n";
                resp.ok = false;
                resp.code = protocol::ErrorCode::BadRequest;
                resp.msg = "auth failed";
                resp.data.fields.clear();
                return;
            }

            session.setLevel(Session::Level::High);
            session.setUsername(config.adminUser);

            const std::string displayUser = user.empty() ? config.adminUser : user;
            std::cout << "LOGIN_HIGH success for user: " << displayUser << "\n";
            resp.ok = true;
            resp.code = protocol::ErrorCode::Ok;
            resp.msg = "login_high_ok";
            resp.data.fields.clear();
            SetString(resp.data, "level", session.levelString());
            SetString(resp.data, "username", session.username());
        });

    router.RegisterCommand("LOGOUT", Session::Level::Low,
        [](const protocol::RequestMessage&, Session& session, protocol::ResponseMessage& resp) {
            if (session.level() == Session::Level::High) {
                session.setLevel(Session::Level::Low);
                session.setUsername(session.lowUsername());
            } else if (session.level() == Session::Level::Low) {
                session.setLevel(Session::Level::Guest);
                session.setUsername("");
                session.clearLowUsername();
            } else {
                resp.ok = false;
                resp.code = protocol::ErrorCode::BadRequest;
                resp.msg = "not logged in";
                resp.data.fields.clear();
                return;
            }

            resp.ok = true;
            resp.code = protocol::ErrorCode::Ok;
            resp.msg = "logout_ok";
            resp.data.fields.clear();
            SetString(resp.data, "level", session.levelString());
            SetString(resp.data, "username", session.username());
        });
}

} // namespace server

#include "DesCipher.h"

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/opensslv.h>

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/provider.h>
#endif

namespace crypto {

namespace {

int HexNibble(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

void EnsureLegacyProvider() {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    static bool attempted = false;
    static OSSL_PROVIDER* legacy = nullptr;
    static OSSL_PROVIDER* def = nullptr;
    if (!attempted) {
        attempted = true;
        def = OSSL_PROVIDER_load(nullptr, "default");
        legacy = OSSL_PROVIDER_load(nullptr, "legacy");
        (void)legacy;
        (void)def;
    }
#endif
}

std::string LastOpenSSLError() {
    unsigned long err = ERR_get_error();
    if (err == 0) {
        return "unknown OpenSSL error";
    }
    char buf[256];
    ERR_error_string_n(err, buf, sizeof(buf));
    return std::string(buf);
}

} // namespace

bool HexToBytes(const std::string& hex, std::vector<uint8_t>& out) {
    if (hex.size() % 2 != 0) {
        return false;
    }
    std::vector<uint8_t> bytes;
    bytes.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        const int hi = HexNibble(hex[i]);
        const int lo = HexNibble(hex[i + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        bytes.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    out.swap(bytes);
    return true;
}

std::string BytesToHex(const std::vector<uint8_t>& bytes) {
    static const char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (uint8_t b : bytes) {
        out.push_back(kHex[(b >> 4) & 0xF]);
        out.push_back(kHex[b & 0xF]);
    }
    return out;
}

bool DesEncryptEcbPkcs7(const std::string& plain,
                        const std::vector<uint8_t>& key,
                        std::vector<uint8_t>& out,
                        std::string& err) {
    if (key.size() != 8) {
        err = "invalid DES key length";
        return false;
    }

    EnsureLegacyProvider();

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        err = "EVP_CIPHER_CTX_new failed";
        return false;
    }

    const EVP_CIPHER* cipher = nullptr;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    EVP_CIPHER* fetched = EVP_CIPHER_fetch(nullptr, "DES-ECB", "provider=legacy");
    if (!fetched) {
        fetched = EVP_CIPHER_fetch(nullptr, "DES-ECB", nullptr);
    }
    cipher = fetched ? fetched : EVP_des_ecb();
#else
    cipher = EVP_des_ecb();
#endif
    if (!cipher) {
        EVP_CIPHER_CTX_free(ctx);
        err = "DES-ECB cipher unavailable";
        return false;
    }

    if (EVP_EncryptInit_ex(ctx, cipher, nullptr, key.data(), nullptr) != 1) {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
        if (fetched) {
            EVP_CIPHER_free(fetched);
        }
#endif
        EVP_CIPHER_CTX_free(ctx);
        err = "EVP_EncryptInit_ex failed: " + LastOpenSSLError();
        return false;
    }

    EVP_CIPHER_CTX_set_padding(ctx, 1);

    std::vector<uint8_t> buffer(plain.size() + EVP_CIPHER_block_size(cipher));
    int outLen1 = 0;
    if (EVP_EncryptUpdate(ctx,
                          buffer.data(),
                          &outLen1,
                          reinterpret_cast<const unsigned char*>(plain.data()),
                          static_cast<int>(plain.size())) != 1) {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
        if (fetched) {
            EVP_CIPHER_free(fetched);
        }
#endif
        EVP_CIPHER_CTX_free(ctx);
        err = "EVP_EncryptUpdate failed: " + LastOpenSSLError();
        return false;
    }

    int outLen2 = 0;
    if (EVP_EncryptFinal_ex(ctx, buffer.data() + outLen1, &outLen2) != 1) {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
        if (fetched) {
            EVP_CIPHER_free(fetched);
        }
#endif
        EVP_CIPHER_CTX_free(ctx);
        err = "EVP_EncryptFinal_ex failed: " + LastOpenSSLError();
        return false;
    }

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    if (fetched) {
        EVP_CIPHER_free(fetched);
    }
#endif
    EVP_CIPHER_CTX_free(ctx);

    buffer.resize(static_cast<size_t>(outLen1 + outLen2));
    out.swap(buffer);
    return true;
}

bool DesEncryptEcbPkcs7Hex(const std::string& plain,
                           const std::vector<uint8_t>& key,
                           std::string& outHex,
                           std::string& err) {
    std::vector<uint8_t> cipher;
    if (!DesEncryptEcbPkcs7(plain, key, cipher, err)) {
        return false;
    }
    outHex = BytesToHex(cipher);
    return true;
}

} // namespace crypto

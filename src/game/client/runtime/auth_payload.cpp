#include "client/runtime/internal.hpp"

#include <openssl/evp.h>

#include <array>
#include <cstddef>
#include <optional>
#include <string>

namespace bz3::client::runtime_detail {

std::string BuildStructuredAuthPayload(std::string username,
                                       std::optional<std::string> password,
                                       std::optional<std::string> passhash) {
    if (username.empty()) {
        return {};
    }
    karma::json::Value auth = karma::json::Object();
    auth["username"] = std::move(username);
    if (passhash.has_value() && !passhash->empty()) {
        auth["passhash"] = std::move(*passhash);
    } else if (password.has_value() && !password->empty()) {
        auth["password"] = std::move(*password);
    }
    if (!auth.contains("password") && !auth.contains("passhash")) {
        return {};
    }
    return karma::json::Dump(auth);
}

bool HashPasswordPBKDF2Sha256(const std::string& password,
                              const std::string& salt,
                              std::string* out_hex_digest) {
    if (!out_hex_digest || salt.empty()) {
        return false;
    }

    constexpr int kIterations = 100000;
    constexpr std::size_t kDigestLen = 32;
    std::array<unsigned char, kDigestLen> digest{};
    const int ok = PKCS5_PBKDF2_HMAC(password.data(),
                                     static_cast<int>(password.size()),
                                     reinterpret_cast<const unsigned char*>(salt.data()),
                                     static_cast<int>(salt.size()),
                                     kIterations,
                                     EVP_sha256(),
                                     static_cast<int>(digest.size()),
                                     digest.data());
    if (ok != 1) {
        return false;
    }

    static constexpr char kHex[] = "0123456789abcdef";
    std::string hex{};
    hex.resize(digest.size() * 2);
    for (size_t i = 0; i < digest.size(); ++i) {
        const unsigned char value = digest[i];
        hex[i * 2] = kHex[(value >> 4) & 0x0F];
        hex[(i * 2) + 1] = kHex[value & 0x0F];
    }
    *out_hex_digest = std::move(hex);
    return true;
}

} // namespace bz3::client::runtime_detail

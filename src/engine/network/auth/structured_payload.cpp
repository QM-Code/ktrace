#include "karma/network/auth/structured_payload.hpp"

#include "karma/common/serialization/json.hpp"

#include <openssl/evp.h>

#include <array>
#include <cctype>
#include <cstddef>

namespace karma::network::auth {
namespace {

std::string TrimCopy(std::string_view value) {
    size_t begin = 0;
    size_t end = value.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return std::string(value.substr(begin, end - begin));
}

} // namespace

std::string BuildStructuredAuthPayload(std::string username,
                                       std::optional<std::string> password,
                                       std::optional<std::string> passhash) {
    if (username.empty()) {
        return {};
    }

    karma::common::serialization::Value auth = karma::common::serialization::Object();
    auth["username"] = std::move(username);
    if (passhash.has_value() && !passhash->empty()) {
        auth["passhash"] = std::move(*passhash);
    } else if (password.has_value() && !password->empty()) {
        auth["password"] = std::move(*password);
    }
    if (!auth.contains("password") && !auth.contains("passhash")) {
        return {};
    }
    return karma::common::serialization::Dump(auth);
}

StructuredAuthPayload ParseStructuredAuthPayload(std::string_view auth_payload) {
    StructuredAuthPayload out{};
    const std::string trimmed = TrimCopy(auth_payload);
    if (trimmed.empty() || trimmed.front() != '{') {
        return out;
    }

    try {
        const auto json_data = karma::common::serialization::Parse(trimmed);
        if (!json_data.is_object()) {
            return out;
        }
        if (const auto it = json_data.find("username"); it != json_data.end() && it->is_string()) {
            out.username = it->get<std::string>();
        }
        if (const auto it = json_data.find("password"); it != json_data.end() && it->is_string()) {
            out.password = it->get<std::string>();
        }
        if (const auto it = json_data.find("passhash"); it != json_data.end() && it->is_string()) {
            out.passhash = it->get<std::string>();
        }
    } catch (...) {
    }

    return out;
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

} // namespace karma::network::auth

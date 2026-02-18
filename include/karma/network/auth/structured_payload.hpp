#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace karma::network::auth {

struct StructuredAuthPayload {
    std::string username{};
    std::string password{};
    std::string passhash{};
};

std::string BuildStructuredAuthPayload(std::string username,
                                       std::optional<std::string> password,
                                       std::optional<std::string> passhash);
StructuredAuthPayload ParseStructuredAuthPayload(std::string_view auth_payload);
bool HashPasswordPBKDF2Sha256(const std::string& password,
                              const std::string& salt,
                              std::string* out_hex_digest);

} // namespace karma::network::auth

#pragma once

#include "game.hpp"
#include "karma/app/engine_app.hpp"
#include "karma/cli/client_app_options.hpp"
#include "karma/cli/client_runtime_options.hpp"
#include "karma/common/json.hpp"

#include <glm/glm.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace bz3::client::runtime_detail {

struct CommunityCredential {
    std::string username{};
    std::string password_hash{};
    std::string salt{};
};

std::string ToLowerAscii(std::string value);
std::string StripIpv6Brackets(std::string host);
std::string ExtractHostFromCredentialKey(std::string key);
bool IsLanLikeHost(const std::string& host_in);

std::optional<CommunityCredential> ReadCommunityCredentialEntry(const karma::json::Value& credentials,
                                                                const std::string& key);
std::optional<CommunityCredential> ResolveCommunityCredential(
    const std::optional<karma::cli::ClientServerEndpoint>& endpoint,
    const std::string& server_option_raw);

std::string BuildStructuredAuthPayload(std::string username,
                                       std::optional<std::string> password,
                                       std::optional<std::string> passhash);
bool HashPasswordPBKDF2Sha256(const std::string& password,
                              const std::string& salt,
                              std::string* out_hex_digest);

glm::vec3 ReadRequiredVec3(const char* path);
glm::vec4 ReadRequiredColor(const char* path);

bz3::GameStartupOptions ResolveGameStartupOptions(const karma::cli::ClientAppOptions& options);
karma::app::EngineConfig BuildEngineConfig(const karma::cli::ClientAppOptions& options);

} // namespace bz3::client::runtime_detail

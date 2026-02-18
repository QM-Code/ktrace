#pragma once

#include "karma/cli/client_runtime_options.hpp"

#include <optional>
#include <string>

namespace karma::network::client::auth {

struct CommunityCredential {
    std::string username{};
    std::string password_hash{};
    std::string salt{};
};

std::optional<CommunityCredential> ResolveCommunityCredential(
    const std::optional<karma::cli::ClientServerEndpoint>& endpoint,
    const std::string& server_option_raw);

} // namespace karma::network::client::auth

#include "client/runtime/internal.hpp"

#include "karma/cli/client/runtime_options.hpp"
#include "karma/common/config/helpers.hpp"
#include "karma/network/auth/structured_payload.hpp"
#include "karma/network/client/auth/community_credentials.hpp"

#include <optional>
#include <stdexcept>
#include <string>

namespace bz3::client::runtime_detail {

bz3::GameStartupOptions ResolveGameStartupOptions(const karma::cli::client::AppOptions& options) {
    bz3::GameStartupOptions startup{};

    std::string connect_error{};
    const auto endpoint =
        karma::cli::client::ResolveServerEndpoint(options.server, options.server_explicit, &connect_error);

    const auto credential =
        karma::network::client::auth::ResolveCommunityCredential(endpoint, options.server);
    const std::string default_player_name = (credential.has_value() && !credential->username.empty())
        ? credential->username
        : karma::common::config::ReadStringConfig({"userDefaults.username"}, "Player");
    startup.player_name = karma::cli::client::ResolvePlayerName(
        options.username,
        options.username_explicit,
        default_player_name);

    if (options.password_explicit) {
        std::optional<std::string> password = options.password;
        std::optional<std::string> passhash = std::nullopt;
        const bool can_hash_with_salt = credential.has_value()
            && !credential->salt.empty()
            && (!options.username_explicit
                || credential->username.empty()
                || credential->username == startup.player_name);
        if (can_hash_with_salt) {
            std::string computed_hash{};
            if (karma::network::auth::HashPasswordPBKDF2Sha256(
                    options.password, credential->salt, &computed_hash)) {
                password = std::nullopt;
                passhash = std::move(computed_hash);
            }
        }
        startup.auth_payload = karma::network::auth::BuildStructuredAuthPayload(
            startup.player_name,
            std::move(password),
            std::move(passhash));
    } else if (credential.has_value()
               && !credential->password_hash.empty()
               && (!options.username_explicit
                   || credential->username.empty()
                   || credential->username == startup.player_name)) {
        startup.auth_payload = karma::network::auth::BuildStructuredAuthPayload(
            startup.player_name,
            std::nullopt,
            credential->password_hash);
    }

    if (!endpoint.has_value()) {
        if (options.server_explicit) {
            throw std::runtime_error(connect_error.empty()
                    ? std::string("Client --server is invalid.")
                    : connect_error);
        }
        return startup;
    }

    startup.connect_addr = endpoint->host;
    startup.connect_port = endpoint->port;
    startup.connect_on_start = true;
    return startup;
}

} // namespace bz3::client::runtime_detail

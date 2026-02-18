#include "karma/network/client/auth/community_credentials.hpp"

#include "karma/common/config_store.hpp"

#include <algorithm>
#include <cctype>

namespace karma::network::client::auth {
namespace {

std::string ToLowerAscii(std::string value) {
    std::transform(value.begin(),
                   value.end(),
                   value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::string StripIpv6Brackets(std::string host) {
    if (host.size() >= 2 && host.front() == '[' && host.back() == ']') {
        return host.substr(1, host.size() - 2);
    }
    return host;
}

std::string ExtractHostFromCredentialKey(std::string key) {
    if (key.empty()) {
        return {};
    }
    const std::size_t scheme_split = key.find("://");
    if (scheme_split != std::string::npos) {
        key = key.substr(scheme_split + 3);
    }
    const std::size_t path_split = key.find_first_of("/?#");
    if (path_split != std::string::npos) {
        key = key.substr(0, path_split);
    }
    if (key.empty()) {
        return {};
    }

    if (key.front() == '[') {
        const std::size_t closing = key.find(']');
        if (closing == std::string::npos) {
            return {};
        }
        return ToLowerAscii(key.substr(1, closing - 1));
    }

    const std::size_t port_split = key.find(':');
    if (port_split != std::string::npos) {
        key = key.substr(0, port_split);
    }
    return ToLowerAscii(key);
}

bool IsLanLikeHost(const std::string& host_in) {
    const std::string host = ToLowerAscii(StripIpv6Brackets(host_in));
    if (host == "localhost" || host == "::1" || host == "127.0.0.1") {
        return true;
    }
    if (host.rfind("10.", 0) == 0 || host.rfind("192.168.", 0) == 0) {
        return true;
    }
    if (host.rfind("172.", 0) == 0) {
        const std::size_t next_dot = host.find('.', 4);
        if (next_dot != std::string::npos) {
            try {
                const int second_octet = std::stoi(host.substr(4, next_dot - 4));
                return second_octet >= 16 && second_octet <= 31;
            } catch (...) {
                return false;
            }
        }
    }
    return false;
}

std::optional<CommunityCredential> ReadCommunityCredentialEntry(const karma::json::Value& credentials,
                                                                const std::string& key) {
    const auto it = credentials.find(key);
    if (it == credentials.end() || !it->is_object()) {
        return std::nullopt;
    }
    CommunityCredential out{};
    if (const auto username_it = it->find("username");
        username_it != it->end() && username_it->is_string()) {
        out.username = username_it->get<std::string>();
    }
    if (const auto hash_it = it->find("passwordHash");
        hash_it != it->end() && hash_it->is_string()) {
        out.password_hash = hash_it->get<std::string>();
    }
    if (const auto salt_it = it->find("salt");
        salt_it != it->end() && salt_it->is_string()) {
        out.salt = salt_it->get<std::string>();
    }
    return out;
}

} // namespace

std::optional<CommunityCredential> ResolveCommunityCredential(
    const std::optional<karma::cli::ClientServerEndpoint>& endpoint,
    const std::string& server_option_raw) {
    const auto* credentials = karma::config::ConfigStore::Get("communityCredentials");
    if (!credentials || !credentials->is_object()) {
        return std::nullopt;
    }

    if (!server_option_raw.empty()) {
        if (auto exact = ReadCommunityCredentialEntry(*credentials, server_option_raw)) {
            return exact;
        }
    }

    std::string endpoint_host{};
    if (endpoint.has_value()) {
        endpoint_host = ToLowerAscii(StripIpv6Brackets(endpoint->host));
        const std::string endpoint_host_with_port = endpoint->host + ":" + std::to_string(endpoint->port);
        if (auto exact = ReadCommunityCredentialEntry(*credentials, endpoint_host_with_port)) {
            return exact;
        }
        if (auto exact = ReadCommunityCredentialEntry(*credentials, endpoint->host)) {
            return exact;
        }
        if (auto exact = ReadCommunityCredentialEntry(*credentials, "http://" + endpoint_host_with_port)) {
            return exact;
        }
        if (auto exact = ReadCommunityCredentialEntry(*credentials, "https://" + endpoint_host_with_port)) {
            return exact;
        }
    }

    if (!endpoint_host.empty()) {
        for (auto it = credentials->begin(); it != credentials->end(); ++it) {
            if (!it->is_object()) {
                continue;
            }
            const std::string key = it.key();
            if (ToLowerAscii(key) == "lan") {
                continue;
            }
            const std::string key_host = ExtractHostFromCredentialKey(key);
            if (!key_host.empty() && key_host == endpoint_host) {
                CommunityCredential out{};
                if (const auto username_it = it->find("username");
                    username_it != it->end() && username_it->is_string()) {
                    out.username = username_it->get<std::string>();
                }
                if (const auto hash_it = it->find("passwordHash");
                    hash_it != it->end() && hash_it->is_string()) {
                    out.password_hash = hash_it->get<std::string>();
                }
                if (const auto salt_it = it->find("salt");
                    salt_it != it->end() && salt_it->is_string()) {
                    out.salt = salt_it->get<std::string>();
                }
                return out;
            }
        }
    }

    if (endpoint_host.empty() || IsLanLikeHost(endpoint_host)) {
        if (auto lan = ReadCommunityCredentialEntry(*credentials, "LAN")) {
            return lan;
        }
        if (auto lan = ReadCommunityCredentialEntry(*credentials, "lan")) {
            return lan;
        }
    }

    return std::nullopt;
}

} // namespace karma::network::client::auth

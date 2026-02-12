#include "karma/network/transport_config_mapping.hpp"

#include "karma/common/config_helpers.hpp"

namespace karma::network {

ClientTransportConfig ResolveClientTransportConfigFromConfig(bool* out_backend_is_custom) {
    ClientTransportConfig transport_config{};
    const std::string backend_name =
        karma::config::ReadStringConfig({"network.ClientTransportBackend"}, std::string("auto"));
    transport_config.backend_name = backend_name;

    const auto parsed_backend = ParseClientTransportBackend(backend_name);
    const bool backend_is_custom = !parsed_backend.has_value();
    if (parsed_backend.has_value()) {
        transport_config.backend = *parsed_backend;
    }
    if (out_backend_is_custom) {
        *out_backend_is_custom = backend_is_custom;
    }
    return transport_config;
}

ServerTransportConfig ResolveServerTransportConfigFromConfig(uint16_t listen_port,
                                                             size_t max_clients,
                                                             size_t channel_count,
                                                             bool* out_backend_is_custom) {
    ServerTransportConfig transport_config{};
    transport_config.listen_port = listen_port;
    transport_config.max_clients = max_clients;
    transport_config.channel_count = channel_count;

    const std::string backend_name =
        karma::config::ReadStringConfig({"network.ServerTransportBackend"}, std::string("auto"));
    transport_config.backend_name = backend_name;

    const auto parsed_backend = ParseServerTransportBackend(backend_name);
    const bool backend_is_custom = !parsed_backend.has_value();
    if (parsed_backend.has_value()) {
        transport_config.backend = *parsed_backend;
    }
    if (out_backend_is_custom) {
        *out_backend_is_custom = backend_is_custom;
    }
    return transport_config;
}

std::string EffectiveClientTransportBackendName(const ClientTransportConfig& config) {
    return config.backend_name.empty()
        ? std::string(ClientTransportBackendName(config.backend))
        : config.backend_name;
}

std::string EffectiveServerTransportBackendName(const ServerTransportConfig& config) {
    return config.backend_name.empty()
        ? std::string(ServerTransportBackendName(config.backend))
        : config.backend_name;
}

} // namespace karma::network

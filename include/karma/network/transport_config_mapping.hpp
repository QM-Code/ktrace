#pragma once

#include "karma/network/client_transport.hpp"
#include "karma/network/server_transport.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace karma::network {

ClientTransportConfig ResolveClientTransportConfigFromConfig(bool* out_backend_is_custom = nullptr);
ServerTransportConfig ResolveServerTransportConfigFromConfig(uint16_t listen_port,
                                                             size_t max_clients,
                                                             size_t channel_count,
                                                             bool* out_backend_is_custom = nullptr);

std::string EffectiveClientTransportBackendName(const ClientTransportConfig& config);
std::string EffectiveServerTransportBackendName(const ServerTransportConfig& config);

} // namespace karma::network

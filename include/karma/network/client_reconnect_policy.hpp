#pragma once

#include "karma/network/client_transport.hpp"

#include <cstdint>

namespace karma::network {

struct ClientReconnectPolicy {
    uint32_t max_attempts = 0;
    uint32_t backoff_initial_ms = 250;
    uint32_t backoff_max_ms = 2000;
    uint32_t timeout_ms = 1000;
};

ClientReconnectPolicy ReadClientReconnectPolicyFromConfig();
void ApplyReconnectPolicyToConnectOptions(const ClientReconnectPolicy& policy,
                                          ClientTransportConnectOptions* options);

} // namespace karma::network

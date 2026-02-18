#include "karma/network/config/reconnect_policy.hpp"

#include "karma/common/config/helpers.hpp"

namespace karma::network {

ClientReconnectPolicy ReadClientReconnectPolicyFromConfig() {
    ClientReconnectPolicy policy{};
    policy.max_attempts = static_cast<uint32_t>(karma::common::config::ReadUInt16Config(
        {"network.ClientReconnectMaxAttempts", "network.ReconnectMaxAttempts"},
        static_cast<uint16_t>(0)));
    policy.backoff_initial_ms = static_cast<uint32_t>(karma::common::config::ReadUInt16Config(
        {"network.ClientReconnectBackoffInitialMs", "network.ReconnectBackoffInitialMs"},
        static_cast<uint16_t>(250)));
    policy.backoff_max_ms = static_cast<uint32_t>(karma::common::config::ReadUInt16Config(
        {"network.ClientReconnectBackoffMaxMs", "network.ReconnectBackoffMaxMs"},
        static_cast<uint16_t>(2000)));
    policy.timeout_ms = static_cast<uint32_t>(karma::common::config::ReadUInt16Config(
        {"network.ClientReconnectTimeoutMs", "network.ReconnectTimeoutMs"},
        static_cast<uint16_t>(1000)));
    return policy;
}

void ApplyReconnectPolicyToConnectOptions(const ClientReconnectPolicy& policy,
                                          ClientTransportConnectOptions* options) {
    if (!options) {
        return;
    }
    options->reconnect_max_attempts = policy.max_attempts;
    options->reconnect_backoff_initial_ms = policy.backoff_initial_ms;
    options->reconnect_backoff_max_ms = policy.backoff_max_ms;
    options->reconnect_timeout_ms = policy.timeout_ms;
}

} // namespace karma::network

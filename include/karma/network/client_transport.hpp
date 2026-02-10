#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace karma::network {

enum class ClientTransportBackend {
    Auto,
    Enet
};

const char* ClientTransportBackendName(ClientTransportBackend backend);
std::optional<ClientTransportBackend> ParseClientTransportBackend(std::string_view name);

struct ClientTransportConfig {
    ClientTransportBackend backend = ClientTransportBackend::Auto;
    std::string backend_name{};
    size_t channel_count = 2;
};

struct ClientTransportConnectOptions {
    std::string host{};
    uint16_t port = 0;
    uint32_t timeout_ms = 0;
    uint32_t reconnect_max_attempts = 0;
    uint32_t reconnect_backoff_initial_ms = 0;
    uint32_t reconnect_backoff_max_ms = 0;
    uint32_t reconnect_timeout_ms = 0;
};

enum class ClientTransportEventType {
    Connected,
    Disconnected,
    Received
};

struct ClientTransportEvent {
    ClientTransportEventType type = ClientTransportEventType::Connected;
    std::vector<std::byte> payload{};
};

struct ClientTransportPollOptions {
    uint32_t service_timeout_ms = 0;
};

class ClientTransport {
 public:
    virtual ~ClientTransport() = default;

    virtual bool isReady() const = 0;
    virtual bool isConnected() const = 0;
    virtual const char* backendName() const = 0;

    virtual bool connect(const ClientTransportConnectOptions& options) = 0;
    virtual size_t poll(const ClientTransportPollOptions& options,
                        std::vector<ClientTransportEvent>* out_events) = 0;
    virtual bool sendReliable(const std::vector<std::byte>& payload) = 0;
    virtual void disconnect(uint32_t data) = 0;
    virtual bool waitForDisconnect(uint32_t timeout_ms) = 0;
    virtual void resetConnection() = 0;
    virtual void close() = 0;
};

using ClientTransportFactory = std::function<std::unique_ptr<ClientTransport>(
    const ClientTransportConfig& config)>;

bool RegisterClientTransportFactory(std::string_view backend_name, ClientTransportFactory factory);
std::unique_ptr<ClientTransport> CreateClientTransport(const ClientTransportConfig& config);

} // namespace karma::network

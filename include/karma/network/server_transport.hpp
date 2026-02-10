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

using PeerToken = std::uintptr_t;

enum class ServerTransportBackend {
    Auto,
    Enet
};

const char* ServerTransportBackendName(ServerTransportBackend backend);
std::optional<ServerTransportBackend> ParseServerTransportBackend(std::string_view name);

struct ServerTransportConfig {
    ServerTransportBackend backend = ServerTransportBackend::Auto;
    std::string backend_name{};
    uint16_t listen_port = 0;
    size_t max_clients = 0;
    size_t channel_count = 0;
};

enum class ServerTransportEventType {
    Connected,
    Disconnected,
    Received
};

struct ServerTransportEvent {
    ServerTransportEventType type = ServerTransportEventType::Connected;
    PeerToken peer = 0;
    std::string peer_ip{};
    uint16_t peer_port = 0;
    std::vector<std::byte> payload{};
};

struct ServerTransportPollOptions {
    uint32_t service_timeout_ms = 0;
};

class ServerTransport {
 public:
    virtual ~ServerTransport() = default;

    virtual bool isReady() const = 0;
    virtual const char* backendName() const = 0;

    virtual size_t poll(const ServerTransportPollOptions& options,
                        std::vector<ServerTransportEvent>* out_events) = 0;
    virtual bool sendReliable(PeerToken peer, const std::vector<std::byte>& payload) = 0;
    virtual void disconnect(PeerToken peer, uint32_t data) = 0;
};

using ServerTransportFactory = std::function<std::unique_ptr<ServerTransport>(
    const ServerTransportConfig& config)>;

bool RegisterServerTransportFactory(std::string_view backend_name, ServerTransportFactory factory);
std::unique_ptr<ServerTransport> CreateServerTransport(const ServerTransportConfig& config);

} // namespace karma::network

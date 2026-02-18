#include "karma/network/transport/server.hpp"
#include "network/transport/pump_normalizer.hpp"

#include <enet.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <mutex>
#include <unordered_map>

namespace karma::network {
namespace {

class EnetGlobalGuard {
 public:
    EnetGlobalGuard() {
        std::lock_guard<std::mutex> lock(mutex());
        auto& count = refCount();
        if (count++ == 0) {
            if (enet_initialize() != 0) {
                spdlog::error("Network transport: enet_initialize() failed");
                initialized_ = false;
                --count;
                return;
            }
        }
        initialized_ = true;
    }

    ~EnetGlobalGuard() {
        std::lock_guard<std::mutex> lock(mutex());
        auto& count = refCount();
        if (count == 0) {
            return;
        }
        if (--count == 0) {
            enet_deinitialize();
        }
    }

    bool initialized() const { return initialized_; }

 private:
    static std::mutex& mutex() {
        static std::mutex m;
        return m;
    }

    static uint32_t& refCount() {
        static uint32_t count = 0;
        return count;
    }

    bool initialized_ = false;
};

std::string Lower(std::string_view value) {
    std::string lowered(value);
    std::transform(lowered.begin(),
                   lowered.end(),
                   lowered.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return lowered;
}

using ServerTransportFactoryMap = std::unordered_map<std::string, ServerTransportFactory>;

ServerTransportFactoryMap& ServerTransportFactories() {
    static ServerTransportFactoryMap factories{};
    return factories;
}

std::mutex& ServerTransportFactoriesMutex() {
    static std::mutex mutex;
    return mutex;
}

std::string PeerIpString(const ENetAddress& address) {
    std::array<char, 128> ip_buffer{};
    if (enet_address_get_host_ip(&address, ip_buffer.data(), ip_buffer.size()) == 0) {
        return std::string(ip_buffer.data());
    }
    return "unknown";
}

class EnetServerTransport final : public ServerTransport {
 public:
    explicit EnetServerTransport(const ServerTransportConfig& config) {
        if (!global_.initialized()) {
            return;
        }

        ENetAddress address{};
        address.host = ENET_HOST_ANY;
        address.port = config.listen_port;
        const size_t max_clients = config.max_clients > 0 ? config.max_clients : 50;
        const size_t channel_count = config.channel_count > 0 ? config.channel_count : 2;
        host_ = enet_host_create(&address, max_clients, channel_count, 0, 0);
        if (!host_) {
            spdlog::error("Network transport: failed to create ENet host on port {}",
                          config.listen_port);
            return;
        }
        ready_ = true;
    }

    ~EnetServerTransport() override {
        if (host_) {
            enet_host_destroy(host_);
            host_ = nullptr;
        }
    }

    bool isReady() const override { return ready_; }

    const char* backendName() const override { return "enet"; }

    size_t poll(const ServerTransportPollOptions& options,
                std::vector<ServerTransportEvent>* out_events) override {
        if (!ready_ || !host_ || !out_events) {
            return 0;
        }

        std::vector<ServerTransportEvent> staged_events{};
        ENetEvent event{};
        int service_result = enet_host_service(host_, &event, options.service_timeout_ms);
        while (service_result > 0) {
            ServerTransportEvent transport_event{};
            transport_event.peer = reinterpret_cast<PeerToken>(event.peer);
            transport_event.peer_ip = event.peer ? PeerIpString(event.peer->address) : "unknown";
            transport_event.peer_port = event.peer ? event.peer->address.port : 0;

            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    transport_event.type = ServerTransportEventType::Connected;
                    staged_events.push_back(std::move(transport_event));
                    break;
                case ENET_EVENT_TYPE_DISCONNECT:
                case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
                    transport_event.type = ServerTransportEventType::Disconnected;
                    staged_events.push_back(std::move(transport_event));
                    break;
                case ENET_EVENT_TYPE_RECEIVE:
                    transport_event.type = ServerTransportEventType::Received;
                    if (event.packet && event.packet->data && event.packet->dataLength > 0) {
                        transport_event.payload.resize(event.packet->dataLength);
                        std::copy_n(reinterpret_cast<const std::byte*>(event.packet->data),
                                    event.packet->dataLength,
                                    transport_event.payload.data());
                    }
                    staged_events.push_back(std::move(transport_event));
                    if (event.packet) {
                        enet_packet_destroy(event.packet);
                    }
                    break;
                default:
                    break;
            }

            service_result = enet_host_service(host_, &event, 0);
        }

        return transport::detail::NormalizePumpEventsPerKey(
            out_events,
            &staged_events,
            ServerTransportEventType::Connected,
            ServerTransportEventType::Received,
            ServerTransportEventType::Disconnected,
            [](const ServerTransportEvent& event) { return event.peer; });
    }

    bool sendReliable(PeerToken peer, const std::vector<std::byte>& payload) override {
        if (!ready_ || !host_ || peer == 0 || payload.empty()) {
            return false;
        }

        auto* peer_handle = reinterpret_cast<ENetPeer*>(peer);
        if (!peer_handle) {
            return false;
        }

        ENetPacket* packet =
            enet_packet_create(payload.data(), payload.size(), ENET_PACKET_FLAG_RELIABLE);
        if (!packet) {
            return false;
        }

        if (enet_peer_send(peer_handle, 0, packet) != 0) {
            enet_packet_destroy(packet);
            return false;
        }

        enet_host_flush(host_);
        return true;
    }

    void disconnect(PeerToken peer, uint32_t data) override {
        if (!ready_ || peer == 0) {
            return;
        }

        auto* peer_handle = reinterpret_cast<ENetPeer*>(peer);
        if (!peer_handle) {
            return;
        }
        enet_peer_disconnect(peer_handle, data);
    }

 private:
    EnetGlobalGuard global_{};
    ENetHost* host_ = nullptr;
    bool ready_ = false;
};

std::unique_ptr<ServerTransport> CreateRegisteredServerTransport(
    std::string_view backend_name,
    const ServerTransportConfig& config) {
    ServerTransportFactory factory{};
    {
        std::lock_guard<std::mutex> lock(ServerTransportFactoriesMutex());
        const auto it = ServerTransportFactories().find(std::string(backend_name));
        if (it == ServerTransportFactories().end()) {
            return nullptr;
        }
        factory = it->second;
    }
    if (!factory) {
        return nullptr;
    }
    return factory(config);
}

bool IsServerTransportFactoryRegistered(std::string_view backend_name) {
    std::lock_guard<std::mutex> lock(ServerTransportFactoriesMutex());
    const auto it = ServerTransportFactories().find(std::string(backend_name));
    return it != ServerTransportFactories().end() && static_cast<bool>(it->second);
}

void EnsureDefaultServerTransportFactoriesRegistered() {
    static const bool registered = []() {
        RegisterServerTransportFactory(
            "enet",
            [](const ServerTransportConfig& config) -> std::unique_ptr<ServerTransport> {
                auto transport = std::make_unique<EnetServerTransport>(config);
                if (!transport->isReady()) {
                    return nullptr;
                }
                return transport;
            });
        return true;
    }();
    (void)registered;
}

} // namespace

const char* ServerTransportBackendName(ServerTransportBackend backend) {
    switch (backend) {
        case ServerTransportBackend::Auto: return "auto";
        case ServerTransportBackend::Enet: return "enet";
        default: return "unknown";
    }
}

std::optional<ServerTransportBackend> ParseServerTransportBackend(std::string_view name) {
    const std::string lowered = Lower(name);
    if (lowered.empty() || lowered == "auto") {
        return ServerTransportBackend::Auto;
    }
    if (lowered == "enet") {
        return ServerTransportBackend::Enet;
    }
    return std::nullopt;
}

bool RegisterServerTransportFactory(std::string_view backend_name, ServerTransportFactory factory) {
    const std::string lowered = Lower(backend_name);
    if (lowered.empty() || !factory) {
        return false;
    }

    std::lock_guard<std::mutex> lock(ServerTransportFactoriesMutex());
    ServerTransportFactories()[lowered] = std::move(factory);
    return true;
}

std::unique_ptr<ServerTransport> CreateServerTransport(const ServerTransportConfig& config) {
    EnsureDefaultServerTransportFactoriesRegistered();

    std::string backend_name = config.backend_name.empty()
        ? std::string(ServerTransportBackendName(config.backend))
        : Lower(config.backend_name);
    if (backend_name.empty() || backend_name == "auto") {
        backend_name = "enet";
    }

    const bool backend_registered = IsServerTransportFactoryRegistered(backend_name);
    auto transport = CreateRegisteredServerTransport(backend_name, config);
    if (transport && transport->isReady()) {
        return transport;
    }
    if (!backend_registered) {
        spdlog::warn("Network transport: unregistered server transport backend='{}'", backend_name);
    }
    return nullptr;
}

} // namespace karma::network

#include "karma/network/transport/client.hpp"
#include "network/transport/pump_normalizer.hpp"

#include <enet.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <chrono>
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

using ClientTransportFactoryMap = std::unordered_map<std::string, ClientTransportFactory>;

ClientTransportFactoryMap& ClientTransportFactories() {
    static ClientTransportFactoryMap factories{};
    return factories;
}

std::mutex& ClientTransportFactoriesMutex() {
    static std::mutex mutex;
    return mutex;
}

uint32_t ReconnectBackoffForAttempt(const ClientTransportConnectOptions& options,
                                    uint32_t attempt_number) {
    if (attempt_number == 0 || options.reconnect_backoff_initial_ms == 0) {
        return 0;
    }

    const uint64_t cap_ms = options.reconnect_backoff_max_ms > 0
        ? static_cast<uint64_t>(options.reconnect_backoff_max_ms)
        : static_cast<uint64_t>(options.reconnect_backoff_initial_ms);

    uint64_t delay_ms = options.reconnect_backoff_initial_ms;
    for (uint32_t step = 1; step < attempt_number; ++step) {
        delay_ms = std::min(delay_ms * 2U, cap_ms);
    }
    return static_cast<uint32_t>(std::min(delay_ms, cap_ms));
}

class EnetClientTransport final : public ClientTransport {
 public:
    explicit EnetClientTransport(const ClientTransportConfig& config)
        : channel_count_(config.channel_count > 0 ? config.channel_count : 2) {
        if (!global_.initialized()) {
            return;
        }

        host_ = enet_host_create(nullptr, 1, channel_count_, 0, 0);
        if (!host_) {
            spdlog::error("Network transport: failed to create ENet client host");
            return;
        }
        ready_ = true;
    }

    ~EnetClientTransport() override {
        close();
    }

    bool isReady() const override { return ready_; }

    bool isConnected() const override { return connected_; }

    const char* backendName() const override { return "enet"; }

    bool connect(const ClientTransportConnectOptions& options) override {
        if (!ready_ || !host_ || options.host.empty() || options.port == 0) {
            return false;
        }

        if (connected_) {
            return true;
        }

        connect_options_ = options;
        has_connect_options_ = true;
        reconnect_pending_ = false;
        reconnect_attempts_ = 0;
        explicit_disconnect_requested_ = false;

        return attemptConnect(options, options.timeout_ms);
    }

    size_t poll(const ClientTransportPollOptions& options,
                std::vector<ClientTransportEvent>* out_events) override {
        if (!ready_ || !host_ || !out_events) {
            return 0;
        }

        std::vector<ClientTransportEvent> staged_events{};
        ENetEvent event{};
        int service_result = enet_host_service(host_, &event, options.service_timeout_ms);
        while (service_result > 0) {
            ClientTransportEvent transport_event{};
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    connected_ = true;
                    reconnect_pending_ = false;
                    reconnect_attempts_ = 0;
                    explicit_disconnect_requested_ = false;
                    transport_event.type = ClientTransportEventType::Connected;
                    staged_events.push_back(std::move(transport_event));
                    break;
                case ENET_EVENT_TYPE_DISCONNECT:
                case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
                    connected_ = false;
                    peer_ = nullptr;
                    if (explicit_disconnect_requested_ || !canReconnect()) {
                        explicit_disconnect_requested_ = false;
                        reconnect_pending_ = false;
                        reconnect_attempts_ = 0;
                        transport_event.type = ClientTransportEventType::Disconnected;
                        staged_events.push_back(std::move(transport_event));
                    } else {
                        reconnect_pending_ = true;
                        reconnect_attempts_ = 0;
                        scheduleNextReconnectAttempt();
                    }
                    break;
                case ENET_EVENT_TYPE_RECEIVE:
                    transport_event.type = ClientTransportEventType::Received;
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

        pollReconnect(&staged_events);
        return transport::detail::NormalizePumpEventsPerKey(out_events,
                                                            &staged_events,
                                                            ClientTransportEventType::Connected,
                                                            ClientTransportEventType::Received,
                                                            ClientTransportEventType::Disconnected,
                                                            [](const ClientTransportEvent&) {
                                                                return 0u;
                                                            });
    }

    bool sendReliable(const std::vector<std::byte>& payload) override {
        if (!ready_ || !host_ || !peer_ || !connected_ || payload.empty()) {
            return false;
        }

        ENetPacket* packet =
            enet_packet_create(payload.data(), payload.size(), ENET_PACKET_FLAG_RELIABLE);
        if (!packet) {
            return false;
        }

        if (enet_peer_send(peer_, 0, packet) != 0) {
            enet_packet_destroy(packet);
            return false;
        }
        enet_host_flush(host_);
        return true;
    }

    void disconnect(uint32_t data) override {
        if (!ready_ || !peer_) {
            return;
        }
        explicit_disconnect_requested_ = true;
        reconnect_pending_ = false;
        reconnect_attempts_ = 0;
        enet_peer_disconnect(peer_, data);
    }

    bool waitForDisconnect(uint32_t timeout_ms) override {
        if (!ready_ || !host_ || !peer_) {
            return true;
        }

        ENetEvent event{};
        while (enet_host_service(host_, &event, timeout_ms) > 0) {
            if (event.type == ENET_EVENT_TYPE_RECEIVE && event.packet) {
                enet_packet_destroy(event.packet);
            }
            if (event.type == ENET_EVENT_TYPE_DISCONNECT ||
                event.type == ENET_EVENT_TYPE_DISCONNECT_TIMEOUT) {
                connected_ = false;
                peer_ = nullptr;
                reconnect_pending_ = false;
                reconnect_attempts_ = 0;
                explicit_disconnect_requested_ = false;
                return true;
            }
        }
        return false;
    }

    void resetConnection() override {
        if (!peer_) {
            connected_ = false;
            reconnect_pending_ = false;
            reconnect_attempts_ = 0;
            explicit_disconnect_requested_ = false;
            return;
        }
        enet_peer_reset(peer_);
        peer_ = nullptr;
        connected_ = false;
        reconnect_pending_ = false;
        reconnect_attempts_ = 0;
        explicit_disconnect_requested_ = false;
    }

    void close() override {
        if (peer_) {
            enet_peer_reset(peer_);
            peer_ = nullptr;
        }
        connected_ = false;
        reconnect_pending_ = false;
        reconnect_attempts_ = 0;
        explicit_disconnect_requested_ = false;
        has_connect_options_ = false;
        if (host_) {
            enet_host_destroy(host_);
            host_ = nullptr;
        }
    }

 private:
    bool canReconnect() const {
        return has_connect_options_ && connect_options_.reconnect_max_attempts > 0 &&
               !connect_options_.host.empty() && connect_options_.port != 0;
    }

    bool attemptConnect(const ClientTransportConnectOptions& options, uint32_t timeout_ms) {
        ENetAddress address{};
        if (enet_address_set_host(&address, options.host.c_str()) != 0) {
            return false;
        }
        address.port = options.port;

        peer_ = enet_host_connect(host_, &address, channel_count_, 0);
        if (!peer_) {
            return false;
        }

        ENetEvent event{};
        if (enet_host_service(host_, &event, timeout_ms) <= 0 ||
            event.type != ENET_EVENT_TYPE_CONNECT) {
            if (event.type == ENET_EVENT_TYPE_RECEIVE && event.packet) {
                enet_packet_destroy(event.packet);
            }
            enet_peer_reset(peer_);
            peer_ = nullptr;
            connected_ = false;
            return false;
        }

        connected_ = true;
        reconnect_pending_ = false;
        reconnect_attempts_ = 0;
        explicit_disconnect_requested_ = false;
        return true;
    }

    void scheduleNextReconnectAttempt() {
        const uint32_t attempt_number = reconnect_attempts_ + 1;
        const uint32_t delay_ms = ReconnectBackoffForAttempt(connect_options_, attempt_number);
        next_reconnect_at_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay_ms);
    }

    void pollReconnect(std::vector<ClientTransportEvent>* staged_events) {
        if (!reconnect_pending_ || connected_ || !staged_events) {
            return;
        }
        if (!canReconnect()) {
            reconnect_pending_ = false;
            reconnect_attempts_ = 0;
            ClientTransportEvent disconnected_event{};
            disconnected_event.type = ClientTransportEventType::Disconnected;
            staged_events->push_back(std::move(disconnected_event));
            return;
        }

        if (reconnect_attempts_ >= connect_options_.reconnect_max_attempts) {
            reconnect_pending_ = false;
            reconnect_attempts_ = 0;
            ClientTransportEvent disconnected_event{};
            disconnected_event.type = ClientTransportEventType::Disconnected;
            staged_events->push_back(std::move(disconnected_event));
            return;
        }

        if (std::chrono::steady_clock::now() < next_reconnect_at_) {
            return;
        }

        ++reconnect_attempts_;
        const uint32_t timeout_ms = connect_options_.reconnect_timeout_ms > 0
            ? connect_options_.reconnect_timeout_ms
            : connect_options_.timeout_ms;
        if (attemptConnect(connect_options_, timeout_ms)) {
            ClientTransportEvent connected_event{};
            connected_event.type = ClientTransportEventType::Connected;
            staged_events->push_back(std::move(connected_event));
            return;
        }

        if (reconnect_attempts_ >= connect_options_.reconnect_max_attempts) {
            reconnect_pending_ = false;
            reconnect_attempts_ = 0;
            ClientTransportEvent disconnected_event{};
            disconnected_event.type = ClientTransportEventType::Disconnected;
            staged_events->push_back(std::move(disconnected_event));
            return;
        }

        scheduleNextReconnectAttempt();
    }

    EnetGlobalGuard global_{};
    ENetHost* host_ = nullptr;
    ENetPeer* peer_ = nullptr;
    size_t channel_count_ = 2;
    bool ready_ = false;
    bool connected_ = false;
    ClientTransportConnectOptions connect_options_{};
    bool has_connect_options_ = false;
    bool reconnect_pending_ = false;
    uint32_t reconnect_attempts_ = 0;
    std::chrono::steady_clock::time_point next_reconnect_at_{};
    bool explicit_disconnect_requested_ = false;
};

std::unique_ptr<ClientTransport> CreateRegisteredClientTransport(
    std::string_view backend_name,
    const ClientTransportConfig& config) {
    ClientTransportFactory factory{};
    {
        std::lock_guard<std::mutex> lock(ClientTransportFactoriesMutex());
        const auto it = ClientTransportFactories().find(std::string(backend_name));
        if (it == ClientTransportFactories().end()) {
            return nullptr;
        }
        factory = it->second;
    }
    if (!factory) {
        return nullptr;
    }
    return factory(config);
}

bool IsClientTransportFactoryRegistered(std::string_view backend_name) {
    std::lock_guard<std::mutex> lock(ClientTransportFactoriesMutex());
    const auto it = ClientTransportFactories().find(std::string(backend_name));
    return it != ClientTransportFactories().end() && static_cast<bool>(it->second);
}

void EnsureDefaultClientTransportFactoriesRegistered() {
    static const bool registered = []() {
        RegisterClientTransportFactory(
            "enet",
            [](const ClientTransportConfig& config) -> std::unique_ptr<ClientTransport> {
                auto transport = std::make_unique<EnetClientTransport>(config);
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

const char* ClientTransportBackendName(ClientTransportBackend backend) {
    switch (backend) {
        case ClientTransportBackend::Auto: return "auto";
        case ClientTransportBackend::Enet: return "enet";
        default: return "unknown";
    }
}

std::optional<ClientTransportBackend> ParseClientTransportBackend(std::string_view name) {
    const std::string lowered = Lower(name);
    if (lowered.empty() || lowered == "auto") {
        return ClientTransportBackend::Auto;
    }
    if (lowered == "enet") {
        return ClientTransportBackend::Enet;
    }
    return std::nullopt;
}

bool RegisterClientTransportFactory(std::string_view backend_name, ClientTransportFactory factory) {
    const std::string lowered = Lower(backend_name);
    if (lowered.empty() || !factory) {
        return false;
    }

    std::lock_guard<std::mutex> lock(ClientTransportFactoriesMutex());
    ClientTransportFactories()[lowered] = std::move(factory);
    return true;
}

std::unique_ptr<ClientTransport> CreateClientTransport(const ClientTransportConfig& config) {
    EnsureDefaultClientTransportFactoriesRegistered();

    std::string backend_name = config.backend_name.empty()
        ? std::string(ClientTransportBackendName(config.backend))
        : Lower(config.backend_name);
    if (backend_name.empty() || backend_name == "auto") {
        backend_name = "enet";
    }

    const bool backend_registered = IsClientTransportFactoryRegistered(backend_name);
    auto transport = CreateRegisteredClientTransport(backend_name, config);
    if (transport && transport->isReady()) {
        return transport;
    }
    if (!backend_registered) {
        spdlog::warn("Network transport: unregistered client transport backend='{}'", backend_name);
    }
    return nullptr;
}

} // namespace karma::network

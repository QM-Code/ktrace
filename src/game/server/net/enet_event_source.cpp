#include "server/net/enet_event_source.hpp"

#include "karma/common/config_helpers.hpp"
#include "karma/common/logging.hpp"
#include "net/protocol_codec.hpp"
#include "net/protocol.hpp"

#include <enet.h>
#include <spdlog/spdlog.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace bz3::server::net {

namespace {

struct ClientConnectionState {
    uint32_t client_id = 0;
    bool joined = false;
    std::string player_name{};
};

class EnetGlobal {
 public:
    EnetGlobal() {
        std::lock_guard<std::mutex> lock(mutex());
        if (refCount()++ == 0) {
            if (enet_initialize() != 0) {
                spdlog::error("ENet: enet_initialize() failed");
                initialized_ = false;
                return;
            }
        }
        initialized_ = true;
    }

    ~EnetGlobal() {
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

std::string PeerIpString(const ENetAddress& address) {
    std::array<char, 128> ip_buffer{};
    if (enet_address_get_host_ip(&address, ip_buffer.data(), ip_buffer.size()) == 0) {
        return std::string(ip_buffer.data());
    }
    return "unknown";
}

std::string DefaultPlayerName(uint32_t client_id) {
    return "player-" + std::to_string(client_id);
}

std::string ResolvePlayerName(const bz3::net::ClientMessage& message, uint32_t client_id) {
    if (!message.player_name.empty()) {
        return message.player_name;
    }
    return DefaultPlayerName(client_id);
}

class EnetServerEventSource final : public ServerEventSource {
 public:
    explicit EnetServerEventSource(uint16_t port) : port_(port) {
        if (!global_.initialized()) {
            spdlog::error("ENet event source: ENet global initialization failed");
            return;
        }

        ENetAddress address{};
        address.host = ENET_HOST_ANY;
        address.port = port_;
        host_ = enet_host_create(&address, kMaxClients, kNumChannels, 0, 0);
        if (!host_) {
            spdlog::error("ENet event source: failed to create host on port {}", port_);
            return;
        }
        initialized_ = true;
        KARMA_TRACE("engine.server",
                    "ENet event source: listening on port {} (max_clients={} channels={})",
                    port_,
                    kMaxClients,
                    kNumChannels);
    }

    ~EnetServerEventSource() override {
        if (host_) {
            enet_host_destroy(host_);
            host_ = nullptr;
        }
    }

    bool initialized() const { return initialized_; }

    std::vector<ServerInputEvent> poll() override {
        std::vector<ServerInputEvent> out;
        if (!host_) {
            return out;
        }

        ENetEvent event{};
        while (enet_host_service(host_, &event, 0) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT: {
                    const uint32_t client_id = allocateClientId();
                    client_by_peer_[event.peer] = ClientConnectionState{client_id, false, {}};
                    KARMA_TRACE("engine.server",
                                "ServerEventSource: ENet connect client_id={} ip={} port={} (awaiting join packet)",
                                client_id,
                                PeerIpString(event.peer->address),
                                event.peer->address.port);
                    KARMA_TRACE("net.server",
                                "ENet connect client_id={} ip={} port={}",
                                client_id,
                                PeerIpString(event.peer->address),
                                event.peer->address.port);
                    break;
                }
                case ENET_EVENT_TYPE_DISCONNECT:
                case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT: {
                    const auto it = client_by_peer_.find(event.peer);
                    if (it == client_by_peer_.end()) {
                        break;
                    }
                    const ClientConnectionState state = it->second;
                    client_by_peer_.erase(it);
                    KARMA_TRACE("engine.server",
                                "ServerEventSource: ENet disconnect client_id={} ip={} port={} joined={}",
                                state.client_id,
                                PeerIpString(event.peer->address),
                                event.peer->address.port,
                                state.joined ? 1 : 0);
                    KARMA_TRACE("net.server",
                                "ENet disconnect client_id={} ip={} port={} joined={}",
                                state.client_id,
                                PeerIpString(event.peer->address),
                                event.peer->address.port,
                                state.joined ? 1 : 0);
                    if (state.joined) {
                        ServerInputEvent input{};
                        input.type = ServerInputEvent::Type::ClientLeave;
                        input.leave.client_id = state.client_id;
                        out.push_back(std::move(input));
                    }
                    break;
                }
                case ENET_EVENT_TYPE_RECEIVE: {
                    handleReceiveEvent(event, out);
                    enet_packet_destroy(event.packet);
                    break;
                }
                default:
                    break;
            }
        }

        return out;
    }

    void onJoinResult(uint32_t client_id,
                      bool accepted,
                      std::string_view reason,
                      std::string_view world_name,
                      const std::vector<SessionSnapshotEntry>& sessions) override {
        ENetPeer* peer = findPeerByClientId(client_id);
        if (!peer) {
            KARMA_TRACE("engine.server",
                        "ServerEventSource: join result dropped client_id={} accepted={} (peer missing)",
                        client_id,
                        accepted ? 1 : 0);
            return;
        }

        if (accepted) {
            static_cast<void>(sendJoinResponse(*peer, true, ""));
            static_cast<void>(sendInit(*peer, client_id, world_name));
            const bool snapshot_sent = sendSessionSnapshot(*peer, sessions);
            KARMA_TRACE("net.server",
                        "Session snapshot {} client_id={} world='{}' sessions={}",
                        snapshot_sent ? "sent" : "send-failed",
                        client_id,
                        world_name,
                        sessions.size());
            KARMA_TRACE("engine.server",
                        "ServerEventSource: join accepted client_id={} world='{}' snapshot_sessions={}",
                        client_id,
                        world_name,
                        sessions.size());
            return;
        }

        static_cast<void>(sendJoinResponse(*peer, false, reason));
        auto it = client_by_peer_.find(peer);
        if (it != client_by_peer_.end()) {
            it->second.joined = false;
        }
        enet_peer_disconnect(peer, 0);
        KARMA_TRACE("engine.server",
                    "ServerEventSource: join rejected client_id={} reason='{}'",
                    client_id,
                    reason);
    }

 private:
    void emitJoinEvent(uint32_t client_id, const std::string& player_name, std::vector<ServerInputEvent>& out) {
        ServerInputEvent input{};
        input.type = ServerInputEvent::Type::ClientJoin;
        input.join.client_id = client_id;
        input.join.player_name = player_name;
        out.push_back(std::move(input));
    }

    void emitLeaveEvent(uint32_t client_id, std::vector<ServerInputEvent>& out) {
        ServerInputEvent input{};
        input.type = ServerInputEvent::Type::ClientLeave;
        input.leave.client_id = client_id;
        out.push_back(std::move(input));
    }

    void handleReceiveEvent(const ENetEvent& event, std::vector<ServerInputEvent>& out) {
        const auto peer_it = client_by_peer_.find(event.peer);
        if (peer_it == client_by_peer_.end()) {
            KARMA_TRACE("engine.server",
                        "ServerEventSource: ENet receive from unknown peer ip={} port={} bytes={}",
                        PeerIpString(event.peer->address),
                        event.peer->address.port,
                        event.packet ? event.packet->dataLength : 0);
            return;
        }

        ClientConnectionState& state = peer_it->second;
        if (!event.packet || !event.packet->data || event.packet->dataLength == 0) {
            KARMA_TRACE("engine.server",
                        "ServerEventSource: ENet receive empty payload client_id={}",
                        state.client_id);
            return;
        }

        const auto decoded = bz3::net::DecodeClientMessage(event.packet->data, event.packet->dataLength);
        if (!decoded.has_value()) {
            KARMA_TRACE("engine.server",
                        "ServerEventSource: ENet receive invalid protobuf client_id={} bytes={}",
                        state.client_id,
                        event.packet->dataLength);
            return;
        }

        switch (decoded->type) {
            case bz3::net::ClientMessageType::JoinRequest: {
                const std::string player_name = ResolvePlayerName(*decoded, state.client_id);
                const uint32_t protocol_version = decoded->protocol_version;
                KARMA_TRACE("net.server",
                            "Handshake request client_id={} name='{}' protocol={} ip={} port={}",
                            state.client_id,
                            player_name,
                            protocol_version,
                            PeerIpString(event.peer->address),
                            event.peer->address.port);
                if (protocol_version != bz3::net::kProtocolVersion) {
                    static_cast<void>(
                        sendJoinResponse(*event.peer, false, "Protocol version mismatch."));
                    KARMA_TRACE("engine.server",
                                "ServerEventSource: ENet join rejected client_id={} name='{}' protocol={} expected={}",
                                state.client_id,
                                player_name,
                                protocol_version,
                                bz3::net::kProtocolVersion);
                    enet_peer_disconnect(event.peer, 0);
                    return;
                }
                if (state.joined) {
                    KARMA_TRACE("engine.server",
                                "ServerEventSource: ENet duplicate join client_id={} name='{}' ignored",
                                state.client_id,
                                player_name);
                    return;
                }

                state.joined = true;
                state.player_name = player_name;
                emitJoinEvent(state.client_id, state.player_name, out);
                KARMA_TRACE("engine.server",
                            "ServerEventSource: ENet join client_id={} name='{}' protocol={} ip={} port={}",
                            state.client_id,
                            state.player_name,
                            protocol_version,
                            PeerIpString(event.peer->address),
                            event.peer->address.port);
                return;
            }
            case bz3::net::ClientMessageType::PlayerLeave: {
                if (!state.joined) {
                    KARMA_TRACE("engine.server",
                                "ServerEventSource: ENet leave before join client_id={} ignored",
                                state.client_id);
                    return;
                }

                state.joined = false;
                emitLeaveEvent(state.client_id, out);
                KARMA_TRACE("engine.server",
                            "ServerEventSource: ENet leave client_id={} name='{}' ip={} port={}",
                            state.client_id,
                            state.player_name,
                            PeerIpString(event.peer->address),
                            event.peer->address.port);
                return;
            }
            default:
                KARMA_TRACE("engine.server",
                            "ServerEventSource: ENet payload ignored client_id={} bytes={}",
                            state.client_id,
                            event.packet->dataLength);
                return;
        }
    }

    ENetPeer* findPeerByClientId(uint32_t client_id) {
        for (auto& [peer, state] : client_by_peer_) {
            if (state.client_id == client_id) {
                return peer;
            }
        }
        return nullptr;
    }

    bool sendServerPayload(ENetPeer& peer, const std::vector<std::byte>& payload) {
        if (payload.empty()) {
            return false;
        }

        ENetPacket* packet =
            enet_packet_create(payload.data(), payload.size(), ENET_PACKET_FLAG_RELIABLE);
        if (!packet) {
            return false;
        }

        if (enet_peer_send(&peer, 0, packet) != 0) {
            enet_packet_destroy(packet);
            return false;
        }
        enet_host_flush(host_);
        return true;
    }

    bool sendJoinResponse(ENetPeer& peer, bool accepted, std::string_view reason) {
        return sendServerPayload(peer, bz3::net::EncodeServerJoinResponse(accepted, reason));
    }

    bool sendInit(ENetPeer& peer, uint32_t client_id, std::string_view world_name) {
        return sendServerPayload(
            peer,
            bz3::net::EncodeServerInit(
                client_id,
                karma::config::ReadStringConfig({"serverName"}, std::string("bz3-server")),
                world_name,
                bz3::net::kProtocolVersion));
    }

    bool sendSessionSnapshot(ENetPeer& peer, const std::vector<SessionSnapshotEntry>& sessions) {
        std::vector<bz3::net::SessionSnapshotEntry> wire_sessions{};
        wire_sessions.reserve(sessions.size());
        for (const auto& session : sessions) {
            wire_sessions.push_back(bz3::net::SessionSnapshotEntry{
                session.session_id,
                session.session_name});
        }
        return sendServerPayload(peer, bz3::net::EncodeServerSessionSnapshot(wire_sessions));
    }

    uint32_t allocateClientId() {
        while (isClientIdInUse(next_client_id_)) {
            ++next_client_id_;
        }
        return next_client_id_++;
    }

    bool isClientIdInUse(uint32_t client_id) const {
        for (const auto& [_, state] : client_by_peer_) {
            if (state.client_id == client_id) {
                return true;
            }
        }
        return false;
    }

    static constexpr size_t kMaxClients = 50;
    static constexpr size_t kNumChannels = 2;
    static constexpr uint32_t kFirstClientId = 2;

    EnetGlobal global_{};
    ENetHost* host_ = nullptr;
    uint16_t port_ = 0;
    bool initialized_ = false;
    uint32_t next_client_id_ = kFirstClientId;
    std::unordered_map<ENetPeer*, ClientConnectionState> client_by_peer_{};
};

} // namespace

std::unique_ptr<ServerEventSource> CreateEnetServerEventSource(uint16_t port) {
    auto enet = std::make_unique<EnetServerEventSource>(port);
    if (!enet->initialized()) {
        return nullptr;
    }
    return enet;
}

} // namespace bz3::server::net

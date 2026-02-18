#include "server/net/transport_event_source/internal.hpp"

#include "karma/common/logging/logging.hpp"
#include "net/protocol.hpp"
#include "net/protocol_codec.hpp"

namespace bz3::server::net::detail {

void TransportServerEventSource::emitJoinEvent(uint32_t client_id,
                                               const std::string& player_name,
                                               const std::string& auth_payload,
                                               const std::string& peer_ip,
                                               uint16_t peer_port,
                                               std::vector<ServerInputEvent>& out) {
    ServerInputEvent input{};
    input.type = ServerInputEvent::Type::ClientJoin;
    input.join.client_id = client_id;
    input.join.player_name = player_name;
    input.join.auth_payload = auth_payload;
    input.join.peer_ip = peer_ip;
    input.join.peer_port = peer_port;
    out.push_back(std::move(input));
}

void TransportServerEventSource::emitLeaveEvent(uint32_t client_id, std::vector<ServerInputEvent>& out) {
    ServerInputEvent input{};
    input.type = ServerInputEvent::Type::ClientLeave;
    input.leave.client_id = client_id;
    out.push_back(std::move(input));
}

void TransportServerEventSource::emitRequestSpawnEvent(uint32_t client_id,
                                                       std::vector<ServerInputEvent>& out) {
    ServerInputEvent input{};
    input.type = ServerInputEvent::Type::ClientRequestSpawn;
    input.request_spawn.client_id = client_id;
    out.push_back(std::move(input));
}

void TransportServerEventSource::emitCreateShotEvent(uint32_t client_id,
                                                     uint32_t local_shot_id,
                                                     float pos_x,
                                                     float pos_y,
                                                     float pos_z,
                                                     float vel_x,
                                                     float vel_y,
                                                     float vel_z,
                                                     std::vector<ServerInputEvent>& out) {
    ServerInputEvent input{};
    input.type = ServerInputEvent::Type::ClientCreateShot;
    input.create_shot.client_id = client_id;
    input.create_shot.local_shot_id = local_shot_id;
    input.create_shot.pos_x = pos_x;
    input.create_shot.pos_y = pos_y;
    input.create_shot.pos_z = pos_z;
    input.create_shot.vel_x = vel_x;
    input.create_shot.vel_y = vel_y;
    input.create_shot.vel_z = vel_z;
    out.push_back(std::move(input));
}

void TransportServerEventSource::handleReceiveEvent(const karma::network::ServerTransportEvent& event,
                                                    std::vector<ServerInputEvent>& out) {
    const auto peer_it = client_by_peer_.find(event.peer);
    if (peer_it == client_by_peer_.end()) {
        KARMA_TRACE("engine.server",
                    "ServerEventSource: transport receive from unknown peer ip={} port={} bytes={}",
                    event.peer_ip,
                    event.peer_port,
                    event.payload.size());
        return;
    }

    ClientConnectionState& state = peer_it->second;
    if (!event.peer_ip.empty()) {
        state.peer_ip = event.peer_ip;
    }
    if (event.peer_port != 0) {
        state.peer_port = event.peer_port;
    }

    if (event.payload.empty()) {
        KARMA_TRACE("engine.server",
                    "ServerEventSource: transport receive empty payload client_id={}",
                    state.client_id);
        return;
    }

    const auto decoded = bz3::net::DecodeClientMessage(event.payload.data(), event.payload.size());
    if (!decoded.has_value()) {
        KARMA_TRACE("engine.server",
                    "ServerEventSource: transport receive invalid protobuf client_id={} bytes={}",
                    state.client_id,
                    event.payload.size());
        return;
    }

    switch (decoded->type) {
        case bz3::net::ClientMessageType::JoinRequest: {
            const std::string player_name = ResolvePlayerName(*decoded, state.client_id);
            const uint32_t protocol_version = decoded->protocol_version;
            state.cached_world_hash = decoded->cached_world_hash;
            state.cached_world_id = decoded->cached_world_id;
            state.cached_world_revision = decoded->cached_world_revision;
            state.cached_world_content_hash = decoded->cached_world_content_hash;
            state.cached_world_manifest_hash = decoded->cached_world_manifest_hash;
            state.cached_world_manifest_file_count = decoded->cached_world_manifest_file_count;
            state.cached_world_manifest.clear();
            state.cached_world_manifest.reserve(decoded->cached_world_manifest.size());
            for (const auto& entry : decoded->cached_world_manifest) {
                state.cached_world_manifest.push_back(WorldManifestEntry{
                    entry.path,
                    entry.size,
                    entry.hash});
            }
            KARMA_TRACE("net.server",
                        "Handshake request client_id={} name='{}' protocol={} auth_payload_present={} cached_world_hash='{}' cached_world_id='{}' cached_world_revision='{}' cached_world_content_hash='{}' cached_world_manifest_hash='{}' cached_world_manifest_files={} cached_world_manifest_entries={} ip={} port={}",
                        state.client_id,
                        player_name,
                        protocol_version,
                        decoded->auth_payload.empty() ? 0 : 1,
                        state.cached_world_hash.empty() ? "-" : state.cached_world_hash,
                        state.cached_world_id.empty() ? "-" : state.cached_world_id,
                        state.cached_world_revision.empty() ? "-" : state.cached_world_revision,
                        state.cached_world_content_hash.empty() ? "-" : state.cached_world_content_hash,
                        state.cached_world_manifest_hash.empty() ? "-" : state.cached_world_manifest_hash,
                        state.cached_world_manifest_file_count,
                        state.cached_world_manifest.size(),
                        state.peer_ip,
                        state.peer_port);
            if (protocol_version != bz3::net::kProtocolVersion) {
                static_cast<void>(sendJoinResponse(state.peer, false, "Protocol version mismatch."));
                KARMA_TRACE("engine.server",
                            "ServerEventSource: transport join rejected client_id={} name='{}' protocol={} expected={}",
                            state.client_id,
                            player_name,
                            protocol_version,
                            bz3::net::kProtocolVersion);
                transport_->disconnect(state.peer, 0);
                return;
            }
            if (state.joined) {
                KARMA_TRACE("engine.server",
                            "ServerEventSource: transport duplicate join client_id={} name='{}' ignored",
                            state.client_id,
                            player_name);
                return;
            }

            state.joined = true;
            state.player_name = player_name;
            emitJoinEvent(state.client_id,
                          state.player_name,
                          decoded->auth_payload,
                          state.peer_ip,
                          state.peer_port,
                          out);
            KARMA_TRACE("engine.server",
                        "ServerEventSource: transport join client_id={} name='{}' protocol={} auth_payload_present={} ip={} port={}",
                        state.client_id,
                        state.player_name,
                        protocol_version,
                        decoded->auth_payload.empty() ? 0 : 1,
                        state.peer_ip,
                        state.peer_port);
            return;
        }
        case bz3::net::ClientMessageType::PlayerLeave: {
            if (!state.joined) {
                KARMA_TRACE("engine.server",
                            "ServerEventSource: transport leave before join client_id={} ignored",
                            state.client_id);
                return;
            }

            state.joined = false;
            emitLeaveEvent(state.client_id, out);
            KARMA_TRACE("engine.server",
                        "ServerEventSource: transport leave client_id={} name='{}' ip={} port={}",
                        state.client_id,
                        state.player_name,
                        state.peer_ip,
                        state.peer_port);
            return;
        }
        case bz3::net::ClientMessageType::RequestPlayerSpawn: {
            if (!state.joined) {
                KARMA_TRACE("engine.server",
                            "ServerEventSource: transport request_spawn before join client_id={} ignored",
                            state.client_id);
                return;
            }

            emitRequestSpawnEvent(state.client_id, out);
            KARMA_TRACE("engine.server",
                        "ServerEventSource: transport request_spawn client_id={} name='{}' ip={} port={}",
                        state.client_id,
                        state.player_name,
                        state.peer_ip,
                        state.peer_port);
            return;
        }
        case bz3::net::ClientMessageType::CreateShot: {
            if (!state.joined) {
                KARMA_TRACE("engine.server",
                            "ServerEventSource: transport create_shot before join client_id={} ignored",
                            state.client_id);
                return;
            }

            emitCreateShotEvent(state.client_id,
                                decoded->local_shot_id,
                                decoded->shot_position.x,
                                decoded->shot_position.y,
                                decoded->shot_position.z,
                                decoded->shot_velocity.x,
                                decoded->shot_velocity.y,
                                decoded->shot_velocity.z,
                                out);
            KARMA_TRACE("engine.server",
                        "ServerEventSource: transport create_shot client_id={} local_shot_id={} ip={} port={}",
                        state.client_id,
                        decoded->local_shot_id,
                        state.peer_ip,
                        state.peer_port);
            return;
        }
        default:
            KARMA_TRACE("engine.server",
                        "ServerEventSource: transport payload ignored client_id={} bytes={}",
                        state.client_id,
                        event.payload.size());
            return;
    }
}

} // namespace bz3::server::net::detail

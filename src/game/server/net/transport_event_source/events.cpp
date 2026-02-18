#include "server/net/transport_event_source/internal.hpp"

#include "karma/common/logging/logging.hpp"

namespace bz3::server::net::detail {

void TransportServerEventSource::onPlayerSpawn(uint32_t client_id) {
    const size_t sent = broadcastToJoined([client_id, this](karma::network::PeerToken peer) {
        return sendPlayerSpawn(peer, client_id);
    });
    KARMA_TRACE("net.server",
                "ServerEventSource: broadcast player_spawn client_id={} peers={}",
                client_id,
                sent);
}

void TransportServerEventSource::onPlayerDeath(uint32_t client_id) {
    const size_t sent = broadcastToJoined([client_id, this](karma::network::PeerToken peer) {
        return sendPlayerDeath(peer, client_id);
    });
    KARMA_TRACE("net.server",
                "ServerEventSource: broadcast player_death client_id={} peers={}",
                client_id,
                sent);
}

void TransportServerEventSource::onCreateShot(uint32_t source_client_id,
                                              uint32_t global_shot_id,
                                              float pos_x,
                                              float pos_y,
                                              float pos_z,
                                              float vel_x,
                                              float vel_y,
                                              float vel_z) {
    const size_t sent = broadcastToJoined([=, this](karma::network::PeerToken peer) {
        return sendCreateShot(peer,
                              source_client_id,
                              global_shot_id,
                              pos_x,
                              pos_y,
                              pos_z,
                              vel_x,
                              vel_y,
                              vel_z);
    });
    KARMA_TRACE("net.server",
                "ServerEventSource: broadcast create_shot source_client_id={} global_shot_id={} peers={}",
                source_client_id,
                global_shot_id,
                sent);
}

void TransportServerEventSource::onRemoveShot(uint32_t shot_id, bool is_global_id) {
    const size_t sent = broadcastToJoined([shot_id, is_global_id, this](karma::network::PeerToken peer) {
        return sendRemoveShot(peer, shot_id, is_global_id);
    });
    KARMA_TRACE("net.server",
                "ServerEventSource: broadcast remove_shot shot_id={} global={} peers={}",
                shot_id,
                is_global_id ? 1 : 0,
                sent);
}

} // namespace bz3::server::net::detail

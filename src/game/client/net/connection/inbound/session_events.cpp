#include "client/net/connection.hpp"

#include "karma/common/logging/logging.hpp"

namespace bz3::client::net {

void ClientConnection::handleSessionSnapshot(const bz3::net::ServerMessage& message) {
    KARMA_TRACE("net.client",
                "ClientConnection: snapshot sessions={}",
                message.sessions.size());
    for (const auto& session : message.sessions) {
        KARMA_TRACE("net.client",
                    "ClientConnection: snapshot session id={} name='{}'",
                    session.session_id,
                    session.session_name);
    }

    if (init_received_ && !join_bootstrap_complete_logged_ && !pending_world_package_.active &&
        !active_world_transfer_.active) {
        join_bootstrap_complete_logged_ = true;
        KARMA_TRACE("net.client",
                    "ClientConnection: join bootstrap complete client_id={} world='{}' server='{}' sessions={}",
                    assigned_client_id_,
                    init_world_name_,
                    init_server_name_,
                    message.sessions.size());
        if (audio_event_callback_) {
            audio_event_callback_(AudioEvent::PlayerSpawn);
        }
    }
}

void ClientConnection::handlePlayerSpawn(const bz3::net::ServerMessage& message) {
    KARMA_TRACE("net.client",
                "ClientConnection: player spawn client_id={}",
                message.event_client_id);
    if (audio_event_callback_) {
        audio_event_callback_(AudioEvent::PlayerSpawn);
    }
}

void ClientConnection::handlePlayerDeath(const bz3::net::ServerMessage& message) {
    KARMA_TRACE("net.client",
                "ClientConnection: player death client_id={}",
                message.event_client_id);
    if (audio_event_callback_) {
        audio_event_callback_(AudioEvent::PlayerDeath);
    }
}

void ClientConnection::handleCreateShot(const bz3::net::ServerMessage& message) {
    KARMA_TRACE("net.client",
                "ClientConnection: create shot id={} source_client_id={}",
                message.event_shot_id,
                message.event_client_id);
    const bool is_local_echo =
        (assigned_client_id_ != 0 && message.event_client_id == assigned_client_id_);
    if (audio_event_callback_ && !is_local_echo) {
        audio_event_callback_(AudioEvent::ShotFire);
    }
}

void ClientConnection::handleRemoveShot(const bz3::net::ServerMessage& message) {
    KARMA_TRACE("net.client",
                "ClientConnection: remove shot id={} global={}",
                message.event_shot_id,
                message.event_shot_is_global ? 1 : 0);
}

} // namespace bz3::client::net

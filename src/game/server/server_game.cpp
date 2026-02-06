#include "server/server_game.hpp"

#include <optional>
#include <utility>

#include "karma/common/logging.hpp"
#include "karma/ecs/world.hpp"
#include "server/domain/components.hpp"

namespace bz3::server {

ServerGame::ServerGame(std::string worldName) : world_name_(std::move(worldName)) {}

const std::string& ServerGame::lastJoinRejectReason() const {
    return last_join_reject_reason_;
}

std::vector<SessionSnapshot> ServerGame::activeSessionSnapshot() const {
    std::vector<SessionSnapshot> out;
    if (!world) {
        return out;
    }

    for (const auto session_entity : session_system_.activeSessions(*world)) {
        const auto* session = world->tryGet<domain::SessionComponent>(session_entity);
        if (!session) {
            continue;
        }
        out.push_back(SessionSnapshot{session->session_id, session->session_name});
    }
    return out;
}

std::optional<uint32_t> ServerGame::connectSession(std::string_view session_name) {
    if (!world) {
        return std::nullopt;
    }

    const auto session_entity = session_system_.createSession(*world, session_name);
    const auto* session = world->tryGet<domain::SessionComponent>(session_entity);
    if (!session) {
        return std::nullopt;
    }

    actor_system_.spawnActorForSession(*world, session_entity);
    KARMA_TRACE("engine.server",
                "ServerGame: session connected id={} name='{}' actors={} world_entities={}",
                session->session_id,
                session->session_name,
                actor_system_.aliveActorCount(*world),
                world->entities().size());
    return session->session_id;
}

bool ServerGame::disconnectSession(uint32_t session_id) {
    if (!world) {
        return false;
    }

    const auto session_entity = session_system_.findSessionById(*world, session_id);
    if (!session_entity.has_value()) {
        return false;
    }

    const auto* session = world->tryGet<domain::SessionComponent>(*session_entity);
    const std::string session_name = session ? session->session_name : std::string{};

    actor_system_.destroyActorsForSession(*world, *session_entity);
    session_system_.disconnectSession(*world, *session_entity);
    session_system_.destroySession(*world, *session_entity);
    KARMA_TRACE("engine.server",
                "ServerGame: session disconnected id={} name='{}' actors={} world_entities={}",
                session_id,
                session_name,
                actor_system_.aliveActorCount(*world),
                world->entities().size());
    return true;
}

bool ServerGame::onClientJoin(uint32_t client_id, std::string_view player_name) {
    if (session_by_client_id_.contains(client_id)) {
        last_join_reject_reason_ = "Client already joined.";
        KARMA_TRACE("engine.server",
                    "ServerGame: ignoring duplicate client join id={} name='{}'",
                    client_id,
                    player_name);
        KARMA_TRACE("net.server",
                    "ServerGame: client join rejected client_id={} reason='{}'",
                    client_id,
                    last_join_reject_reason_);
        return false;
    }

    const std::string resolved_player_name = player_name.empty()
        ? ("player-" + std::to_string(client_id))
        : std::string(player_name);

    if (hasActiveSessionName(resolved_player_name)) {
        last_join_reject_reason_ =
            "Player name '" + resolved_player_name + "' is already in use.";
        KARMA_TRACE("engine.server",
                    "ServerGame: client join rejected client_id={} name='{}' reason='{}'",
                    client_id,
                    resolved_player_name,
                    last_join_reject_reason_);
        KARMA_TRACE("net.server",
                    "ServerGame: client join rejected client_id={} name='{}' reason='{}'",
                    client_id,
                    resolved_player_name,
                    last_join_reject_reason_);
        return false;
    }

    const auto session_id = connectSession(resolved_player_name);
    if (!session_id.has_value()) {
        last_join_reject_reason_ = "Server failed to create session.";
        KARMA_TRACE("net.server",
                    "ServerGame: client join rejected client_id={} name='{}' reason='{}'",
                    client_id,
                    resolved_player_name,
                    last_join_reject_reason_);
        return false;
    }

    last_join_reject_reason_.clear();
    session_by_client_id_[client_id] = *session_id;
    KARMA_TRACE("engine.server",
                "ServerGame: client join mapped client_id={} name='{}' -> session_id={}",
                client_id,
                resolved_player_name,
                *session_id);
    KARMA_TRACE("net.server",
                "ServerGame: client connected client_id={} name='{}' session_id={}",
                client_id,
                resolved_player_name,
                *session_id);
    return true;
}

bool ServerGame::onClientLeave(uint32_t client_id) {
    const auto it = session_by_client_id_.find(client_id);
    if (it == session_by_client_id_.end()) {
        KARMA_TRACE("engine.server",
                    "ServerGame: ignoring client leave for unknown client_id={}",
                    client_id);
        return false;
    }

    const uint32_t session_id = it->second;
    session_by_client_id_.erase(it);
    const bool disconnected = disconnectSession(session_id);
    KARMA_TRACE("engine.server",
                "ServerGame: client leave client_id={} session_id={} result={}",
                client_id,
                session_id,
                disconnected ? "ok" : "missing");
    if (disconnected) {
        KARMA_TRACE("net.server",
                    "ServerGame: client disconnected client_id={} session_id={}",
                    client_id,
                    session_id);
    }
    return disconnected;
}

void ServerGame::onStart() {
    if (!world) {
        return;
    }

    match_system_.onStart(*world, world_name_);
    session_system_.onStart(*world);
    actor_system_.onStart(*world);
    session_by_client_id_.clear();

    KARMA_TRACE("engine.server",
                "ServerGame: onStart world='{}' sessions={} actors={} world_entities={}",
                world_name_,
                session_system_.activeSessionCount(*world),
                actor_system_.aliveActorCount(*world),
                world->entities().size());
}

void ServerGame::onTick(float dt) {
    if (!world) {
        return;
    }

    match_system_.onTick(*world, dt);
    session_system_.onTick(*world, dt);
    actor_system_.onTick(*world, dt);

    status_log_accumulator_ += dt;

    const domain::MatchStateComponent* state = match_system_.tryState(*world);
    if (!state) {
        return;
    }

    if (status_log_accumulator_ >= 1.0f) {
        status_log_accumulator_ = 0.0f;
        KARMA_TRACE("engine.server",
                    "ServerGame: tick={} uptime={:.2f}s sessions={} actors={} world_entities={}",
                    state->tick,
                    state->uptime_seconds,
                    session_system_.activeSessionCount(*world),
                    actor_system_.aliveActorCount(*world),
                    world->entities().size());
    }
}

void ServerGame::onShutdown() {
    if (!world) {
        return;
    }

    session_by_client_id_.clear();
    actor_system_.onShutdown(*world);
    session_system_.onShutdown(*world);
    match_system_.onShutdown(*world);

    KARMA_TRACE("engine.server",
                "ServerGame: onShutdown world='{}' world_entities={}",
                world_name_,
                world->entities().size());
}

bool ServerGame::hasActiveSessionName(std::string_view player_name) const {
    if (!world) {
        return false;
    }
    for (const auto session_entity : session_system_.activeSessions(*world)) {
        const auto* session = world->tryGet<domain::SessionComponent>(session_entity);
        if (!session) {
            continue;
        }
        if (session->session_name == player_name) {
            return true;
        }
    }
    return false;
}

} // namespace bz3::server

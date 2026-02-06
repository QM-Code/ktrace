#include "server/domain/session_system.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

#include "karma/ecs/world.hpp"

#include "server/domain/components.hpp"

namespace bz3::server::domain {

void SessionSystem::onStart(karma::ecs::World& world) {
    onShutdown(world);
}

void SessionSystem::onTick(karma::ecs::World& world, float dt) {
    for (const auto entity : session_entities_) {
        SessionComponent* session = world.tryGet<SessionComponent>(entity);
        if (!session || !session->connected) {
            continue;
        }
        session->connected_seconds += dt;
    }
}

void SessionSystem::onShutdown(karma::ecs::World& world) {
    for (const auto entity : session_entities_) {
        if (world.isAlive(entity)) {
            world.destroyEntity(entity);
        }
    }
    session_entities_.clear();
}

karma::ecs::Entity SessionSystem::createSession(karma::ecs::World& world, std::string_view session_name) {
    const auto entity = world.createEntity();

    SessionComponent session{};
    session.session_id = next_session_id_++;
    session.session_name = std::string(session_name);
    session.connected = true;
    session.connected_seconds = 0.0f;
    world.add(entity, std::move(session));

    session_entities_.push_back(entity);
    return entity;
}

void SessionSystem::disconnectSession(karma::ecs::World& world, karma::ecs::Entity session_entity) {
    SessionComponent* session = world.tryGet<SessionComponent>(session_entity);
    if (!session) {
        return;
    }
    session->connected = false;
}

void SessionSystem::destroySession(karma::ecs::World& world, karma::ecs::Entity session_entity) {
    session_entities_.erase(
        std::remove(session_entities_.begin(), session_entities_.end(), session_entity),
        session_entities_.end());
    if (world.isAlive(session_entity)) {
        world.destroyEntity(session_entity);
    }
}

std::optional<karma::ecs::Entity> SessionSystem::findSessionById(const karma::ecs::World& world,
                                                                  uint32_t session_id) const {
    for (const auto entity : session_entities_) {
        if (!world.isAlive(entity)) {
            continue;
        }
        const SessionComponent* session = world.tryGet<SessionComponent>(entity);
        if (session && session->session_id == session_id) {
            return entity;
        }
    }
    return std::nullopt;
}

size_t SessionSystem::sessionCount(const karma::ecs::World& world) const {
    size_t count = 0;
    for (const auto entity : session_entities_) {
        if (world.isAlive(entity) && world.has<SessionComponent>(entity)) {
            ++count;
        }
    }
    return count;
}

std::vector<karma::ecs::Entity> SessionSystem::activeSessions(const karma::ecs::World& world) const {
    std::vector<karma::ecs::Entity> active;
    active.reserve(session_entities_.size());
    for (const auto entity : session_entities_) {
        if (!world.isAlive(entity)) {
            continue;
        }
        const SessionComponent* session = world.tryGet<SessionComponent>(entity);
        if (!session || !session->connected) {
            continue;
        }
        active.push_back(entity);
    }
    return active;
}

size_t SessionSystem::activeSessionCount(const karma::ecs::World& world) const {
    return activeSessions(world).size();
}

} // namespace bz3::server::domain

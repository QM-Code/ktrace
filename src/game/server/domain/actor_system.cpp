#include "server/domain/actor_system.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "karma/ecs/world.hpp"

#include "server/domain/components.hpp"

namespace bz3::server::domain {

void ActorSystem::onStart(karma::ecs::World& world) {
    onShutdown(world);
    simulation_phase_ = 0.0f;
}

karma::ecs::Entity ActorSystem::spawnActorForSession(karma::ecs::World& world, karma::ecs::Entity session_entity) {
    if (!world.isAlive(session_entity) || !world.has<SessionComponent>(session_entity)) {
        return {};
    }

    const auto actor_entity = world.createEntity();
    const float spawn_x = -12.0f + (static_cast<float>(actor_entities_.size()) * 12.0f);

    ActorComponent actor{};
    actor.actor_id = next_actor_id_++;
    actor.owner_session = session_entity;
    actor.alive = true;
    actor.health = 100.0f;
    world.add(actor_entity, std::move(actor));

    ActorMotionComponent motion{};
    motion.position = glm::vec3{spawn_x, 0.0f, 0.0f};
    motion.velocity = glm::vec3{0.0f, 0.0f, 1.5f};
    world.add(actor_entity, motion);

    actor_entities_.push_back(actor_entity);
    return actor_entity;
}

void ActorSystem::destroyActor(karma::ecs::World& world, karma::ecs::Entity actor_entity) {
    actor_entities_.erase(
        std::remove(actor_entities_.begin(), actor_entities_.end(), actor_entity),
        actor_entities_.end());
    if (world.isAlive(actor_entity)) {
        world.destroyEntity(actor_entity);
    }
}

void ActorSystem::destroyActorsForSession(karma::ecs::World& world, karma::ecs::Entity session_entity) {
    std::vector<karma::ecs::Entity> to_destroy;
    to_destroy.reserve(actor_entities_.size());

    for (const auto entity : actor_entities_) {
        const ActorComponent* actor = world.tryGet<ActorComponent>(entity);
        if (actor && actor->owner_session == session_entity) {
            to_destroy.push_back(entity);
        }
    }

    for (const auto entity : to_destroy) {
        destroyActor(world, entity);
    }
}

void ActorSystem::onTick(karma::ecs::World& world, float dt) {
    simulation_phase_ += dt;

    for (const auto entity : actor_entities_) {
        ActorComponent* actor = world.tryGet<ActorComponent>(entity);
        ActorMotionComponent* motion = world.tryGet<ActorMotionComponent>(entity);
        if (!actor || !motion || !actor->alive) {
            continue;
        }

        motion->position += motion->velocity * dt;
        motion->position.x += std::sin(simulation_phase_ + static_cast<float>(actor->actor_id)) * (0.25f * dt);

        actor->health = std::max(0.0f, actor->health - (0.05f * dt));
        actor->alive = actor->health > 0.0f;
    }
}

void ActorSystem::onShutdown(karma::ecs::World& world) {
    for (const auto entity : actor_entities_) {
        if (world.isAlive(entity)) {
            world.destroyEntity(entity);
        }
    }
    actor_entities_.clear();
}

size_t ActorSystem::actorCount(const karma::ecs::World& world) const {
    size_t count = 0;
    for (const auto entity : actor_entities_) {
        if (world.isAlive(entity) && world.has<ActorComponent>(entity)) {
            ++count;
        }
    }
    return count;
}

size_t ActorSystem::aliveActorCount(const karma::ecs::World& world) const {
    size_t count = 0;
    for (const auto entity : actor_entities_) {
        const ActorComponent* actor = world.tryGet<ActorComponent>(entity);
        if (actor && actor->alive) {
            ++count;
        }
    }
    return count;
}

} // namespace bz3::server::domain

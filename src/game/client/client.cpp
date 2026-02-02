#include "client.hpp"
#include "karma/core/types.hpp"
#include "game/net/messages.hpp"
#include "client/game.hpp"
#include "karma/components/mesh.h"
#include "karma/components/transform.h"
#include "renderer/radar_components.hpp"
#include "spdlog/spdlog.h"
#include <glm/glm.hpp>

namespace components = karma::components;

void Client::syncRenderFromState() {
    if (ecsEntity.isValid() && game.engine.ecsWorld) {
        if (game.engine.ecsWorld->has<components::TransformComponent>(ecsEntity)) {
            auto &transform = game.engine.ecsWorld->get<components::TransformComponent>(ecsEntity);
            transform.position = state.position;
            transform.rotation = state.rotation;
            transform.scale = state.alive ? glm::vec3(1.0f) : glm::vec3(0.0f);
        }
        if (game.engine.ecsWorld->has<game::renderer::RadarCircle>(ecsEntity)) {
            game.engine.ecsWorld->get<game::renderer::RadarCircle>(ecsEntity).enabled = state.alive;
        }
    }
}

Client::Client(Game &game, client_id id, const PlayerState &initialState)
    : Actor(game, id),
      dieAudio(game.engine.audio->loadClip(game.world->resolveAssetPath("audio.player.Die").string(), 10)) {
    if (game.engine.ecsWorld) {
        ecsEntity = game.engine.ecsWorld->createEntity();
        components::TransformComponent xform{};
        xform.scale = glm::vec3(1.0f);
        game.engine.ecsWorld->add(ecsEntity, xform);
        components::MeshComponent mesh{};
        mesh.mesh_key = game.world->resolveAssetPath("playerModel").string();
        game.engine.ecsWorld->add(ecsEntity, mesh);
        game::renderer::RadarCircle circle{};
        circle.radius = 1.2f;
        game.engine.ecsWorld->add(ecsEntity, circle);
    }

    setState(initialState);
    justSpawned = state.alive;
    lastSpawnPosition = state.position;
    spdlog::trace("Client::Client: Initialized location for client id {}", id);
}

Client::~Client() {
    if (ecsEntity.isValid() && game.engine.ecsWorld) {
        game.engine.ecsWorld->destroyEntity(ecsEntity);
    }
}

void Client::update(TimeUtils::duration /*deltaTime*/) {
    syncRenderFromState();
}

void Client::setState(const PlayerState &newState) {
    state = newState;
}

void Client::die() {
    if (!state.alive) {
        return;
    }
    Actor::die();
    state.alive = false;
    dieAudio.play(state.position);
    spdlog::trace("Client::update: Client id {} has died", id);
}

void Client::spawn(glm::vec3 position, glm::quat rotation, glm::vec3 velocity) {
    setLocation(position, rotation, velocity);
    state.alive = true;
    justSpawned = true;
    lastSpawnPosition = state.position;
    syncRenderFromState();
    spdlog::trace("Client::update: Client id {} has spawned", id);
}

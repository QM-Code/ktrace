#include "shot.hpp"
#include "client/game.hpp"
#include "karma/components/mesh.h"
#include "karma/components/transform.h"
#include "karma_extras/ecs/render_components.h"
#include "renderer/radar_components.hpp"
#include "spdlog/spdlog.h"
#include <glm/gtc/quaternion.hpp>

namespace components = karma::components;
Shot::Shot(Game &game,
           shot_id id,
           bool isGlobalId,
           glm::vec3 position,
           glm::vec3 velocity)
    : game(game),
      id(id),
      isGlobalId(isGlobalId),
    position(position),
    prevPosition(position),
    velocity(velocity),
    audioEngine(*game.engine.audio),
    fireAudio(audioEngine.loadClip(game.world->resolveAssetPath("audio.shot.Fire").string(), 20)),
    ricochetAudio(audioEngine.loadClip(game.world->resolveAssetPath("audio.shot.Ricochet").string(), 20))
{
    if (game.engine.ecsWorld) {
        ecsEntity = game.engine.ecsWorld->createEntity();
        components::TransformComponent xform{};
        xform.position = position;
        xform.scale = glm::vec3(0.6f);
        game.engine.ecsWorld->add(ecsEntity, xform);
        components::MeshComponent mesh{};
        mesh.mesh_key = game.world->resolveAssetPath("shotModel").string();
        game.engine.ecsWorld->add(ecsEntity, mesh);
        game.engine.ecsWorld->add(ecsEntity, karma::ecs::Transparency{true});
        game::renderer::RadarCircle circle{};
        circle.radius = 0.5f;
        game.engine.ecsWorld->add(ecsEntity, circle);
        spdlog::info("Shot: ECS render enabled (shot_id={}, ecs_entity={})", id, ecsEntity.index);
    }

    fireAudio.play(position);
}

// Local id constructor
Shot::Shot(Game &game, glm::vec3 position, glm::vec3 velocity) : Shot(game, getNextLocalShotId(), false, position, velocity) {
    ClientMsg_CreateShot createShotMsg;
    createShotMsg.localShotId = id;
    createShotMsg.position = position;
    createShotMsg.velocity = velocity;
    game.engine.network->send<ClientMsg_CreateShot>(createShotMsg);
};

// Global id constructor
Shot::Shot(Game &game, shot_id globalId, glm::vec3 position, glm::vec3 velocity) : Shot(game, globalId, true, position, velocity) {};

Shot::~Shot() {
    if (ecsEntity.isValid() && game.engine.ecsWorld) {
        game.engine.ecsWorld->destroyEntity(ecsEntity);
    }
}

void Shot::update(TimeUtils::duration deltaTime) {
    // Cast across the full frame segment to avoid tunneling.
    const glm::vec3 start = position;
    const glm::vec3 end = position + velocity * deltaTime;

    glm::vec3 hitPoint, hitNormal;
    if (game.engine.physics->raycast(start, end, hitPoint, hitNormal)) {
        const float speed = glm::length(velocity);
        const glm::vec3 n = glm::normalize(hitNormal);
        const glm::vec3 dir = speed > 0.f ? glm::normalize(velocity) : velocity;

        // Snap to contact and nudge off the surface slightly to avoid re-hit next frame.
        constexpr float EPS = 1e-3f;
        position = hitPoint + n * EPS;
        velocity = glm::reflect(dir, n) * speed;

        ricochetAudio.play(hitPoint);
        spdlog::trace(
            "Shot::update: Shot {} ricocheted at point ({:.6f}, {:.6f}, {:.6f}) with normal ({:.6f}, {:.6f}, {:.6f})",
            id,
            hitPoint.x, hitPoint.y, hitPoint.z,
            hitNormal.x, hitNormal.y, hitNormal.z);
    } else {
        position = end;
    }

    if (ecsEntity.isValid() && game.engine.ecsWorld &&
        game.engine.ecsWorld->has<components::TransformComponent>(ecsEntity)) {
        game.engine.ecsWorld->get<components::TransformComponent>(ecsEntity).position = position;
    }
    prevPosition = position; // track last position for potential future use
}

bool Shot::isEqual(shot_id otherId, bool otherIsGlobalId) {
    return (id == otherId) && (isGlobalId == otherIsGlobalId);
}

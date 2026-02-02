#include "player.hpp"
#include "game/engine/client_engine.hpp"
#include "karma/core/types.hpp"
#include "game/net/messages.hpp"
#include "client/game.hpp"
#include "karma/components/audio_listener.h"
#include "karma/components/camera.h"
#include "karma/components/mesh.h"
#include "karma/components/transform.h"
#include "karma/common/config_helpers.hpp"
#include "renderer/radar_components.hpp"
#include <cmath>
#include <string>
#include <utility>
#include <memory>
#include "spdlog/spdlog.h"
#include "shot.hpp"

namespace components = karma::components;

Player::Player(Game &game,
               client_id id,
               PlayerParameters params,
               const std::string name,
               bool registeredUser,
               bool communityAdmin,
               bool localAdmin)
        : Actor(game, id),
            grounded(false),
            physics(&game.engine.physics->createPlayer()),
      audioEngine(*game.engine.audio),
    jumpAudio(audioEngine.loadClip(game.world->resolveAssetPath("audio.player.Jump").string(), 5)),
    dieAudio(audioEngine.loadClip(game.world->resolveAssetPath("audio.player.Die").string(), 1)),
    spawnAudio(audioEngine.loadClip(game.world->resolveAssetPath("audio.player.Spawn").string(), 1)),
    landAudio(audioEngine.loadClip(game.world->resolveAssetPath("audio.player.Land").string(), 1)),
      lastJumpTime(TimeUtils::GetCurrentTime()),
      jumpCooldown(TimeUtils::getDuration(0.1f)) {
        setParameters(std::move(params));
    state.name = name;
    state.registeredUser = registeredUser;
    state.communityAdmin = communityAdmin;
    state.localAdmin = localAdmin;
    state.alive = false;
    state.score = 0;
    lastPosition = glm::vec3(0.0f);
    lastRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    if (game.engine.ecsWorld) {
        ecsEntity = game.engine.ecsWorld->createEntity();
        components::TransformComponent xform{};
        xform.scale = glm::vec3(1.0f);
        game.engine.ecsWorld->add(ecsEntity, xform);
        game::renderer::RadarCircle circle{};
        circle.radius = 1.2f;
        game.engine.ecsWorld->add(ecsEntity, circle);
        // Local player mesh rendering is skipped to avoid first-person camera inside the tank.
        spdlog::info("Player: ECS entity created for local player (ecs_entity={})", ecsEntity.index);
    }

    // Initialize controller extents from parameters once params are set
    setExtents(glm::vec3(
        getParameter("x_extent"),
        getParameter("y_extent"),
        getParameter("z_extent")));
}

Player::~Player() {
    if (ecsEntity.isValid() && game.engine.ecsWorld) {
        game.engine.ecsWorld->destroyEntity(ecsEntity);
    }
}

glm::vec3 Player::getForwardVector() const {
    return physics->getForwardVector();
}

void Player::setExtents(const glm::vec3& extents) {
    if (physics) {
        physics->setHalfExtents(extents * 0.5f);
    }
}

void Player::earlyUpdate() {
    bool wasGrounded = grounded;
    grounded = physics->isGrounded();

    if (ecsEntity.isValid() && game.engine.ecsWorld) {
        if (game.engine.ecsWorld->has<components::TransformComponent>(ecsEntity)) {
            auto &transform = game.engine.ecsWorld->get<components::TransformComponent>(ecsEntity);
            transform.position = state.position;
            transform.rotation = state.rotation;
        }
        if (game.engine.ecsWorld->has<game::renderer::RadarCircle>(ecsEntity)) {
            game.engine.ecsWorld->get<game::renderer::RadarCircle>(ecsEntity).enabled = state.alive;
        }
    }

    if (state.alive) {
        game.engine.ui->setDialogVisible(false);
        
        if (grounded) {
            glm::vec2 movement(0.0f);
            if (game.getFocusState() == FOCUS_STATE_GAME)
                movement = game.engine.getInputState().movement;
            glm::vec3 movementVector = physics->getForwardVector();
            movementVector *= movement.y * getParameter("speed");
            movementVector.y = physics->getVelocity().y;

            physics->setVelocity(movementVector);

            physics->setAngularVelocity(glm::vec3(
                0.0f,
                -movement.x * getParameter("turnSpeed"),
                0.0f
            ));

            if (game.getFocusState() == FOCUS_STATE_GAME) {
                if (grounded && game.engine.getInputState().jump && TimeUtils::GetElapsedTime(lastJumpTime, TimeUtils::GetCurrentTime()) >= jumpCooldown) {
                    glm::vec3 velocity = physics->getVelocity();
                    velocity.y = getParameter("jumpSpeed");
                    physics->setVelocity(velocity);
                    lastJumpTime = TimeUtils::GetCurrentTime();
                    grounded = false;
                    jumpAudio.play(state.position);
                }
            }

            if (wasGrounded == false) {
                landAudio.play(state.position);
            }
        }

        if (game.getFocusState() == FOCUS_STATE_GAME) {
            if (game.engine.getInputState().fire) {
                const glm::vec3 cameraPos = state.position + glm::vec3(0.0f, muzzleOffset.y, 0.0f);
                const glm::vec3 muzzlePos = state.position + getForwardVector() * muzzleOffset.z +
                                            glm::vec3(0.0f, muzzleOffset.y, 0.0f);

                glm::vec3 shotPosition = muzzlePos;
                if (game.engine.physics) {
                    glm::vec3 hitPoint{};
                    glm::vec3 hitNormal{};
                    if (game.engine.physics->raycast(cameraPos, muzzlePos, hitPoint, hitNormal)) {
                        const glm::vec3 dirVec = muzzlePos - cameraPos;
                        const float dirLenSq = glm::dot(dirVec, dirVec);
                        if (dirLenSq > 1e-6f) {
                            const glm::vec3 dir = dirVec * (1.0f / std::sqrt(dirLenSq));
                            constexpr float backOff = 0.05f;
                            shotPosition = hitPoint - dir * backOff;
                        } else {
                            shotPosition = hitPoint;
                        }
                    }
                }

                glm::vec3 shotVelocity = getForwardVector() * getParameter("shotSpeed") + getVelocity();

                const glm::vec3 playerHitCenter = state.position + glm::vec3(0.0f, 1.0f, 0.0f);
                const glm::vec3 toShot = shotPosition - playerHitCenter;
                constexpr float minSelfShotDistance = 1.1f;
                const float distSq = glm::dot(toShot, toShot);
                if (distSq < minSelfShotDistance * minSelfShotDistance) {
                    glm::vec3 fwd = getForwardVector();
                    const float fwdLenSq = glm::dot(fwd, fwd);
                    if (fwdLenSq > 1e-6f) {
                        fwd *= (1.0f / std::sqrt(fwdLenSq));
                    } else {
                        fwd = glm::vec3(0.0f, 0.0f, -1.0f);
                    }
                    shotPosition = playerHitCenter + fwd * minSelfShotDistance;
                }

                auto shot = std::make_unique<Shot>(game, shotPosition, shotVelocity);
                game.addShot(std::move(shot));
            }
        }

    } else {
        if (grounded) {
            physics->setVelocity(glm::vec3(0.0f));
            physics->setAngularVelocity(glm::vec3(0.0f));
        }

        game.engine.ui->setDialogVisible(true);

        if (game.engine.getInputState().spawn) {
            ClientMsg_RequestPlayerSpawn spawnMsg;
            game.engine.network->send<ClientMsg_RequestPlayerSpawn>(spawnMsg);
        }
    }
}

void Player::lateUpdate() {
    setLocation(physics->getPosition(), physics->getRotation(), physics->getVelocity());
    if (game.engine.cameraEntity.isValid() && game.engine.ecsWorld &&
        game.engine.ecsWorld->has<components::TransformComponent>(game.engine.cameraEntity)) {
        auto &transform = game.engine.ecsWorld->get<components::TransformComponent>(game.engine.cameraEntity);
        transform.position = state.position + glm::vec3(0.0f, muzzleOffset.y, 0.0f);
        transform.rotation = state.rotation;
    }
    if (state.alive) {
        if (glm::distance(lastPosition, state.position) > POSITION_UPDATE_THRESHOLD ||
            angleBetween(lastRotation, state.rotation) > ROTATION_UPDATE_THRESHOLD) {
            ClientMsg_PlayerLocation locMsg;
            locMsg.position = state.position;
            locMsg.rotation = state.rotation;
            game.engine.network->send<ClientMsg_PlayerLocation>(locMsg);
            lastPosition = state.position;
            lastRotation = state.rotation;
        }
    }

}

void Player::update(TimeUtils::duration /*deltaTime*/) {
    earlyUpdate();
    lateUpdate();
}

void Player::setState(const PlayerState &newState) {
    state = newState;
    if (physics) {
        physics->setPosition(state.position);
        physics->setRotation(state.rotation);
        physics->setVelocity(state.velocity);
    }
    if (ecsEntity.isValid() && game.engine.ecsWorld &&
        game.engine.ecsWorld->has<components::TransformComponent>(ecsEntity)) {
        auto &transform = game.engine.ecsWorld->get<components::TransformComponent>(ecsEntity);
        transform.position = state.position;
        transform.rotation = state.rotation;
    }
}

void Player::die() {
    if (!state.alive) {
        return;
    }
    Actor::die();
    dieAudio.play(state.position);
    state.alive = false;
    auto vel = physics->getVelocity();
    physics->setVelocity(glm::vec3(vel.x, getParameter("jumpSpeed"), vel.z));
}

void Player::spawn(glm::vec3 position, glm::quat rotation, glm::vec3 velocity) {
    spawnAudio.play(position);
    state.alive = true;
    setLocation(position, rotation, velocity);

    physics->setPosition(position);
    physics->setRotation(rotation);
    physics->setVelocity(velocity);
    physics->setAngularVelocity(glm::vec3(0.0f));
}

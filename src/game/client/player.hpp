#pragma once
#include <glm/glm.hpp>
#include <string>
#include "karma/core/types.hpp"
#include "game/net/messages.hpp"
#include "karma/physics/player_controller.hpp"
#include "karma/audio/audio.hpp"
#include "karma/ecs/entity.h"
#include <spdlog/spdlog.h>

#include "actor.hpp"

#define POSITION_UPDATE_THRESHOLD 0.01f
#define ROTATION_UPDATE_THRESHOLD 0.01f

class Game;

class Player : public Actor {
private:
    bool grounded;

    PhysicsPlayerController* physics = nullptr;
    Audio& audioEngine;
    AudioClip jumpAudio;
    AudioClip dieAudio;
    AudioClip spawnAudio;
    AudioClip landAudio;

    TimeUtils::time lastJumpTime;
    TimeUtils::duration jumpCooldown;

    glm::vec3 lastPosition;
    glm::quat lastRotation;

    karma::ecs::Entity ecsEntity{};
    glm::vec3 muzzleOffset{0.0f, 1.18f, 2.22f};

public:
    Player(Game &game,
           client_id,
           PlayerParameters params,
           const std::string name,
           bool registeredUser,
           bool communityAdmin,
           bool localAdmin);
    ~Player();

    std::string getName() const { return state.name; }

    client_id getClientId() const { return id; }

    glm::vec3 getPosition() const { return state.position; }
    glm::vec3 getVelocity() const { return state.velocity; }
    glm::quat getRotation() const { return state.rotation; }
    glm::vec3 getForwardVector() const;
    int getScore() const { return state.score; }
    void setScore(int score) { Actor::setScore(score); }

    void setExtents(const glm::vec3& extents);
    void earlyUpdate();
    void lateUpdate();

    void update(TimeUtils::duration deltaTime) override;
    void setState(const PlayerState &newState) override;
    void die() override;
    void spawn(glm::vec3 position, glm::quat rotation, glm::vec3 velocity) override;
};

#pragma once
#include <cstdint>
#include "karma/core/types.hpp"
#include "game/net/messages.hpp"
#include <glm/vec3.hpp>
#include "karma/audio/audio.hpp"
#include "actor.hpp"
#include "karma/ecs/entity.h"

#include <string>

class Game;
class AudioClip;

class Client : public Actor {
private:
    AudioClip dieAudio;
    karma::ecs::Entity ecsEntity{};
    bool justSpawned = false;
    glm::vec3 lastSpawnPosition{0.0f};

    void syncRenderFromState();

public:
    Client(Game &game, client_id id, const PlayerState &initialState);
    ~Client();

    std::string getName() const { return state.name; }
    int getScore() const { return state.score; }

    void update(TimeUtils::duration deltaTime) override;
    void setState(const PlayerState &newState) override;
    void die() override;
    void spawn(glm::vec3 position, glm::quat rotation, glm::vec3 velocity) override;
};

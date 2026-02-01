#pragma once

#include "karma/core/types.hpp"
#include "game/net/messages.hpp"
#include "karma/physics/static_body.hpp"
#include "game/world/config.hpp"
#include "karma/common/world_content.hpp"
#include "karma/ecs/entity.h"

#include <filesystem>
#include "karma/common/json.hpp"
#include <string>
#include <vector>

class Game;

class ClientWorldSession {
private:
    Game &game;
    karma::ecs::Entity worldEcsEntity{};
    PhysicsStaticBody physics;
    world::WorldContent content_;
    PlayerParameters defaultPlayerParameters_;
    bool initialized = false;

    std::string serverName;
    uint32_t protocolVersion = 0;
    std::vector<std::string> features;

public:
    client_id playerId{};

    ClientWorldSession(Game &game, std::string worldDir);
    ~ClientWorldSession();

    void load(std::string worldPath);
    bool isInitialized() const;
    void update();
    std::filesystem::path resolveAssetPath(const std::string &assetName) const;

    PlayerParameters defaultPlayerParameters() const {
        return defaultPlayerParameters_;
    }
};

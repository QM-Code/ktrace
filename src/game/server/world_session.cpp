#include "server/world_session.hpp"

#include "server/game.hpp"
#include "spdlog/spdlog.h"
#include "karma/common/data_path_resolver.hpp"
#include "karma/common/world_archive.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <random>
#include <optional>

ServerWorldSession::ServerWorldSession(Game &game,
                                       std::string serverNameIn,
                                       std::string worldName,
                                       karma::json::Value worldConfig,
                                       std::string worldDir,
                                       bool enableWorldZipping)
        : game(game),
          serverName(std::move(serverNameIn)),
          archiveOnStartup(enableWorldZipping) {
    const std::vector<karma::data::ConfigLayerSpec> baseSpecs = {
        {"common/config.json", "data/common/config.json", spdlog::level::err, true},
        {"server/config.json", "data/server/config.json", spdlog::level::err, true}
    };

    const std::optional<karma::json::Value> configOpt = worldConfig.is_null() ? std::nullopt : std::optional<karma::json::Value>(std::move(worldConfig));
    content_ = world::LoadWorldContent(baseSpecs,
                                       configOpt,
                                       std::filesystem::path(worldDir),
                                       std::move(worldName),
                                       "ServerWorldSession");
    defaultPlayerParameters_ = game_world::ExtractDefaultPlayerParameters(content_.config);

    if (archiveOnStartup) {
        archiveCache = buildArchive();
        archiveCached = true;
    } else {
        spdlog::debug("ServerWorldSession: Skipping archive generation for bundled world at {}", content_.rootDir.string());
    }

    physics = game.engine.physics->createStaticMesh(resolveAssetPath("world").string());
}

ServerWorldSession::~ServerWorldSession() {
    physics.destroy();
}

world::ArchiveBytes ServerWorldSession::buildArchive() {
    if (!archiveOnStartup) {
        return {};
    }

    if (archiveCached) {
        return archiveCache;
    }

    archiveCache = world::BuildWorldArchive(content_.rootDir);
    archiveCached = true;
    return archiveCache;
}

void ServerWorldSession::update() {
}

void ServerWorldSession::sendWorldInit(client_id clientId) {
    world::ArchiveBytes worldData = buildArchive();

    ServerMsg_Init initHeaderMsg;
    initHeaderMsg.clientId = clientId;
    initHeaderMsg.serverName = serverName;
    initHeaderMsg.worldName = content_.name;
    initHeaderMsg.protocolVersion = NET_PROTOCOL_VERSION;
    initHeaderMsg.defaultPlayerParams = defaultPlayerParameters_;
    initHeaderMsg.worldData = worldData;
    game.engine.network->send<ServerMsg_Init>(clientId, &initHeaderMsg);

    spdlog::trace("ServerWorldSession: Sent init message to client id {}", clientId);
}

std::filesystem::path ServerWorldSession::resolveAssetPath(const std::string &assetName) const {
    return content_.resolveAssetPath(assetName, "ServerWorldSession");
}

Location ServerWorldSession::pickSpawnLocation() const {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> distXZ(-20.0f, 20.0f);
    std::uniform_real_distribution<float> distRot(0.0f, glm::two_pi<float>());

    const float x = distXZ(rng);
    const float z = distXZ(rng);

    const glm::vec3 rayStart{x, 500.0f, z};
    const glm::vec3 rayEnd{x, -100.0f, z};
    glm::vec3 hitPoint{};
    glm::vec3 hitNormal{};
    float y = 5.0f; // fallback height if nothing is hit

    if (game.engine.physics && game.engine.physics->raycast(rayStart, rayEnd, hitPoint, hitNormal)) {
        y = hitPoint.y;
    }

    const float rotY = distRot(rng);
    return Location{
        .position = glm::vec3(x, y, z),
        .rotation = glm::angleAxis(rotY, glm::vec3(0.0f, 1.0f, 0.0f))
    };
}

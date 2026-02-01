#include "client/world_session.hpp"

#include "client/game.hpp"
#include "spdlog/spdlog.h"
#include "karma/components/mesh.h"
#include "karma/components/transform.h"
#include "renderer/radar_components.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "karma/common/config_helpers.hpp"
#include "karma/common/config_store.hpp"
#include "karma/common/world_archive.hpp"

namespace components = karma::components;

ClientWorldSession::ClientWorldSession(Game &game, std::string worldDir)
        : game(game) {
    const auto userConfigPath = karma::config::ConfigStore::Initialized()
        ? karma::config::ConfigStore::UserConfigPath()
        : karma::data::EnsureUserConfigFile("config.json");

    const std::vector<karma::data::ConfigLayerSpec> layerSpecs = {
        {"common/config.json", "data/common/config.json", spdlog::level::err, true},
        {"client/config.json", "data/client/config.json", spdlog::level::debug, false},
        {userConfigPath, "user config", spdlog::level::debug, false}
    };

    content_ = world::LoadWorldContent(layerSpecs,
                                       std::nullopt,
                                       std::filesystem::path(worldDir),
                                       std::string{},
                                       "ClientWorldSession");
    defaultPlayerParameters_ = game_world::ExtractDefaultPlayerParameters(content_.config);
}

ClientWorldSession::~ClientWorldSession() {
    if (worldEcsEntity.isValid() && game.engine.ecsWorld) {
        game.engine.ecsWorld->destroyEntity(worldEcsEntity);
    }
    physics.destroy();
}

void ClientWorldSession::load(std::string worldPath) {
    content_.rootDir = std::move(worldPath);
}

bool ClientWorldSession::isInitialized() const {
    return initialized;
}

void ClientWorldSession::update() {
    for (const auto &initMsg : game.engine.network->consumeMessages<ServerMsg_Init>()) {
        spdlog::trace("ClientWorldSession: Received init message from server");
        serverName = initMsg.serverName;
        content_.name = initMsg.worldName;
        protocolVersion = initMsg.protocolVersion;
        features = initMsg.features;
        if (protocolVersion != 0 && protocolVersion != NET_PROTOCOL_VERSION) {
            spdlog::error("ClientWorldSession: Protocol version mismatch (client {}, server {})",
                          NET_PROTOCOL_VERSION,
                          protocolVersion);
            game.engine.network->disconnect("Protocol version mismatch.");
            return;
        }
        defaultPlayerParameters_.clear();
        for (const auto& [key, val] : initMsg.defaultPlayerParams) {
            defaultPlayerParameters_[key] = val;
        }
        playerId = initMsg.clientId;

        if (!initMsg.worldData.empty()) {
            std::filesystem::path downloadsDir;
            if (const auto endpoint = game.engine.network->getServerEndpoint()) {
                downloadsDir = karma::data::EnsureUserWorldDirectoryForServer(endpoint->host, endpoint->port);
            } else {
                spdlog::warn("ClientWorldSession: Server endpoint unknown; falling back to shared world directory");
                downloadsDir = karma::data::EnsureUserWorldsDirectory();
            }

            content_.rootDir = downloadsDir;

            world::ExtractWorldArchive(initMsg.worldData, downloadsDir);

            const auto worldConfigPath = downloadsDir / "config.json";
            auto worldConfigOpt = world::ReadWorldJsonFile(worldConfigPath);
            if (worldConfigOpt.has_value()) {
                if (!worldConfigOpt->is_object()) {
                    spdlog::warn("ClientWorldSession: World config is not a JSON object: {}", worldConfigPath.string());
                } else {
                    constexpr const char* worldConfigLabel = "world config";
                    if (!karma::config::ConfigStore::AddRuntimeLayer(worldConfigLabel, *worldConfigOpt, downloadsDir)) {
                        spdlog::warn("ClientWorldSession: Failed to merge world config layer from {}", worldConfigPath.string());
                    } else {
                        karma::data::MergeJsonObjects(content_.config, *worldConfigOpt);
                        content_.mergeLayer(*worldConfigOpt, downloadsDir);
                        if (defaultPlayerParameters_.empty()) {
                            defaultPlayerParameters_ = game_world::ExtractDefaultPlayerParameters(content_.config);
                        }
                    }
                }
            } else {
                spdlog::warn("ClientWorldSession: World config not found at {}", worldConfigPath.string());
            }

        } else {
            spdlog::debug("ClientWorldSession: Received bundled world indication; skipping download");
        }

        const auto worldPath = resolveAssetPath("world");
        worldEcsEntity = game.engine.ecsWorld->createEntity();
        components::TransformComponent worldXform{};
        game.engine.ecsWorld->add(worldEcsEntity, worldXform);
        components::MeshComponent worldMesh{};
        worldMesh.mesh_key = worldPath.string();
        game.engine.ecsWorld->add(worldEcsEntity, worldMesh);
        game.engine.ecsWorld->add(worldEcsEntity, game::renderer::RadarRenderable{true});
        spdlog::info("ClientWorldSession: ECS world mesh enabled (entity={}, path={})",
                     worldEcsEntity.index, worldPath.string());
        physics = game.engine.physics->createStaticMesh(worldPath.string());

        spdlog::info("ClientWorldSession: World initialized from server");
        initialized = true;
        return;
    }
}

std::filesystem::path ClientWorldSession::resolveAssetPath(const std::string &assetName) const {
    return content_.resolveAssetPath(assetName, "ClientWorldSession");
}

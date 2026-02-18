#include "game.hpp"

#include "client/domain/tank_drive_controller.hpp"
#include "client/game/math.hpp"
#include "karma/common/config_helpers.hpp"
#include "karma/common/config_store.hpp"
#include "karma/common/logging.hpp"
#include "karma/ecs/world.hpp"
#include "karma/renderer/layers.hpp"
#include "karma/renderer/render_system.hpp"
#include "karma/scene/components.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>

#include <spdlog/spdlog.h>

namespace bz3 {

bool Game::ensureLocalTankEntity() {
    if (!world || !graphics || !render) {
        return false;
    }
    if (tank_entity_.isValid() && world->isAlive(tank_entity_)) {
        return true;
    }

    destroyLocalTankEntity();

    const auto model_path = karma::config::ConfigStore::ResolveAssetPath("assets.models.playerModel", {});
    if (model_path.empty()) {
        spdlog::error("Game: failed to resolve assets.models.playerModel");
        return false;
    }

    tank_mesh_id_ = graphics->createMeshFromFile(model_path);
    if (tank_mesh_id_ == karma::renderer::kInvalidMesh) {
        spdlog::error("Game: failed to create tank mesh from '{}'", model_path.string());
        return false;
    }

    karma::renderer::MaterialDesc tank_material{};
    tank_material.base_color = glm::vec4(0.95f, 0.95f, 0.95f, 1.0f);
    tank_material_id_ = graphics->createMaterial(tank_material);
    if (tank_material_id_ == karma::renderer::kInvalidMaterial) {
        graphics->destroyMesh(tank_mesh_id_);
        tank_mesh_id_ = karma::renderer::kInvalidMesh;
        spdlog::error("Game: failed to create tank material");
        return false;
    }

    tank_drive_ = std::make_unique<client::domain::TankDriveController>();
    client::domain::TankDriveParams drive_params{};
    drive_params.forward_speed =
        std::max(0.0f, karma::config::ReadFloatConfig({"gameplay.tank.forwardSpeed"}, 8.0f));
    drive_params.reverse_speed =
        std::max(0.0f, karma::config::ReadFloatConfig({"gameplay.tank.reverseSpeed"}, 5.0f));
    drive_params.turn_speed =
        std::max(0.0f, karma::config::ReadFloatConfig({"gameplay.tank.turnSpeed"}, 2.0f));
    tank_drive_->setParams(drive_params);

    client::domain::TankDriveState initial_state{};
    initial_state.position = glm::vec3(
        karma::config::ReadFloatConfig({"gameplay.tank.startX"}, 0.0f),
        karma::config::ReadFloatConfig({"gameplay.tank.startY"}, 0.6f),
        karma::config::ReadFloatConfig({"gameplay.tank.startZ"}, 0.0f));
    initial_state.yaw_radians =
        karma::config::ReadFloatConfig({"gameplay.tank.startYawDegrees"}, 0.0f)
        * glm::pi<float>() / 180.0f;
    tank_drive_->setState(initial_state);

    tank_model_scale_ = std::max(0.01f, karma::config::ReadFloatConfig({"gameplay.tank.modelScale"}, 1.0f));
    tank_model_yaw_offset_radians_ =
        karma::config::ReadFloatConfig({"gameplay.tank.modelYawOffsetDegrees"}, 0.0f)
        * glm::pi<float>() / 180.0f;
    tank_camera_yaw_offset_radians_ =
        karma::config::ReadFloatConfig({"gameplay.tank.cameraYawOffsetDegrees"}, 0.0f)
        * glm::pi<float>() / 180.0f;

    std::string camera_mode =
        karma::config::ReadStringConfig("gameplay.tank.cameraMode", std::string("fps"));
    for (char& ch : camera_mode) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    tank_camera_mode_ =
        (camera_mode == "chase" || camera_mode == "thirdperson" || camera_mode == "third_person")
        ? TankCameraMode::Chase
        : TankCameraMode::Fps;

    tank_camera_height_ = std::max(
        0.1f,
        karma::config::ReadFloatConfig(
            {"gameplay.tank.cameraHeight"},
            (tank_camera_mode_ == TankCameraMode::Fps) ? 1.2f : 4.0f));
    tank_camera_follow_distance_ = std::max(
        0.1f,
        karma::config::ReadFloatConfig(
            {"gameplay.tank.cameraDistance"},
            (tank_camera_mode_ == TankCameraMode::Fps) ? 0.0f : 10.0f));
    tank_camera_forward_offset_ =
        std::max(0.0f, karma::config::ReadFloatConfig({"gameplay.tank.cameraForwardOffset"}, 0.7f));
    tank_camera_look_ahead_ =
        std::max(1.0f, karma::config::ReadFloatConfig({"gameplay.tank.cameraLookAhead"}, 8.0f));
    tank_camera_smooth_rate_ =
        std::max(1.0f, karma::config::ReadFloatConfig({"gameplay.tank.cameraSmoothRate"}, 18.0f));
    tank_visual_smooth_rate_ =
        std::max(1.0f, karma::config::ReadFloatConfig({"gameplay.tank.visualSmoothRate"}, 22.0f));
    tank_sim_max_step_seconds_ = std::clamp(
        karma::config::ReadFloatConfig({"gameplay.tank.simMaxStepSeconds"}, 1.0f / 120.0f),
        1.0f / 300.0f,
        1.0f / 30.0f);
    tank_collision_radius_ =
        std::max(0.1f, karma::config::ReadFloatConfig({"gameplay.tank.collisionRadius"}, 1.0f));
    buildTankCollisionCache();
    tank_visual_position_ = initial_state.position;
    tank_visual_yaw_radians_ = client::game::detail::NormalizeAngle(initial_state.yaw_radians);
    tank_visual_initialized_ = true;
    tank_camera_initialized_ = false;

    tank_entity_ = world->createEntity();

    karma::scene::TransformComponent transform{};
    world->add(tank_entity_, transform);

    karma::scene::RenderComponent render_component{};
    render_component.mesh = tank_mesh_id_;
    render_component.material = tank_material_id_;
    render_component.layer = karma::renderer::kLayerWorld;
    render_component.casts_shadow = true;
    world->add(tank_entity_, render_component);

    updateLocalTankTransform();
    updateTankFollowCamera(0.0f);
    syncInputMode();

    KARMA_TRACE("game.client",
                "Game: local tank entity ready model='{}' entity={} forward_speed={:.2f} reverse_speed={:.2f} turn_speed={:.2f} camera_mode={} collision_obstacles={}",
                model_path.string(),
                tank_entity_.index,
                drive_params.forward_speed,
                drive_params.reverse_speed,
                drive_params.turn_speed,
                (tank_camera_mode_ == TankCameraMode::Fps) ? "fps" : "chase",
                tank_collision_rects_.size());
    return true;
}

void Game::destroyLocalTankEntity() {
    if (world && tank_entity_.isValid() && world->isAlive(tank_entity_)) {
        world->destroyEntity(tank_entity_);
    }
    tank_entity_ = {};

    if (graphics && tank_material_id_ != karma::renderer::kInvalidMaterial) {
        graphics->destroyMaterial(tank_material_id_);
    }
    tank_material_id_ = karma::renderer::kInvalidMaterial;

    if (graphics && tank_mesh_id_ != karma::renderer::kInvalidMesh) {
        graphics->destroyMesh(tank_mesh_id_);
    }
    tank_mesh_id_ = karma::renderer::kInvalidMesh;
    tank_collision_rects_.clear();
    tank_collision_ready_ = false;
    tank_world_bounds_ready_ = false;
    tank_visual_initialized_ = false;
    tank_camera_initialized_ = false;
    tank_drive_.reset();
}

void Game::updateLocalTankTransform() {
    if (!world || !tank_entity_.isValid() || !world->isAlive(tank_entity_) || !tank_drive_) {
        return;
    }

    auto* transform = world->tryGet<karma::scene::TransformComponent>(tank_entity_);
    if (!transform) {
        return;
    }

    const float yaw = tank_visual_yaw_radians_ + tank_model_yaw_offset_radians_;
    glm::mat4 local(1.0f);
    local = glm::translate(local, tank_visual_position_);
    local = glm::rotate(local, yaw, glm::vec3{0.0f, 1.0f, 0.0f});
    local = glm::scale(local, glm::vec3{tank_model_scale_, tank_model_scale_, tank_model_scale_});
    transform->local = local;
    transform->world = local;
}

} // namespace bz3

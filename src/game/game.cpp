#include "game.hpp"

#include "client/domain/tank_drive_controller.hpp"
#include "client/net/connection.hpp"
#include "karma/geometry/mesh_loader.hpp"
#include "karma/audio/backend.hpp"
#include "karma/audio/audio_system.hpp"
#include "karma/common/config_helpers.hpp"
#include "karma/common/config_store.hpp"
#include "karma/common/logging.hpp"
#include "karma/ecs/world.hpp"
#include "karma/input/input_system.hpp"
#include "karma/renderer/layers.hpp"
#include "karma/renderer/render_system.hpp"
#include "karma/scene/components.hpp"
#include "karma/ui/ui_draw_context.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <spdlog/spdlog.h>
#include <cstdlib>
#include <string>
#include <utility>

namespace bz3 {

namespace {

constexpr float kEpsilon = 1e-6f;
constexpr float kMinObstacleHeight = 0.8f;
constexpr float kMinObstacleArea = 0.7f;
constexpr float kHugeGroundArea = 1500.0f;
constexpr float kTwoPi = glm::pi<float>() * 2.0f;

float Clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float SmoothAlpha(float dt, float smooth_rate) {
    if (dt <= 0.0f || smooth_rate <= 0.0f) {
        return 1.0f;
    }
    const float alpha = 1.0f - std::exp(-smooth_rate * dt);
    return Clamp01(alpha);
}

float NormalizeAngle(float radians) {
    if (!std::isfinite(radians)) {
        return 0.0f;
    }
    const float wrapped = std::fmod(radians, kTwoPi);
    if (wrapped < 0.0f) {
        return wrapped + kTwoPi;
    }
    return wrapped;
}

float LerpAngle(float from, float to, float alpha) {
    const float clamped_alpha = Clamp01(alpha);
    const float delta = std::remainder(to - from, kTwoPi);
    return NormalizeAngle(from + delta * clamped_alpha);
}

glm::vec3 LerpVec3(const glm::vec3& from, const glm::vec3& to, float alpha) {
    const float t = Clamp01(alpha);
    return from + ((to - from) * t);
}

} // namespace

Game::Game(GameStartupOptions options) : startup_(std::move(options)) {}

Game::~Game() = default;

void Game::onStart() {
    tank_enabled_ = karma::config::ReadBoolConfig({"gameplay.tank.enabled"}, false);
    if (tank_enabled_) {
        static_cast<void>(ensureLocalTankEntity());
    } else {
        destroyLocalTankEntity();
    }
    syncInputMode();

    if (!startup_.connect_on_start) {
        return;
    }

    connection_ = std::make_unique<client::net::ClientConnection>(
        startup_.connect_addr,
        startup_.connect_port,
        startup_.player_name,
        startup_.auth_payload,
        [this](client::net::AudioEvent event) { onAudioEvent(event); });
    if (!connection_->start()) {
        KARMA_TRACE("net.client",
                    "Game: startup connect failed addr='{}' port={} name='{}'",
                    startup_.connect_addr,
                    startup_.connect_port,
                    startup_.player_name);
        connection_.reset();
    }
}

void Game::onUpdate(float dt) {
    if (tank_enabled_) {
        static_cast<void>(ensureLocalTankEntity());
    }
    if (connection_) {
        connection_->poll();
        if (connection_->shouldExit()) {
            spdlog::error("Game: network join was rejected, closing client.");
            std::exit(1);
        }
    }

    if (!input) {
        chat_entry_focused_ = false;
        console_toggle_was_down_ = false;
        escape_was_down_ = false;
        chat_was_down_ = false;
        spawn_was_down_ = false;
        fire_was_down_ = false;
        return;
    }

    const bool console_down = input->global().actionDown("console");
    const bool escape_down = input->global().actionDown("quickMenu");
    const bool chat_down = input->game().actionDown("chat");
    const bool console_pressed =
        input->global().actionPressed("console") || (console_down && !console_toggle_was_down_);
    const bool escape_pressed =
        input->global().actionPressed("quickMenu") || (escape_down && !escape_was_down_);
    const bool chat_pressed =
        input->game().actionPressed("chat") || (chat_down && !chat_was_down_);

    if (console_pressed) {
        console_visible_ = !console_visible_;
        if (!console_visible_) {
            chat_entry_focused_ = false;
        }
    } else if (chat_pressed) {
        console_visible_ = true;
        chat_entry_focused_ = true;
    } else if (console_visible_ && escape_pressed) {
        console_visible_ = false;
        chat_entry_focused_ = false;
    }

    syncInputMode();

    const bool spawn_down = input->game().actionDown("spawn");
    const bool fire_down = input->game().actionDown("fire");

    const bool spawn_pressed =
        input->game().actionPressed("spawn") || (spawn_down && !spawn_was_down_);
    const bool fire_pressed =
        input->game().actionPressed("fire") || (fire_down && !fire_was_down_);

    const bool gameplay_input_enabled = !console_visible_ && !chat_entry_focused_;

    if (tank_enabled_) {
        updateLocalTank(dt, gameplay_input_enabled);
    }

    if (gameplay_input_enabled && spawn_pressed) {
        if (connection_ && connection_->isConnected()) {
            (void)connection_->sendRequestPlayerSpawn();
        }
    }

    if (gameplay_input_enabled && fire_pressed) {
        // Play local fire immediately for responsive feedback.
        onAudioEvent(client::net::AudioEvent::ShotFire);
        if (connection_ && connection_->isConnected()) {
            (void)connection_->sendCreateShot();
        }
    }

    console_toggle_was_down_ = console_down;
    escape_was_down_ = escape_down;
    chat_was_down_ = chat_down;
    spawn_was_down_ = spawn_down;
    fire_was_down_ = fire_down;
}

void Game::onUiUpdate(float dt, karma::ui::UiDrawContext& ui) {
    (void)dt;
    const bool connected = connection_ && connection_->isConnected();
    const bool hud_visible = connected || !console_visible_;

    if (hud_visible) {
        karma::ui::UiDrawContext::TextPanel hud{};
        hud.title = "BZ3 HUD";
        hud.lines.push_back("Player: " + startup_.player_name);
        if (startup_.connect_on_start) {
            hud.lines.push_back("Connect target: " + startup_.connect_addr + ":" +
                                std::to_string(startup_.connect_port));
            hud.lines.push_back(std::string("Connection: ") + (connected ? "connected" : "connecting"));
        } else {
            hud.lines.push_back("Connection: offline");
        }
        hud.lines.push_back(console_visible_ ? "Console: open" : "Console: closed");
        hud.lines.push_back(chat_entry_focused_ ? "Chat entry focus: active" : "Chat entry focus: idle");
        hud.lines.push_back("HUD rule: connected || !console");
        if (tank_enabled_ && tank_drive_) {
            const auto& tank = tank_drive_->state();
            hud.lines.push_back("Tank: arrow keys to drive");
            hud.lines.push_back("Tank pos: (" + std::to_string(tank.position.x) + ", " +
                                std::to_string(tank.position.y) + ", " +
                                std::to_string(tank.position.z) + ")");
        } else if (!tank_enabled_) {
            hud.lines.push_back("Mode: roaming camera (tank disabled)");
        } else {
            hud.lines.push_back("Tank: unavailable (player model failed to load)");
        }
        ui.addTextPanel(std::move(hud));
    }

    if (console_visible_) {
        karma::ui::UiDrawContext::TextPanel console{};
        console.title = "BZ3 Console";
        console.x = 72.0f;
        console.y = 84.0f;
        console.lines.push_back("Console parity slice (engine lifecycle + game state mapping).");
        console.lines.push_back("` toggle: open/close");
        console.lines.push_back("Chat action: focus chat entry + open console");
        console.lines.push_back("Esc: close console");
        console.lines.push_back(chat_entry_focused_ ? "Chat entry focus is active." : "Chat entry focus is idle.");
        console.lines.push_back("Gameplay input blocked while console/chat focus is active.");
        ui.addTextPanel(std::move(console));
    }
}

void Game::onShutdown() {
    if (connection_) {
        connection_->shutdown();
        connection_.reset();
    }
    destroyLocalTankEntity();
    console_visible_ = false;
    chat_entry_focused_ = false;
    syncInputMode();
}

void Game::syncInputMode() {
    if (!input) {
        return;
    }

    if (world && tank_entity_.isValid() && world->isAlive(tank_entity_)) {
        input->setMode(karma::input::InputMode::Game);
        return;
    }

    const bool ui_focus_active = console_visible_ || chat_entry_focused_;
    input->setMode(ui_focus_active ? karma::input::InputMode::Game
                                   : karma::input::InputMode::Roaming);
}

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
    tank_visual_yaw_radians_ = NormalizeAngle(initial_state.yaw_radians);
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

void Game::buildTankCollisionCache() {
    tank_collision_rects_.clear();
    tank_collision_ready_ = false;
    tank_world_bounds_ready_ = false;

    const auto world_path = karma::config::ConfigStore::ResolveAssetPath("assets.models.world", {});
    if (world_path.empty()) {
        KARMA_TRACE("game.client", "Game: collision cache skipped (world asset unresolved)");
        return;
    }

    std::vector<karma::geometry::SceneMesh> scene_meshes{};
    if (!karma::geometry::LoadScene(world_path, scene_meshes) || scene_meshes.empty()) {
        KARMA_TRACE("game.client",
                    "Game: collision cache skipped (LoadScene failed path='{}')",
                    world_path.string());
        return;
    }

    bool has_world_bounds = false;
    glm::vec2 world_min{};
    glm::vec2 world_max{};

    for (const auto& scene_mesh : scene_meshes) {
        if (scene_mesh.mesh.positions.empty()) {
            continue;
        }

        glm::vec3 minv = scene_mesh.mesh.positions[0];
        glm::vec3 maxv = scene_mesh.mesh.positions[0];
        for (const auto& p : scene_mesh.mesh.positions) {
            minv.x = std::min(minv.x, p.x);
            minv.y = std::min(minv.y, p.y);
            minv.z = std::min(minv.z, p.z);
            maxv.x = std::max(maxv.x, p.x);
            maxv.y = std::max(maxv.y, p.y);
            maxv.z = std::max(maxv.z, p.z);
        }

        if (!has_world_bounds) {
            world_min = glm::vec2(minv.x, minv.z);
            world_max = glm::vec2(maxv.x, maxv.z);
            has_world_bounds = true;
        } else {
            world_min.x = std::min(world_min.x, minv.x);
            world_min.y = std::min(world_min.y, minv.z);
            world_max.x = std::max(world_max.x, maxv.x);
            world_max.y = std::max(world_max.y, maxv.z);
        }

        const float height = maxv.y - minv.y;
        const float area_xz = (maxv.x - minv.x) * (maxv.z - minv.z);
        if (height < kMinObstacleHeight || area_xz < kMinObstacleArea) {
            continue;
        }
        if (area_xz > kHugeGroundArea && height < 3.0f) {
            continue;
        }

        tank_collision_rects_.push_back(TankObstacleRect{
            .min_x = minv.x,
            .max_x = maxv.x,
            .min_z = minv.z,
            .max_z = maxv.z,
            .min_y = minv.y,
            .max_y = maxv.y});
    }

    if (has_world_bounds) {
        tank_world_bounds_ready_ = true;
        tank_world_min_ = world_min;
        tank_world_max_ = world_max;
    }
    tank_collision_ready_ = tank_world_bounds_ready_ || !tank_collision_rects_.empty();
}

glm::vec3 Game::resolveTankCollision(const glm::vec3& previous_position,
                                     const glm::vec3& candidate_position) const {
    auto clamp_to_world = [this](glm::vec3 pos) {
        if (!tank_world_bounds_ready_) {
            return pos;
        }
        const float min_x = tank_world_min_.x + tank_collision_radius_;
        const float max_x = tank_world_max_.x - tank_collision_radius_;
        const float min_z = tank_world_min_.y + tank_collision_radius_;
        const float max_z = tank_world_max_.y - tank_collision_radius_;
        if (min_x < max_x) {
            pos.x = std::clamp(pos.x, min_x, max_x);
        }
        if (min_z < max_z) {
            pos.z = std::clamp(pos.z, min_z, max_z);
        }
        return pos;
    };

    glm::vec3 candidate = clamp_to_world(candidate_position);
    if (!positionBlockedByCollision(candidate)) {
        return candidate;
    }

    glm::vec3 x_only = clamp_to_world(glm::vec3{candidate.x, previous_position.y, previous_position.z});
    if (!positionBlockedByCollision(x_only)) {
        return x_only;
    }

    glm::vec3 z_only = clamp_to_world(glm::vec3{previous_position.x, previous_position.y, candidate.z});
    if (!positionBlockedByCollision(z_only)) {
        return z_only;
    }

    return clamp_to_world(previous_position);
}

bool Game::positionBlockedByCollision(const glm::vec3& position) const {
    if (!tank_collision_ready_) {
        return false;
    }

    for (const auto& obstacle : tank_collision_rects_) {
        if ((position.y + 0.25f) < obstacle.min_y || (position.y - 1.5f) > obstacle.max_y) {
            continue;
        }
        const float closest_x = std::clamp(position.x, obstacle.min_x, obstacle.max_x);
        const float closest_z = std::clamp(position.z, obstacle.min_z, obstacle.max_z);
        const float dx = position.x - closest_x;
        const float dz = position.z - closest_z;
        if ((dx * dx) + (dz * dz) <= (tank_collision_radius_ * tank_collision_radius_)) {
            return true;
        }
    }
    return false;
}

void Game::updateLocalTank(float dt, bool gameplay_input_enabled) {
    if (!tank_drive_) {
        return;
    }

    client::domain::TankDriveInput drive_input{};
    if (gameplay_input_enabled && input) {
        if (input->game().actionDown("moveForward")) {
            drive_input.throttle += 1.0f;
        }
        if (input->game().actionDown("moveBackward")) {
            drive_input.throttle -= 1.0f;
        }
        if (input->game().actionDown("moveLeft")) {
            drive_input.steering += 1.0f;
        }
        if (input->game().actionDown("moveRight")) {
            drive_input.steering -= 1.0f;
        }
    }

    const float clamped_dt = std::max(0.0f, dt);
    const int simulation_steps = std::clamp(
        static_cast<int>(std::ceil(clamped_dt / std::max(kEpsilon, tank_sim_max_step_seconds_))),
        1,
        12);
    const float step_dt = clamped_dt / static_cast<float>(simulation_steps);
    for (int step = 0; step < simulation_steps; ++step) {
        const auto before = tank_drive_->state();
        tank_drive_->update(step_dt, drive_input);
        auto after = tank_drive_->state();
        after.position = resolveTankCollision(before.position, after.position);
        const glm::vec3 delta = after.position - before.position;
        if (glm::dot(delta, delta) <= kEpsilon) {
            after.speed = 0.0f;
        }
        tank_drive_->setState(after);
    }

    const auto& sim = tank_drive_->state();
    if (!tank_visual_initialized_) {
        tank_visual_position_ = sim.position;
        tank_visual_yaw_radians_ = NormalizeAngle(sim.yaw_radians);
        tank_visual_initialized_ = true;
    } else {
        const float alpha = SmoothAlpha(clamped_dt, tank_visual_smooth_rate_);
        tank_visual_position_ = LerpVec3(tank_visual_position_, sim.position, alpha);
        tank_visual_yaw_radians_ = LerpAngle(tank_visual_yaw_radians_, sim.yaw_radians, alpha);
    }

    updateLocalTankTransform();
    updateTankFollowCamera(clamped_dt);
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

void Game::updateTankFollowCamera(float dt_seconds) {
    if (!render || !tank_drive_) {
        return;
    }

    const float camera_yaw = NormalizeAngle(tank_visual_yaw_radians_ + tank_camera_yaw_offset_radians_);
    const glm::vec3 forward = client::domain::TankDriveController::ForwardFromYaw(camera_yaw);
    const glm::vec3 up{0.0f, 1.0f, 0.0f};

    glm::vec3 desired_position{};
    glm::vec3 desired_target{};
    if (tank_camera_mode_ == TankCameraMode::Fps) {
        desired_position = tank_visual_position_
            + (up * tank_camera_height_)
            + (forward * tank_camera_forward_offset_);
        desired_target = desired_position + (forward * tank_camera_look_ahead_);
    } else {
        desired_position = tank_visual_position_
            - (forward * tank_camera_follow_distance_)
            + (up * tank_camera_height_);
        desired_target = tank_visual_position_ + glm::vec3{0.0f, 1.2f, 0.0f}
            + (forward * tank_camera_look_ahead_);
    }

    if (!tank_camera_initialized_) {
        tank_camera_position_ = desired_position;
        tank_camera_target_ = desired_target;
        tank_camera_initialized_ = true;
    } else {
        const float alpha = SmoothAlpha(dt_seconds, tank_camera_smooth_rate_);
        tank_camera_position_ = LerpVec3(tank_camera_position_, desired_position, alpha);
        tank_camera_target_ = LerpVec3(tank_camera_target_, desired_target, alpha);
    }

    auto camera = render->camera();
    camera.position = tank_camera_position_;
    camera.target = tank_camera_target_;
    render->setCamera(camera);
}

void Game::onAudioEvent(client::net::AudioEvent event) {
    switch (event) {
        case client::net::AudioEvent::PlayerSpawn:
            playOneShotAsset("assets.audio.player.Spawn", 1.0f, 1.0f);
            break;
        case client::net::AudioEvent::PlayerDeath:
            playOneShotAsset("assets.audio.player.Die", 1.0f, 1.0f);
            break;
        case client::net::AudioEvent::ShotFire:
            playOneShotAsset("assets.audio.shot.Fire", 0.65f, 1.0f);
            break;
        default:
            break;
    }
}

void Game::playOneShotAsset(const char* asset_key, float gain, float pitch) {
    if (!audio || !asset_key || *asset_key == '\0') {
        return;
    }

    karma::audio_backend::PlayRequest request{};
    request.asset_path = asset_key;
    request.gain = gain;
    request.pitch = pitch;
    request.loop = false;
    audio->playOneShot(request);
}

} // namespace bz3

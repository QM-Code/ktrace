#include "game.hpp"

#include "karma/common/data_path_resolver.hpp"
#include "karma/common/config_helpers.hpp"
#include "karma/common/logging.hpp"
#include "karma/input/input_system.hpp"
#include "karma/renderer/device.hpp"
#include "karma/renderer/layers.hpp"

#include <cmath>
#include <stdexcept>
#include "karma/renderer/render_system.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace bz3 {

namespace {
glm::vec3 ReadVec3Config(const char* path) {
    const auto values = karma::config::ReadRequiredFloatArrayConfig(path);
    if (values.size() != 3) {
        throw std::runtime_error(std::string("Config '") + path + "' must have 3 elements");
    }
    return {values[0], values[1], values[2]};
}
}

Game::Game(std::string model_key) : model_key_(std::move(model_key)) {}

void Game::onStart() {
    if (!graphics || !render) {
        return;
    }

    if (model_key_.empty()) {
        throw std::runtime_error("Missing required render.model config");
    }
    const auto tank_path = karma::data::Resolve(model_key_);
    KARMA_TRACE("render.mesh", "Game: loading tank mesh '{}'", tank_path.string());
    tank_mesh_ = graphics->createMeshFromFile(tank_path);
    if (tank_mesh_ == karma::renderer::kInvalidMesh) {
        spdlog::error("Failed to load tank mesh from {}", tank_path.string());
    }

    karma::renderer::MaterialDesc material{};
    material.base_color = {0.8f, 0.8f, 0.8f, 1.0f};
    tank_material_ = graphics->createMaterial(material);

    tank_entity_ = scene_.createEntity();
    scene_.setMesh(tank_entity_, tank_mesh_);
    scene_.setMaterial(tank_entity_, tank_material_);
    scene_.setLayer(tank_entity_, karma::renderer::kLayerWorld);
    scene_.setTransform(tank_entity_, glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f)));
    render->setScene(&scene_);

    roam_config_.move_speed = karma::config::ReadRequiredFloatConfig("game.roamingCamera.MoveSpeed");
    roam_config_.fast_multiplier = karma::config::ReadRequiredFloatConfig("game.roamingCamera.FastMultiplier");
    roam_config_.look_sensitivity = karma::config::ReadRequiredFloatConfig("game.roamingCamera.LookSensitivity");
    roam_config_.look_smoothing = karma::config::ReadRequiredFloatConfig("game.roamingCamera.LookSmoothing");
    roam_config_.invert_y = karma::config::ReadRequiredBoolConfig("game.roamingCamera.InvertY");
    roam_config_.start_yaw_offset_deg = karma::config::ReadRequiredFloatConfig("game.roamingCamera.StartYawOffsetDeg");
    roam_config_.start_position = ReadVec3Config("game.roamingCamera.StartPosition");
    roam_config_.start_target = ReadVec3Config("game.roamingCamera.StartTarget");

    camera_.position = roam_config_.start_position;
    camera_.target = roam_config_.start_target;
    camera_.fov_y_degrees = karma::config::ReadRequiredFloatConfig("render.camera.FovYDegrees");
    camera_.near_clip = karma::config::ReadRequiredFloatConfig("render.camera.NearClip");
    camera_.far_clip = karma::config::ReadRequiredFloatConfig("render.camera.FarClip");

    glm::vec3 dir = roam_config_.start_target - roam_config_.start_position;
    if (glm::length(dir) < 0.0001f) {
        dir = glm::vec3(0.0f, 0.0f, -1.0f);
    } else {
        dir = glm::normalize(dir);
    }
    yaw_ = std::atan2(dir.x, -dir.z);
    pitch_ = std::asin(glm::clamp(dir.y, -1.0f, 1.0f));
    yaw_ += glm::radians(roam_config_.start_yaw_offset_deg);
    target_yaw_ = yaw_;
    target_pitch_ = pitch_;
    camera_initialized_ = true;
    render->setCamera(camera_);
    if (input) {
        input->setMode(karma::input::InputMode::Roaming);
    }

    KARMA_TRACE("render.frame", "Game started; tank mesh={}, material={}", tank_mesh_, tank_material_);
}

void Game::onUpdate(float dt) {
    if (!render || !input || !camera_initialized_) {
        return;
    }

    float move_speed = roam_config_.move_speed;
    if (input->actionDown("moveFast")) {
        move_speed *= roam_config_.fast_multiplier;
    }

    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    glm::vec3 forward(std::sin(yaw_) * std::cos(pitch_),
                      std::sin(pitch_),
                      std::cos(yaw_) * std::cos(pitch_));
    if (glm::length(forward) < 0.0001f) {
        forward = glm::vec3(0.0f, 0.0f, -1.0f);
    } else {
        forward = glm::normalize(forward);
    }
    glm::vec3 right = glm::normalize(glm::cross(forward, up));

    glm::vec3 move_delta(0.0f);
    if (input->actionDown("moveForward")) move_delta += forward;
    if (input->actionDown("moveBackward")) move_delta -= forward;
    if (input->actionDown("moveRight")) move_delta += right;
    if (input->actionDown("moveLeft")) move_delta -= right;
    if (input->actionDown("moveUp")) move_delta += up;
    if (input->actionDown("moveDown")) move_delta -= up;
    if (glm::length(move_delta) > 0.0f) {
        camera_.position += glm::normalize(move_delta) * move_speed * dt;
    }

    if (input->actionDown("look")) {
        const float invert = roam_config_.invert_y ? -1.0f : 1.0f;
        target_yaw_ -= input->mouseDeltaX() * roam_config_.look_sensitivity;
        target_pitch_ -= input->mouseDeltaY() * roam_config_.look_sensitivity * invert;
        const float max_pitch = glm::radians(89.0f);
        target_pitch_ = glm::clamp(target_pitch_, -max_pitch, max_pitch);
    }

    if (roam_config_.look_smoothing > 0.0f) {
        const float alpha = 1.0f - std::exp(-roam_config_.look_smoothing * dt);
        yaw_ += (target_yaw_ - yaw_) * alpha;
        pitch_ += (target_pitch_ - pitch_) * alpha;
    } else {
        yaw_ = target_yaw_;
        pitch_ = target_pitch_;
    }

    glm::vec3 look_dir(std::sin(yaw_) * std::cos(pitch_),
                       std::sin(pitch_),
                       std::cos(yaw_) * std::cos(pitch_));
    camera_.target = camera_.position + look_dir;
    render->setCamera(camera_);
}

void Game::onShutdown() {
    if (!graphics) {
        return;
    }
    if (tank_mesh_ != karma::renderer::kInvalidMesh) {
        graphics->destroyMesh(tank_mesh_);
    }
    if (tank_material_ != karma::renderer::kInvalidMaterial) {
        graphics->destroyMaterial(tank_material_);
    }
}

} // namespace bz3

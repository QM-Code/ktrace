#include "karma/scene/roaming_camera.hpp"

#include <cmath>
#include <stdexcept>

#include "karma/common/config_helpers.hpp"
#include "karma/common/logging.hpp"
#include "karma/renderer/render_system.hpp"

#include <glm/gtc/constants.hpp>

namespace karma::scene {
namespace {

glm::vec3 ReadVec3Config(const char* path) {
    const auto values = config::ReadRequiredFloatArrayConfig(path);
    if (values.size() != 3) {
        throw std::runtime_error(std::string("Config '") + path + "' must have 3 elements");
    }
    return {values[0], values[1], values[2]};
}

float ClampPitch(float pitch) {
    const float limit = glm::half_pi<float>() - 0.01f;
    return glm::clamp(pitch, -limit, limit);
}

} // namespace

void RoamingCameraController::loadFromConfig() {
    config_.move_speed = config::ReadRequiredFloatConfig("game.roamingCamera.MoveSpeed");
    config_.fast_multiplier = config::ReadRequiredFloatConfig("game.roamingCamera.FastMultiplier");
    config_.look_sensitivity = config::ReadRequiredFloatConfig("game.roamingCamera.LookSensitivity");
    config_.look_smoothing = config::ReadRequiredFloatConfig("game.roamingCamera.LookSmoothing");
    config_.invert_y = config::ReadRequiredBoolConfig("game.roamingCamera.InvertY");
    config_.start_yaw_offset_deg = config::ReadRequiredFloatConfig("game.roamingCamera.StartYawOffsetDeg");
    config_.start_position = ReadVec3Config("game.roamingCamera.StartPosition");
    config_.start_target = ReadVec3Config("game.roamingCamera.StartTarget");

    camera_.fov_y_degrees = config::ReadRequiredFloatConfig("render.camera.FovYDegrees");
    camera_.near_clip = config::ReadRequiredFloatConfig("render.camera.NearClip");
    camera_.far_clip = config::ReadRequiredFloatConfig("render.camera.FarClip");
}

void RoamingCameraController::initialize(renderer::RenderSystem& render) {
    camera_.position = config_.start_position;
    camera_.target = config_.start_target;

    glm::vec3 dir = config_.start_target - config_.start_position;
    if (glm::length(dir) < 0.0001f) {
        dir = glm::vec3(0.0f, 0.0f, -1.0f);
    } else {
        dir = glm::normalize(dir);
    }
    yaw_ = std::atan2(dir.x, -dir.z);
    pitch_ = std::asin(glm::clamp(dir.y, -1.0f, 1.0f));
    yaw_ += glm::radians(config_.start_yaw_offset_deg);
    target_yaw_ = yaw_;
    target_pitch_ = pitch_;

    render.setCamera(camera_);
    initialized_ = true;
}

void RoamingCameraController::update(float dt,
                                     const input::InputContext& input,
                                     renderer::RenderSystem& render) {
    if (!active_ || !initialized_) {
        return;
    }

    float move_speed = config_.move_speed;
    if (input.actionDown("moveFast")) {
        move_speed *= config_.fast_multiplier;
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
    if (input.actionDown("moveForward")) move_delta += forward;
    if (input.actionDown("moveBackward")) move_delta -= forward;
    if (input.actionDown("moveRight")) move_delta += right;
    if (input.actionDown("moveLeft")) move_delta -= right;
    if (input.actionDown("moveUp")) move_delta += up;
    if (input.actionDown("moveDown")) move_delta -= up;
    if (glm::length(move_delta) > 0.0f) {
        camera_.position += glm::normalize(move_delta) * move_speed * dt;
    }

    if (input.actionDown("look")) {
        const float invert = config_.invert_y ? -1.0f : 1.0f;
        target_yaw_ -= input.mouseDeltaX() * config_.look_sensitivity;
        target_pitch_ -= input.mouseDeltaY() * config_.look_sensitivity * invert;
        target_pitch_ = ClampPitch(target_pitch_);
    }

    if (config_.look_smoothing > 0.0f) {
        const float alpha = 1.0f - std::exp(-config_.look_smoothing * dt);
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
    render.setCamera(camera_);

    const bool camera_stats = input.actionDown("cameraStats");
    if (camera_stats && !camera_stats_down_) {
        const float yaw_deg = glm::degrees(yaw_);
        const float pitch_deg = glm::degrees(pitch_);
        KARMA_TRACE("render.camera",
                    "Camera stats: Position=[{:.3f}, {:.3f}, {:.3f}] Target=[{:.3f}, {:.3f}, {:.3f}] "
                    "YawDeg={:.3f} PitchDeg={:.3f}",
                    camera_.position.x, camera_.position.y, camera_.position.z,
                    camera_.target.x, camera_.target.y, camera_.target.z,
                    yaw_deg, pitch_deg);
        KARMA_TRACE("render.camera",
                    "Config snippet: game.roamingCamera.StartPosition=[{:.3f}, {:.3f}, {:.3f}] "
                    "StartTarget=[{:.3f}, {:.3f}, {:.3f}] StartYawOffsetDeg=0.0",
                    camera_.position.x, camera_.position.y, camera_.position.z,
                    camera_.target.x, camera_.target.y, camera_.target.z);
    }
    camera_stats_down_ = camera_stats;
}

} // namespace karma::scene

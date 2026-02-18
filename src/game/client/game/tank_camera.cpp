#include "game.hpp"

#include "client/domain/tank_drive_controller.hpp"
#include "client/game/math.hpp"
#include "karma/renderer/render_system.hpp"

#include <glm/glm.hpp>

namespace bz3 {

void Game::updateTankFollowCamera(float dt_seconds) {
    if (!render || !tank_drive_) {
        return;
    }

    const float camera_yaw = client::game::detail::NormalizeAngle(
        tank_visual_yaw_radians_ + tank_camera_yaw_offset_radians_);
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
        const float alpha = client::game::detail::SmoothAlpha(dt_seconds, tank_camera_smooth_rate_);
        tank_camera_position_ = client::game::detail::LerpVec3(tank_camera_position_, desired_position, alpha);
        tank_camera_target_ = client::game::detail::LerpVec3(tank_camera_target_, desired_target, alpha);
    }

    auto camera = render->camera();
    camera.position = tank_camera_position_;
    camera.target = tank_camera_target_;
    render->setCamera(camera);
}

} // namespace bz3

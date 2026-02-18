#include "game.hpp"

#include "client/domain/tank_drive_controller.hpp"
#include "client/game/math.hpp"
#include "karma/input/input_system.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

namespace bz3 {

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
        static_cast<int>(std::ceil(clamped_dt / std::max(client::game::detail::kEpsilon,
                                                         tank_sim_max_step_seconds_))),
        1,
        12);
    const float step_dt = clamped_dt / static_cast<float>(simulation_steps);
    for (int step = 0; step < simulation_steps; ++step) {
        const auto before = tank_drive_->state();
        tank_drive_->update(step_dt, drive_input);
        auto after = tank_drive_->state();
        after.position = resolveTankCollision(before.position, after.position);
        const glm::vec3 delta = after.position - before.position;
        if (glm::dot(delta, delta) <= client::game::detail::kEpsilon) {
            after.speed = 0.0f;
        }
        tank_drive_->setState(after);
    }

    const auto& sim = tank_drive_->state();
    if (!tank_visual_initialized_) {
        tank_visual_position_ = sim.position;
        tank_visual_yaw_radians_ = client::game::detail::NormalizeAngle(sim.yaw_radians);
        tank_visual_initialized_ = true;
    } else {
        const float alpha = client::game::detail::SmoothAlpha(clamped_dt, tank_visual_smooth_rate_);
        tank_visual_position_ = client::game::detail::LerpVec3(tank_visual_position_, sim.position, alpha);
        tank_visual_yaw_radians_ =
            client::game::detail::LerpAngle(tank_visual_yaw_radians_, sim.yaw_radians, alpha);
    }

    updateLocalTankTransform();
    updateTankFollowCamera(clamped_dt);
}

} // namespace bz3

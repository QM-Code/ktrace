#pragma once

#include <glm/glm.hpp>

#include "karma/input/input_system.hpp"
#include "karma/renderer/types.hpp"

namespace karma::renderer {
class RenderSystem;
}

namespace karma::scene {

class RoamingCameraController {
 public:
    void loadFromConfig();
    void initialize(renderer::RenderSystem& render);
    void update(float dt, const input::InputContext& input, renderer::RenderSystem& render);

    void setActive(bool active) { active_ = active; }
    bool isActive() const { return active_; }

 private:
    struct Config {
        float move_speed = 8.0f;
        float fast_multiplier = 3.0f;
        float look_sensitivity = 0.002f;
        float look_smoothing = 20.0f;
        bool invert_y = false;
        float start_yaw_offset_deg = 0.0f;
        float start_pitch_offset_deg = 0.0f;
        glm::vec3 start_position{0.0f, 6.0f, 12.0f};
        glm::vec3 start_target{0.0f, 1.0f, 0.0f};
    } config_{};

    renderer::CameraData camera_{};
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    float target_yaw_ = 0.0f;
    float target_pitch_ = 0.0f;
    bool initialized_ = false;
    bool active_ = false;
    bool camera_stats_down_ = false;
};

} // namespace karma::scene

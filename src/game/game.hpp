#pragma once

#include "karma/app/game_interface.hpp"
#include "karma/renderer/types.hpp"
#include "karma/scene/scene.hpp"

#include <glm/glm.hpp>

namespace bz3 {

class Game final : public karma::app::GameInterface {
 public:
    explicit Game(std::string model_key = {});
    void onStart() override;
    void onUpdate(float dt) override;
    void onShutdown() override;

 private:
    struct RoamCameraConfig {
        float move_speed = 8.0f;
        float fast_multiplier = 3.0f;
        float look_sensitivity = 0.002f;
        float look_smoothing = 20.0f;
        bool invert_y = false;
        float start_yaw_offset_deg = 0.0f;
        glm::vec3 start_position{0.0f, 6.0f, 12.0f};
        glm::vec3 start_target{0.0f, 1.0f, 0.0f};
    };

    karma::scene::Scene scene_{};
    karma::scene::EntityId tank_entity_ = karma::scene::kInvalidEntity;
    std::string model_key_;
    karma::renderer::MeshId tank_mesh_ = karma::renderer::kInvalidMesh;
    karma::renderer::MaterialId tank_material_ = karma::renderer::kInvalidMaterial;
    RoamCameraConfig roam_config_{};
    karma::renderer::CameraData camera_{};
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    float target_yaw_ = 0.0f;
    float target_pitch_ = 0.0f;
    bool camera_initialized_ = false;
};

} // namespace bz3

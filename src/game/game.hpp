#pragma once

#include "karma/app/game_interface.hpp"
#include "karma/ecs/entity.hpp"
#include "karma/renderer/types.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace bz3::client::net {
class ClientConnection;
enum class AudioEvent;
}

namespace bz3::client::domain {
class TankDriveController;
}

namespace bz3 {

struct GameStartupOptions {
    std::string player_name{};
    std::string connect_addr{};
    uint16_t connect_port = 0;
    bool connect_on_start = false;
};

class Game final : public karma::app::GameInterface {
 public:
    explicit Game(GameStartupOptions options = {});
    ~Game() override;
    void onStart() override;
    void onUpdate(float dt) override;
    void onUiUpdate(float dt, karma::ui::UiDrawContext& ui) override;
    void onShutdown() override;

 private:
    enum class TankCameraMode {
        Fps = 0,
        Chase = 1
    };

    struct TankObstacleRect {
        float min_x = 0.0f;
        float max_x = 0.0f;
        float min_z = 0.0f;
        float max_z = 0.0f;
        float min_y = 0.0f;
        float max_y = 0.0f;
    };

    bool ensureLocalTankEntity();
    void destroyLocalTankEntity();
    void buildTankCollisionCache();
    glm::vec3 resolveTankCollision(const glm::vec3& previous_position,
                                   const glm::vec3& candidate_position) const;
    bool positionBlockedByCollision(const glm::vec3& position) const;
    void updateLocalTank(float dt, bool gameplay_input_enabled);
    void updateLocalTankTransform();
    void updateTankFollowCamera(float dt_seconds);
    void syncInputMode();
    void onAudioEvent(client::net::AudioEvent event);
    void playOneShotAsset(const char* asset_key, float gain = 1.0f, float pitch = 1.0f);

    GameStartupOptions startup_{};
    std::unique_ptr<client::net::ClientConnection> connection_{};
    bool tank_enabled_ = false;
    std::unique_ptr<client::domain::TankDriveController> tank_drive_{};
    karma::ecs::Entity tank_entity_{};
    karma::renderer::MeshId tank_mesh_id_ = karma::renderer::kInvalidMesh;
    karma::renderer::MaterialId tank_material_id_ = karma::renderer::kInvalidMaterial;
    float tank_model_scale_ = 1.0f;
    float tank_model_yaw_offset_radians_ = 0.0f;
    float tank_camera_yaw_offset_radians_ = 0.0f;
    TankCameraMode tank_camera_mode_ = TankCameraMode::Fps;
    float tank_camera_height_ = 4.0f;
    float tank_camera_follow_distance_ = 10.0f;
    float tank_camera_look_ahead_ = 4.0f;
    float tank_camera_forward_offset_ = 0.7f;
    float tank_camera_smooth_rate_ = 18.0f;
    float tank_visual_smooth_rate_ = 22.0f;
    float tank_sim_max_step_seconds_ = 1.0f / 120.0f;
    glm::vec3 tank_visual_position_{0.0f, 0.6f, 0.0f};
    float tank_visual_yaw_radians_ = 0.0f;
    bool tank_visual_initialized_ = false;
    glm::vec3 tank_camera_position_{0.0f, 0.0f, 0.0f};
    glm::vec3 tank_camera_target_{0.0f, 0.0f, -1.0f};
    bool tank_camera_initialized_ = false;
    float tank_collision_radius_ = 1.0f;
    bool tank_collision_ready_ = false;
    std::vector<TankObstacleRect> tank_collision_rects_{};
    bool tank_world_bounds_ready_ = false;
    glm::vec2 tank_world_min_{0.0f, 0.0f};
    glm::vec2 tank_world_max_{0.0f, 0.0f};
    bool console_visible_ = false;
    bool chat_entry_focused_ = false;
    bool console_toggle_was_down_ = false;
    bool escape_was_down_ = false;
    bool chat_was_down_ = false;
    bool spawn_was_down_ = false;
    bool fire_was_down_ = false;
};

} // namespace bz3

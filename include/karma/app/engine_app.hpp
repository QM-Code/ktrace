#pragma once

#include <memory>
#include <optional>

#include "karma/app/game_interface.hpp"
#include "karma/audio/audio_system.hpp"
#include "karma/audio/backend.hpp"
#include "karma/common/simulation_clock.hpp"
#include "karma/platform/window.hpp"
#include "karma/renderer/device.hpp"
#include "karma/renderer/render_system.hpp"
#include "karma/input/input_system.hpp"
#include "karma/physics/backend.hpp"
#include "karma/physics/physics_system.hpp"
#include "karma/ecs/world.hpp"
#include "karma/ui/ui_system.hpp"
#include "karma/scene/roaming_camera.hpp"
#include "karma/scene/scene.hpp"
#include "karma/scene/scene_bootstrap.hpp"

namespace karma::app {

struct EngineConfig {
    platform::WindowConfig window{};
    bool vsync = true;
    int samples = 1;
    bool cursor_visible = true;
    bool enable_anisotropy = true;
    int anisotropy_level = 16;
    bool generate_mipmaps = true;
    int shadow_map_size = 2048;
    int shadow_pcf_radius = 1;
    renderer::CameraData default_camera{};
    renderer::DirectionalLightData default_light{};
    renderer_backend::BackendKind render_backend = renderer_backend::BackendKind::Auto;
    physics_backend::BackendKind physics_backend = physics_backend::BackendKind::Auto;
    audio_backend::BackendKind audio_backend = audio_backend::BackendKind::Auto;
    std::optional<ui::Backend> ui_backend_override{};
    bool enable_audio = true;
    float simulation_fixed_hz = 60.0f;
    float simulation_max_frame_dt = 0.25f;
    int simulation_max_steps = 4;
};

class EngineApp {
 public:
    EngineApp();
    ~EngineApp();

    void start(GameInterface& game, const EngineConfig& config = {});
    void tick();
    bool isRunning() const { return running_; }
    void requestStop();

 private:
    void initSubsystems();
    void shutdownSubsystems();

    GameInterface* game_ = nullptr;
    EngineConfig config_{};
    std::unique_ptr<platform::Window> window_;
    std::unique_ptr<renderer::GraphicsDevice> graphics_;
    std::unique_ptr<renderer::RenderSystem> render_system_;
    ecs::World world_{};
    scene::Scene scene_;
    scene::StartupSceneResources startup_scene_resources_{};
    input::InputContext input_system_{};
    audio::AudioSystem audio_system_{};
    physics::PhysicsSystem physics_system_{};
    scene::RoamingCameraController roaming_camera_{};
    ui::UiSystem ui_system_{};
    common::SimulationClock simulation_clock_{};

    bool running_ = false;
    glm::vec3 last_listener_position_{0.0f, 0.0f, 0.0f};
    bool has_last_listener_position_ = false;
    float sim_trace_window_seconds_ = 0.0f;
    float sim_trace_window_dt_sum_ = 0.0f;
    float sim_trace_window_max_dt_ = 0.0f;
    int sim_trace_window_frames_ = 0;
    int sim_trace_window_steps_sum_ = 0;
    int sim_trace_window_steps_max_ = 0;
};

} // namespace karma::app

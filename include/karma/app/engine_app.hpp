#pragma once

#include <memory>
#include <optional>

#include "karma/app/game_interface.hpp"
#include "karma/platform/window.hpp"
#include "karma/renderer/device.hpp"
#include "karma/renderer/render_system.hpp"
#include "karma/input/input_system.hpp"
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
    std::optional<ui::Backend> ui_backend_override{};
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
    scene::RoamingCameraController roaming_camera_{};
    ui::UiSystem ui_system_{};

    bool running_ = false;
};

} // namespace karma::app

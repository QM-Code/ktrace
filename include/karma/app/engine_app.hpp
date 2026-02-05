#pragma once

#include <memory>

#include "karma/app/game_interface.hpp"
#include "karma/platform/window.hpp"
#include "karma/renderer/device.hpp"

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
};

class EngineApp {
 public:
    EngineApp() = default;
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

    bool running_ = false;
};

} // namespace karma::app

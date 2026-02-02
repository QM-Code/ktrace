#pragma once

#include "karma/app/engine_config.hpp"
#include "karma/app/game_interface.hpp"
#include "karma/core/types.hpp"
#include "karma/systems/system_graph.h"
#include "karma/ecs/systems/camera_sync_system.hpp"
#include "karma/ecs/systems/audio_sync_system.hpp"
#include "karma/ecs/systems/physics_sync_system.hpp"
#include "karma/ecs/systems/procedural_mesh_sync_system.hpp"
#include "karma/ecs/systems/render_sync_system.hpp"
#include "karma/ecs/world.h"
#include "karma/ecs/systems/renderer_system.hpp"
#include "karma/graphics/resources.hpp"
#include "karma/renderer/renderer_context.hpp"
#include "karma/app/ui_context.h"
#include <memory>

namespace graphics {
class GraphicsDevice;
}
class Input;
class Audio;
class PhysicsWorld;
namespace engine::renderer {
class RendererCore;
struct RendererContext;
}
namespace platform {
class Window;
}

namespace karma::app {
struct EngineContext {
    platform::Window *window = nullptr;
    graphics::GraphicsDevice *graphics = nullptr;
    Input *input = nullptr;
    Audio *audio = nullptr;
    PhysicsWorld *physics = nullptr;
    karma::ecs::World *ecsWorld = nullptr;
    graphics::ResourceRegistry *resources = nullptr;
    graphics::MaterialId defaultMaterial = graphics::kInvalidMaterial;
    engine::renderer::RendererContext rendererContext{};
    engine::renderer::RendererCore *rendererCore = nullptr;
};

class EngineApp {
public:
    EngineApp();
    ~EngineApp();

    void setGame(GameInterface *game);
    void setConfig(const EngineConfig &config);
    EngineConfig &config();
    const EngineConfig &config() const;
    void setUi(std::unique_ptr<UiLayer> ui);
    bool start(GameInterface &game, const EngineConfig &config);
    void tick();
    bool isRunning() const;
    EngineContext &context();
    const EngineContext &context() const;

private:
    GameInterface *game_ = nullptr;
    EngineConfig config_{};
    bool running_ = false;
    bool started_ = false;
    float fixed_accumulator_ = 0.0f;
    TimeUtils::time last_tick_time_ = TimeUtils::GetCurrentTime();
    EngineContext context_{};
    karma::ecs::World ecsWorld_{};
    systems::SystemGraph systemGraph_{};
    ecs::RendererSystem rendererSystem_{};
    ecs::RenderSyncSystem renderSyncSystem_{};
    ecs::PhysicsSyncSystem physicsSyncSystem_{};
    ecs::AudioSyncSystem audioSyncSystem_{};
    ecs::CameraSyncSystem cameraSyncSystem_{};
    ecs::ProceduralMeshSyncSystem proceduralMeshSyncSystem_{};
    std::unique_ptr<graphics::ResourceRegistry> resources_{};
    std::unique_ptr<UiLayer> ui_layer_{};
    UIContext ui_context_{};
    int lastFramebufferWidth_ = 0;
    int lastFramebufferHeight_ = 0;
};
} // namespace karma::app

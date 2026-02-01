#pragma once

#include "karma/components/mesh.h"
#include "karma/components/transform.h"
#include "karma/ecs/world.h"
#include "karma/renderer/renderer_context.hpp"
#include "karma/renderer/renderer_core.hpp"
#include "karma/renderer/scene_renderer.hpp"
#include "karma/graphics/ui_render_target_bridge.hpp"
#include "renderer/radar_renderer.hpp"
#include "karma_extras/ui/bridges/ui_render_target_bridge.hpp"

#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>


namespace platform {
class Window;
}
namespace graphics {
}

class Renderer {

private:
    std::unique_ptr<engine::renderer::RendererCore> core_;
    platform::Window* window = nullptr;

    render_id nextId = 1;

    std::unique_ptr<game::renderer::RadarRenderer> radarRenderer_;
    std::unique_ptr<graphics_backend::UiRenderTargetBridge> imguiBridge_;
    std::unique_ptr<ui::UiRenderTargetBridge> uiRenderTargetBridge_;
    struct RadarEcsEntry {
        render_id id = 0;
        std::string mesh_key{};
    };
    std::unordered_map<karma::ecs::Entity, RadarEcsEntry> radarEcsEntities_;
    struct RadarEcsCircleEntry {
        render_id id = 0;
        float radius = 1.0f;
    };
    std::unordered_map<karma::ecs::Entity, RadarEcsCircleEntry> radarEcsCircles_;

    karma::ecs::World *ecsWorld = nullptr;
    bool ecsRadarSyncEnabled = true;


    void syncEcsRadar();

public:
    Renderer(platform::Window &window);
    ~Renderer();
    void renderRadar(const glm::vec3 &cameraPosition, const glm::quat &cameraRotation);
    void setEcsWorld(karma::ecs::World *world);
    void setMainLayer(graphics::LayerId layer) { if (core_) { core_->context().mainLayer = layer; } }
    graphics::TextureHandle getRadarTexture() const;
    ui::UiRenderTargetBridge* getUiRenderTargetBridge() const;
    void configureRadar(const game::renderer::RadarConfig& config);
    engine::renderer::RendererCore *getRendererCore() const { return core_.get(); }

};

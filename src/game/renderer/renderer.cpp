#include "renderer/renderer.hpp"
#include "renderer/radar_components.hpp"
#include "karma/graphics/ui_render_target_bridge.hpp"
#include "karma/graphics/resources.hpp"

#if defined(KARMA_UI_BACKEND_IMGUI)
#if defined(KARMA_RENDER_BACKEND_BGFX)
#include "karma_extras/ui/platform/imgui/renderer_bgfx.hpp"
#elif defined(KARMA_RENDER_BACKEND_DILIGENT)
#include "karma_extras/ui/platform/imgui/renderer_diligent.hpp"
#endif
#endif

#include "karma/platform/window.hpp"
#include "spdlog/spdlog.h"
#include <cmath>
#include <unordered_set>

namespace components = karma::components;

namespace {

class RendererUiBridge final : public ui::UiRenderTargetBridge {
public:
    explicit RendererUiBridge(graphics_backend::UiRenderTargetBridge* bridge)
        : bridge_(bridge) {}

    void* toImGuiTextureId(const graphics::TextureHandle& texture) const override {
        return bridge_ ? bridge_->toImGuiTextureId(texture) : nullptr;
    }

    void rebuildImGuiFonts(ImFontAtlas* atlas) override {
        if (bridge_) {
            bridge_->rebuildImGuiFonts(atlas);
        }
    }

    void renderImGuiToTarget(ImDrawData* drawData) override {
        if (bridge_) {
            bridge_->renderImGuiToTarget(drawData);
        }
    }

    bool isImGuiReady() const override {
        return bridge_ && bridge_->isImGuiReady();
    }

    void ensureImGuiRenderTarget(int width, int height) override {
        if (bridge_) {
            bridge_->ensureImGuiRenderTarget(width, height);
        }
    }

    graphics::TextureHandle getImGuiRenderTarget() const override {
        return bridge_ ? bridge_->getImGuiRenderTarget() : graphics::TextureHandle{};
    }

private:
    graphics_backend::UiRenderTargetBridge* bridge_ = nullptr;
};

} // namespace

Renderer::Renderer(platform::Window &windowIn)
    : window(&windowIn) {
    core_ = std::make_unique<engine::renderer::RendererCore>(windowIn);
    radarRenderer_ = std::make_unique<game::renderer::RadarRenderer>(core_->device(), core_->scene());
#if defined(KARMA_UI_BACKEND_IMGUI)
#if defined(KARMA_RENDER_BACKEND_BGFX)
    imguiBridge_ = std::make_unique<graphics_backend::BgfxRenderer>();
#elif defined(KARMA_RENDER_BACKEND_DILIGENT)
    imguiBridge_ = std::make_unique<graphics_backend::DiligentRenderer>();
#endif
    if (imguiBridge_) {
        uiRenderTargetBridge_ = std::make_unique<RendererUiBridge>(imguiBridge_.get());
    }
#endif

}

Renderer::~Renderer() = default;

void Renderer::renderRadar(const glm::vec3 &cameraPosition, const glm::quat &cameraRotation) {
    if (!core_) {
        return;
    }
    if (radarRenderer_) {
        const float fovDeg = core_->context().fov;
        const float halfVertRad = glm::radians(fovDeg * 0.5f);
        const float halfHorizRad = std::atan(std::tan(halfVertRad) * core_->context().aspect);
        radarRenderer_->setFovDegrees(glm::degrees(halfHorizRad * 2.0f));
    }
    if (ecsRadarSyncEnabled && ecsWorld && radarRenderer_) {
        syncEcsRadar();
    }
    if (radarRenderer_) {
        radarRenderer_->render(cameraPosition, cameraRotation);
    }
}

void Renderer::setEcsWorld(karma::ecs::World *world) {
    ecsWorld = world;
}

void Renderer::syncEcsRadar() {
    auto &radarTags = ecsWorld->storage<game::renderer::RadarRenderable>();
    auto &meshes = ecsWorld->storage<components::MeshComponent>();
    auto &transforms = ecsWorld->storage<components::TransformComponent>();
    if (radarTags.denseEntities().empty()) {
        if (!radarEcsEntities_.empty()) {
            for (const auto &entry : radarEcsEntities_) {
                radarRenderer_->destroy(entry.second.id);
            }
            radarEcsEntities_.clear();
        }
    } else {
        std::unordered_set<karma::ecs::Entity> seen;
        seen.reserve(radarTags.denseEntities().size());

        for (const karma::ecs::Entity entity : radarTags.denseEntities()) {
            const auto &tag = radarTags.get(entity);
            if (!tag.enabled) {
                continue;
            }
            if (!meshes.has(entity)) {
                continue;
            }
            const std::string &meshKey = meshes.get(entity).mesh_key;
            if (meshKey.empty()) {
                continue;
            }

            seen.insert(entity);
            auto it = radarEcsEntities_.find(entity);
            if (it == radarEcsEntities_.end()) {
                const render_id id = nextId++;
                radarRenderer_->setModel(id, meshKey, true);
                radarRenderer_->setPosition(id, glm::vec3(0.0f));
                radarRenderer_->setRotation(id, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
                radarRenderer_->setScale(id, glm::vec3(1.0f));
                radarEcsEntities_.insert({entity, RadarEcsEntry{id, meshKey}});
                spdlog::info("Renderer: ECS radar sync created (ecs_entity={}, render_id={}, mesh={})",
                             entity.index, id, meshKey);
            } else if (it->second.mesh_key != meshKey) {
                radarRenderer_->setModel(it->second.id, meshKey, true);
                it->second.mesh_key = meshKey;
                spdlog::info("Renderer: ECS radar sync updated (ecs_entity={}, render_id={}, mesh={})",
                             entity.index, it->second.id, meshKey);
            }

            if (transforms.has(entity)) {
                const components::TransformComponent &xform = transforms.get(entity);
                const auto &entry = radarEcsEntities_.find(entity)->second;
                radarRenderer_->setPosition(entry.id, xform.position);
                radarRenderer_->setRotation(entry.id, xform.rotation);
                radarRenderer_->setScale(entry.id, xform.scale);
            }
        }

        for (auto it = radarEcsEntities_.begin(); it != radarEcsEntities_.end(); ) {
            if (seen.find(it->first) == seen.end()) {
                radarRenderer_->destroy(it->second.id);
                it = radarEcsEntities_.erase(it);
            } else {
                ++it;
            }
        }
    }

    auto &radarCircles = ecsWorld->storage<game::renderer::RadarCircle>();
    if (radarCircles.denseEntities().empty()) {
        if (!radarEcsCircles_.empty()) {
            for (const auto &entry : radarEcsCircles_) {
                radarRenderer_->destroy(entry.second.id);
            }
            radarEcsCircles_.clear();
        }
        return;
    }

    std::unordered_set<karma::ecs::Entity> seenCircles;
    seenCircles.reserve(radarCircles.denseEntities().size());

    for (const karma::ecs::Entity entity : radarCircles.denseEntities()) {
        const auto &circle = radarCircles.get(entity);
        if (!circle.enabled) {
            continue;
        }
        if (!transforms.has(entity)) {
            continue;
        }

        seenCircles.insert(entity);
        auto it = radarEcsCircles_.find(entity);
        if (it == radarEcsCircles_.end()) {
            const render_id id = nextId++;
            radarRenderer_->setRadarCircleGraphic(id, circle.radius);
            radarRenderer_->setPosition(id, transforms.get(entity).position);
            radarEcsCircles_.insert({entity, RadarEcsCircleEntry{id, circle.radius}});
        } else {
            if (std::abs(it->second.radius - circle.radius) > 0.0001f) {
                radarRenderer_->setRadarCircleGraphic(it->second.id, circle.radius);
                it->second.radius = circle.radius;
            }
            radarRenderer_->setPosition(it->second.id, transforms.get(entity).position);
        }
    }

    for (auto it = radarEcsCircles_.begin(); it != radarEcsCircles_.end(); ) {
        if (seenCircles.find(it->first) == seenCircles.end()) {
            radarRenderer_->destroy(it->second.id);
            it = radarEcsCircles_.erase(it);
        } else {
            ++it;
        }
    }
}

graphics::TextureHandle Renderer::getRadarTexture() const {
    return radarRenderer_ ? radarRenderer_->getRadarTexture() : graphics::TextureHandle{};
}

ui::UiRenderTargetBridge* Renderer::getUiRenderTargetBridge() const {
    return uiRenderTargetBridge_.get();
}

void Renderer::configureRadar(const game::renderer::RadarConfig& config) {
    if (radarRenderer_) {
        radarRenderer_->configure(config);
    }
}

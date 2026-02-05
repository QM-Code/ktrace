#include "karma/renderer/render_system.hpp"

#include "karma/common/logging.hpp"
#include "karma/scene/scene.hpp"

namespace karma::renderer {

RenderSystem::RenderSystem(GraphicsDevice& graphics) : graphics_(graphics) {
    camera_.position = {0.0f, 6.0f, 12.0f};
    camera_.target = {0.0f, 1.0f, 0.0f};
    KARMA_TRACE("render.system",
                "RenderSystem: default light dir=({:.2f},{:.2f},{:.2f}) color=({:.2f},{:.2f},{:.2f}) ambient=({:.2f},{:.2f},{:.2f}) unlit={:.2f}",
                light_.direction.x, light_.direction.y, light_.direction.z,
                light_.color.r, light_.color.g, light_.color.b,
                light_.ambient.r, light_.ambient.g, light_.ambient.b,
                light_.unlit);
}

void RenderSystem::beginFrame(int width, int height, float dt) {
    graphics_.beginFrame(width, height, dt);
}

void RenderSystem::submit(const DrawItem& item) {
    queues_[item.layer].push_back(item);
}

void RenderSystem::setCamera(const CameraData& camera) {
    camera_ = camera;
}

void RenderSystem::setDirectionalLight(const DirectionalLightData& light) {
    light_ = light;
    KARMA_TRACE("render.system",
                "RenderSystem: light dir=({:.2f},{:.2f},{:.2f}) color=({:.2f},{:.2f},{:.2f}) ambient=({:.2f},{:.2f},{:.2f}) unlit={:.2f}",
                light_.direction.x, light_.direction.y, light_.direction.z,
                light_.color.r, light_.color.g, light_.color.b,
                light_.ambient.r, light_.ambient.g, light_.ambient.b,
                light_.unlit);
}

void RenderSystem::setScene(karma::scene::Scene* scene) {
    scene_ = scene;
}

void RenderSystem::renderFrame() {
    graphics_.setCamera(camera_);
    graphics_.setDirectionalLight(light_);

    if (scene_) {
        for (const auto& entity : scene_->entities()) {
            if (!entity.alive) {
                continue;
            }
            if (entity.mesh == kInvalidMesh || entity.material == kInvalidMaterial) {
                continue;
            }
            DrawItem item{};
            item.mesh = entity.mesh;
            item.material = entity.material;
            item.transform = entity.transform;
            item.layer = entity.layer;
            queues_[item.layer].push_back(item);
        }
    }

    for (auto& [layer, items] : queues_) {
        if (items.empty()) {
            continue;
        }
        for (const auto& item : items) {
            graphics_.submit(item);
        }
        graphics_.renderLayer(layer);
    }

    queues_.clear();
}

void RenderSystem::endFrame() {
    graphics_.endFrame();
}

} // namespace karma::renderer

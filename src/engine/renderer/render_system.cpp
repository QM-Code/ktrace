#include "karma/renderer/render_system.hpp"

#include "karma/common/logging.hpp"
#include "karma/ecs/world.hpp"
#include "karma/scene/components.hpp"
#include <set>
#include <string>

namespace karma::renderer {

RenderSystem::RenderSystem(GraphicsDevice& graphics) : graphics_(graphics) {
    camera_.position = {0.0f, 6.0f, 12.0f};
    camera_.target = {0.0f, 1.0f, 0.0f};
    KARMA_TRACE("render.system",
                "RenderSystem: default light dir=({:.2f},{:.2f},{:.2f}) color=({:.2f},{:.2f},{:.2f}) ambient=({:.2f},{:.2f},{:.2f}) unlit={:.2f} shadows(enabled={} strength={:.2f} bias={:.4f} recv={:.3f} norm={:.3f} rasterDepth={:.4f} rasterSlope={:.3f} extent={:.1f} map={} pcf={} tris={} mode={})",
                light_.direction.x, light_.direction.y, light_.direction.z,
                light_.color.r, light_.color.g, light_.color.b,
                light_.ambient.r, light_.ambient.g, light_.ambient.b,
                light_.unlit,
                light_.shadow.enabled ? 1 : 0,
                light_.shadow.strength,
                light_.shadow.bias,
                light_.shadow.receiver_bias_scale,
                light_.shadow.normal_bias_scale,
                light_.shadow.raster_depth_bias,
                light_.shadow.raster_slope_bias,
                light_.shadow.extent,
                light_.shadow.map_size,
                light_.shadow.pcf_radius,
                light_.shadow.triangle_budget,
                DirectionalLightData::ShadowExecutionModeToken(light_.shadow.execution_mode));
    KARMA_TRACE("render.system",
                "RenderSystem: default environment enabled={} sky=({:.2f},{:.2f},{:.2f}) ground=({:.2f},{:.2f},{:.2f}) diffuse={:.2f} specular={:.2f} exposure={:.2f}",
                environment_.enabled ? 1 : 0,
                environment_.sky_color.r, environment_.sky_color.g, environment_.sky_color.b,
                environment_.ground_color.r, environment_.ground_color.g, environment_.ground_color.b,
                environment_.diffuse_strength,
                environment_.specular_strength,
                environment_.skybox_exposure);
}

void RenderSystem::beginFrame(int width, int height, float dt) {
    graphics_.beginFrame(width, height, dt);
}

void RenderSystem::submit(const DrawItem& item) {
    queues_[item.layer].push_back(item);
}

void RenderSystem::submitDebugLine(const DebugLineItem& line) {
    debug_line_queues_[line.layer].push_back(line);
}

void RenderSystem::setCamera(const CameraData& camera) {
    camera_ = camera;
}

const CameraData& RenderSystem::camera() const {
    return camera_;
}

void RenderSystem::setDirectionalLight(const DirectionalLightData& light) {
    light_ = light;
    KARMA_TRACE("render.system",
                "RenderSystem: light dir=({:.2f},{:.2f},{:.2f}) color=({:.2f},{:.2f},{:.2f}) ambient=({:.2f},{:.2f},{:.2f}) unlit={:.2f} shadows(enabled={} strength={:.2f} bias={:.4f} recv={:.3f} norm={:.3f} rasterDepth={:.4f} rasterSlope={:.3f} extent={:.1f} map={} pcf={} tris={} mode={})",
                light_.direction.x, light_.direction.y, light_.direction.z,
                light_.color.r, light_.color.g, light_.color.b,
                light_.ambient.r, light_.ambient.g, light_.ambient.b,
                light_.unlit,
                light_.shadow.enabled ? 1 : 0,
                light_.shadow.strength,
                light_.shadow.bias,
                light_.shadow.receiver_bias_scale,
                light_.shadow.normal_bias_scale,
                light_.shadow.raster_depth_bias,
                light_.shadow.raster_slope_bias,
                light_.shadow.extent,
                light_.shadow.map_size,
                light_.shadow.pcf_radius,
                light_.shadow.triangle_budget,
                DirectionalLightData::ShadowExecutionModeToken(light_.shadow.execution_mode));
}

void RenderSystem::setEnvironmentLighting(const EnvironmentLightingData& environment) {
    environment_ = environment;
    KARMA_TRACE("render.system",
                "RenderSystem: environment enabled={} sky=({:.2f},{:.2f},{:.2f}) ground=({:.2f},{:.2f},{:.2f}) diffuse={:.2f} specular={:.2f} exposure={:.2f}",
                environment_.enabled ? 1 : 0,
                environment_.sky_color.r, environment_.sky_color.g, environment_.sky_color.b,
                environment_.ground_color.r, environment_.ground_color.g, environment_.ground_color.b,
                environment_.diffuse_strength,
                environment_.specular_strength,
                environment_.skybox_exposure);
}

void RenderSystem::setWorld(karma::ecs::World* world) {
    world_ = world;
}

void RenderSystem::renderFrame() {
    graphics_.setCamera(camera_);
    graphics_.setDirectionalLight(light_);
    graphics_.setEnvironmentLighting(environment_);

    if (world_) {
        ecs::World& world = *world_;
        const std::vector<scene::EntityId> entities =
            world.view<scene::TransformComponent, scene::RenderComponent>();
        size_t drawable_count = 0;
        for (const scene::EntityId entity : entities) {
            const scene::TransformComponent& transform = world.get<scene::TransformComponent>(entity);
            const scene::RenderComponent& render = world.get<scene::RenderComponent>(entity);
            if (!render.visible ||
                render.mesh == kInvalidMesh ||
                render.material == kInvalidMaterial) {
                continue;
            }
            ++drawable_count;
            DrawItem item{};
            item.mesh = render.mesh;
            item.material = render.material;
            item.transform = transform.world;
            item.layer = render.layer;
            item.casts_shadow = render.casts_shadow;
            queues_[item.layer].push_back(item);
        }
        KARMA_TRACE_CHANGED("ecs.world",
                            std::to_string(entities.size()) + ":" + std::to_string(drawable_count),
                            "RenderSystem: ecs view entities={} drawable={}",
                            entities.size(),
                            drawable_count);
    }

    std::set<LayerId> active_layers{};
    for (const auto& [layer, items] : queues_) {
        if (!items.empty()) {
            active_layers.insert(layer);
        }
    }
    for (const auto& [layer, lines] : debug_line_queues_) {
        if (!lines.empty()) {
            active_layers.insert(layer);
        }
    }

    for (const LayerId layer : active_layers) {
        auto item_it = queues_.find(layer);
        if (item_it != queues_.end()) {
            for (const auto& item : item_it->second) {
                graphics_.submit(item);
            }
        }

        auto line_it = debug_line_queues_.find(layer);
        if (line_it != debug_line_queues_.end()) {
            for (const auto& line : line_it->second) {
                graphics_.submitDebugLine(line);
            }
        }

        graphics_.renderLayer(layer);
    }

    queues_.clear();
    debug_line_queues_.clear();
}

void RenderSystem::endFrame() {
    graphics_.endFrame();
}

} // namespace karma::renderer

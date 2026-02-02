#pragma once

#include "karma/graphics/device.hpp"
#include "karma/graphics/types.hpp"
#include "karma/components/mesh.h"
#include "karma/components/transform.h"
#include "karma/ecs/world.h"
#include "karma_extras/ecs/render_components.h"
#include "spdlog/spdlog.h"
#include <cstdlib>
#include <unordered_map>

namespace graphics {
class GraphicsDevice;
}

namespace karma::ecs {
namespace components = karma::components;

class RendererSystem {
public:
    void setDefaultMaterial(graphics::MaterialId material) { defaultMaterial_ = material; }

    void update(karma::ecs::World &world, graphics::GraphicsDevice *graphics, float dt) {
        if (!graphics) {
            return;
        }
        if (debugEnabled()) {
            debugAccum_ += dt;
            if (debugAccum_ >= 1.0f) {
                debugAccum_ = 0.0f;
                spdlog::info("RendererSystem: entities={} meshes={} renderEntities={}",
                             world.storage<components::TransformComponent>().denseEntities().size(),
                             world.storage<RenderMesh>().denseEntities().size(),
                             world.storage<RenderEntity>().denseEntities().size());
            }
        }
        for (auto it = entities_.begin(); it != entities_.end(); ) {
            if (!world.isAlive(it->first)) {
                if (it->second.entity != graphics::kInvalidEntity) {
                    graphics->destroyEntity(it->second.entity);
                }
                it = entities_.erase(it);
            } else {
                ++it;
            }
        }
        auto &meshes = world.storage<RenderMesh>();
        auto &materials = world.storage<Material>();
        auto &renderEntities = world.storage<RenderEntity>();
        auto &transparency = world.storage<Transparency>();
        auto &layers = world.storage<RenderLayer>();
        auto &meshComponents = world.storage<components::MeshComponent>();
        auto &transforms = world.storage<components::TransformComponent>();
        for (const karma::ecs::Entity entity : transforms.denseEntities()) {
            const components::TransformComponent &transform = transforms.get(entity);
            graphics::EntityId gfxEntity = graphics::kInvalidEntity;
            if (renderEntities.has(entity)) {
                gfxEntity = renderEntities.get(entity).entityId;
            }
            graphics::LayerId desiredLayer = 0;
            if (layers.has(entity)) {
                desiredLayer = layers.get(entity).layer;
            }
            if (auto handleIt = entities_.find(entity); handleIt != entities_.end()) {
                if (handleIt->second.layer != desiredLayer) {
                    if (handleIt->second.entity != graphics::kInvalidEntity) {
                        graphics->destroyEntity(handleIt->second.entity);
                    }
                    handleIt->second.entity = graphics::kInvalidEntity;
                    handleIt->second.layer = desiredLayer;
                    gfxEntity = graphics::kInvalidEntity;
                }
            }

            if (gfxEntity == graphics::kInvalidEntity) {
                const bool hasMesh = meshes.has(entity);
                const bool hasMeshKey = meshComponents.has(entity);
                if (!hasMesh && !hasMeshKey) {
                    continue;
                }
                gfxEntity = graphics->createEntity(desiredLayer);
                entities_.insert({entity, RenderHandle{gfxEntity, desiredLayer}});
                if (hasMeshKey && !meshComponents.get(entity).mesh_key.empty()) {
                    graphics->setEntityModel(gfxEntity, meshComponents.get(entity).mesh_key, graphics::kInvalidMaterial);
                } else if (hasMesh) {
                    graphics->setEntityMesh(gfxEntity, meshes.get(entity).meshId, graphics::kInvalidMaterial);
                }
                world.add(entity, RenderEntity{gfxEntity});
            }

            if (gfxEntity != graphics::kInvalidEntity) {
                if (meshComponents.has(entity) && !meshComponents.get(entity).mesh_key.empty()) {
                    graphics::MaterialId material = graphics::kInvalidMaterial;
                    if (materials.has(entity)) {
                        material = materials.get(entity).materialId;
                    }
                    graphics->setEntityModel(gfxEntity, meshComponents.get(entity).mesh_key, material);
                } else if (meshes.has(entity)) {
                    graphics::MaterialId material = defaultMaterial_;
                    if (materials.has(entity)) {
                        material = materials.get(entity).materialId;
                    }
                    graphics->setEntityMesh(gfxEntity, meshes.get(entity).meshId, material);
                }
                if (transparency.has(entity)) {
                    graphics->setTransparency(gfxEntity, transparency.get(entity).enabled);
                }
                graphics->setPosition(gfxEntity, transform.position);
                graphics->setRotation(gfxEntity, transform.rotation);
                graphics->setScale(gfxEntity, transform.scale);
            } else if (meshes.has(entity)) {
                spdlog::warn("RendererSystem: failed to create render entity for ECS id {}", entity.index);
            }
        }
    }

private:
    struct RenderHandle {
        graphics::EntityId entity = graphics::kInvalidEntity;
        graphics::LayerId layer = 0;
    };
    std::unordered_map<karma::ecs::Entity, RenderHandle> entities_;
    graphics::MaterialId defaultMaterial_ = graphics::kInvalidMaterial;
    float debugAccum_ = 0.0f;

    bool debugEnabled() const {
        if (debugCached_ != -1) {
            return debugCached_ == 1;
        }
        debugCached_ = std::getenv("KARMA_ECS_RENDER_DEBUG") ? 1 : 0;
        return debugCached_ == 1;
    }

    mutable int debugCached_ = -1;
};

} // namespace karma::ecs

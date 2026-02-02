#pragma once

#include "karma/components/mesh.h"
#include "karma/ecs/world.h"
#include "karma_extras/ecs/render_components.h"
#include "karma/graphics/resources.hpp"
#include "karma/graphics/types.hpp"

namespace karma::ecs {
namespace components = karma::components;

class RenderSyncSystem {
public:
    void update(karma::ecs::World &world,
                graphics::ResourceRegistry *resources,
                graphics::MaterialId defaultMaterial) {
        if (!resources) {
            return;
        }
        auto &meshStorage = world.storage<components::MeshComponent>();
        auto &renderMeshStorage = world.storage<RenderMesh>();
        auto &renderEntityStorage = world.storage<RenderEntity>();
        for (const auto entity : meshStorage.denseEntities()) {
            const components::MeshComponent &mesh = meshStorage.get(entity);
            if (mesh.mesh_key.empty()) {
                continue;
            }
            if (!renderEntityStorage.has(entity) && !renderMeshStorage.has(entity)) {
                const graphics::MeshId meshId = resources->loadMesh(mesh.mesh_key);
                if (meshId != graphics::kInvalidMesh) {
                    world.add(entity, RenderMesh{meshId});
                }
            }
        }
        auto &materialStorage = world.storage<MaterialComponent>();
        auto &renderMaterialStorage = world.storage<Material>();
        for (const auto entity : materialStorage.denseEntities()) {
            const MaterialComponent &material = materialStorage.get(entity);
            if (material.materialId == graphics::kInvalidMaterial) {
                continue;
            }
            world.add(entity, Material{material.materialId});
        }
        if (defaultMaterial != graphics::kInvalidMaterial) {
            for (const auto entity : renderMeshStorage.denseEntities()) {
                if (!renderMaterialStorage.has(entity)) {
                    world.add(entity, Material{defaultMaterial});
                }
            }
        }
    }
};

} // namespace karma::ecs

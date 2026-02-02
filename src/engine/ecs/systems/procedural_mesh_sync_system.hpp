#pragma once

#include "karma/graphics/device.hpp"
#include "karma/ecs/world.h"
#include "karma_extras/ecs/render_components.h"

namespace karma::ecs {

class ProceduralMeshSyncSystem {
public:
    void update(karma::ecs::World &world, graphics::GraphicsDevice *graphics) {
        if (!graphics) {
            return;
        }
        auto &procedurals = world.storage<ProceduralMesh>();
        auto &renderMeshes = world.storage<RenderMesh>();
        for (const auto entity : procedurals.denseEntities()) {
            const ProceduralMesh &proc = procedurals.get(entity);
            if (!proc.dirty && renderMeshes.has(entity)) {
                continue;
            }
            const graphics::MeshId meshId = graphics->createMesh(proc.mesh);
            if (meshId != graphics::kInvalidMesh) {
                world.add(entity, RenderMesh{meshId});
                procedurals.get(entity).dirty = false;
            }
        }
    }
};

} // namespace karma::ecs

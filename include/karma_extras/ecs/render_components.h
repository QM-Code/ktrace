#pragma once

#include "karma/graphics/types.hpp"
#include <cstdint>

namespace karma::ecs {

struct RenderMesh {
    uint32_t meshId = 0;
};

struct Material {
    uint32_t materialId = 0;
};

struct MaterialComponent {
    graphics::MaterialId materialId = graphics::kInvalidMaterial;
};

struct RenderEntity {
    graphics::EntityId entityId = graphics::kInvalidEntity;
};

struct RenderLayer {
    graphics::LayerId layer = 0;
};

struct Transparency {
    bool enabled = false;
};

struct ProceduralMesh {
    graphics::MeshData mesh{};
    bool dirty = true;
};

} // namespace karma::ecs

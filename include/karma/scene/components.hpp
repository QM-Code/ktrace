#pragma once

#include <vector>

#include "karma/ecs/entity.hpp"
#include "karma/renderer/types.hpp"

namespace karma::scene {

using EntityId = ecs::Entity;
constexpr EntityId kInvalidEntity{};

struct TransformComponent {
    glm::mat4 local{1.0f};
    glm::mat4 world{1.0f};
};

struct RenderComponent {
    renderer::MeshId mesh = renderer::kInvalidMesh;
    renderer::MaterialId material = renderer::kInvalidMaterial;
    renderer::LayerId layer = 0;
    bool visible = true;
    bool casts_shadow = true;
};

struct HierarchyComponent {
    EntityId parent = kInvalidEntity;
    std::vector<EntityId> children{};
};

} // namespace karma::scene

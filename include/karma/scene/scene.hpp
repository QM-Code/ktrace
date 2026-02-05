#pragma once

#include <vector>

#include "karma/renderer/types.hpp"

namespace karma::scene {

using EntityId = uint32_t;
constexpr EntityId kInvalidEntity = 0;

struct Entity {
    bool alive = true;
    glm::mat4 transform{1.0f};
    renderer::MeshId mesh = renderer::kInvalidMesh;
    renderer::MaterialId material = renderer::kInvalidMaterial;
    renderer::LayerId layer = 0;
};

class Scene {
 public:
    EntityId createEntity() {
        Entity entity{};
        entities_.push_back(entity);
        return static_cast<EntityId>(entities_.size());
    }

    void destroyEntity(EntityId id) {
        Entity* entity = getEntity(id);
        if (entity) {
            entity->alive = false;
        }
    }

    void setTransform(EntityId id, const glm::mat4& transform) {
        if (auto* entity = getEntity(id)) {
            entity->transform = transform;
        }
    }

    void setMesh(EntityId id, renderer::MeshId mesh) {
        if (auto* entity = getEntity(id)) {
            entity->mesh = mesh;
        }
    }

    void setMaterial(EntityId id, renderer::MaterialId material) {
        if (auto* entity = getEntity(id)) {
            entity->material = material;
        }
    }

    void setLayer(EntityId id, renderer::LayerId layer) {
        if (auto* entity = getEntity(id)) {
            entity->layer = layer;
        }
    }

    const std::vector<Entity>& entities() const { return entities_; }

 private:
    Entity* getEntity(EntityId id) {
        if (id == kInvalidEntity) {
            return nullptr;
        }
        const size_t index = static_cast<size_t>(id - 1);
        if (index >= entities_.size()) {
            return nullptr;
        }
        return &entities_[index];
    }

    std::vector<Entity> entities_;
};

} // namespace karma::scene

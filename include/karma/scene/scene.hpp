#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "karma/ecs/world.hpp"
#include "karma/scene/components.hpp"

namespace karma::scene {

class Scene {
 public:
    explicit Scene(ecs::World& world) : world_(&world) {}

    void setWorld(ecs::World& world) {
        world_ = &world;
    }

    bool setParent(EntityId child, EntityId parent) {
        ecs::World& world = worldRef();
        if (!world.isAlive(child)) {
            return false;
        }
        if (parent.isValid() && !world.isAlive(parent)) {
            return false;
        }
        if (parent == child) {
            return false;
        }
        if (parent.isValid() && wouldCreateCycle(child, parent)) {
            return false;
        }

        detachFromParent(child);

        if (!parent.isValid()) {
            cleanupHierarchy(child);
            return true;
        }

        HierarchyComponent& child_hierarchy = ensureHierarchy(child);
        child_hierarchy.parent = parent;

        HierarchyComponent& parent_hierarchy = ensureHierarchy(parent);
        if (std::find(parent_hierarchy.children.begin(), parent_hierarchy.children.end(), child) ==
            parent_hierarchy.children.end()) {
            parent_hierarchy.children.push_back(child);
        }
        return true;
    }

    void clearParent(EntityId child) {
        setParent(child, kInvalidEntity);
    }

    bool isAlive(EntityId id) const {
        return worldRef().isAlive(id);
    }

    EntityId parent(EntityId child) const {
        const ecs::World& world = worldRef();
        const HierarchyComponent* hierarchy = world.tryGet<HierarchyComponent>(child);
        if (!hierarchy) {
            return kInvalidEntity;
        }
        return hierarchy->parent;
    }

    void updateWorldTransforms() {
        ecs::World& world = worldRef();
        const std::vector<EntityId> entities = world.view<TransformComponent>();
        if (entities.empty()) {
            return;
        }

        uint32_t max_index = 0;
        for (const EntityId entity : entities) {
            max_index = std::max(max_index, entity.index);
        }
        std::vector<uint8_t> state(static_cast<size_t>(max_index) + 1, 0);

        auto resolve = [&](auto& self, EntityId entity) -> void {
            if (!world.isAlive(entity) || !world.has<TransformComponent>(entity)) {
                return;
            }
            if (entity.index >= state.size()) {
                return;
            }

            uint8_t& visit = state[entity.index];
            if (visit == 2) {
                return;
            }
            if (visit == 1) {
                // Cycle fallback keeps transforms deterministic even with invalid user hierarchy edits.
                TransformComponent& transform = world.get<TransformComponent>(entity);
                transform.world = transform.local;
                visit = 2;
                return;
            }

            visit = 1;
            TransformComponent& transform = world.get<TransformComponent>(entity);
            EntityId parent_entity = kInvalidEntity;
            if (const HierarchyComponent* hierarchy = world.tryGet<HierarchyComponent>(entity)) {
                parent_entity = hierarchy->parent;
            }

            if (parent_entity.isValid() &&
                world.isAlive(parent_entity) &&
                world.has<TransformComponent>(parent_entity)) {
                self(self, parent_entity);
                const TransformComponent& parent_transform = world.get<TransformComponent>(parent_entity);
                transform.world = parent_transform.world * transform.local;
            } else {
                transform.world = transform.local;
            }
            visit = 2;
        };

        for (const EntityId entity : entities) {
            resolve(resolve, entity);
        }
    }

    ecs::World& world() {
        return worldRef();
    }

    const ecs::World& world() const {
        return worldRef();
    }

 private:
    HierarchyComponent& ensureHierarchy(EntityId entity) {
        ecs::World& world = worldRef();
        if (!world.has<HierarchyComponent>(entity)) {
            world.add(entity, HierarchyComponent{});
        }
        return world.get<HierarchyComponent>(entity);
    }

    void cleanupHierarchy(EntityId entity) {
        ecs::World& world = worldRef();
        HierarchyComponent* hierarchy = world.tryGet<HierarchyComponent>(entity);
        if (!hierarchy) {
            return;
        }
        if (!hierarchy->parent.isValid() && hierarchy->children.empty()) {
            world.remove<HierarchyComponent>(entity);
        }
    }

    void detachFromParent(EntityId child) {
        ecs::World& world = worldRef();
        HierarchyComponent* child_hierarchy = world.tryGet<HierarchyComponent>(child);
        if (!child_hierarchy || !child_hierarchy->parent.isValid()) {
            return;
        }

        const EntityId old_parent = child_hierarchy->parent;
        if (world.isAlive(old_parent)) {
            if (HierarchyComponent* parent_hierarchy = world.tryGet<HierarchyComponent>(old_parent)) {
                auto& children = parent_hierarchy->children;
                children.erase(std::remove(children.begin(), children.end(), child), children.end());
                cleanupHierarchy(old_parent);
            }
        }
        child_hierarchy->parent = kInvalidEntity;
        cleanupHierarchy(child);
    }

    bool wouldCreateCycle(EntityId child, EntityId new_parent) const {
        const ecs::World& world = worldRef();
        EntityId cursor = new_parent;
        while (cursor.isValid()) {
            if (cursor == child) {
                return true;
            }
            const HierarchyComponent* hierarchy = world.tryGet<HierarchyComponent>(cursor);
            if (!hierarchy) {
                break;
            }
            cursor = hierarchy->parent;
        }
        return false;
    }

    ecs::World& worldRef() {
        if (!world_) {
            throw std::runtime_error("Scene has no world bound");
        }
        return *world_;
    }

    const ecs::World& worldRef() const {
        if (!world_) {
            throw std::runtime_error("Scene has no world bound");
        }
        return *world_;
    }

    ecs::World* world_ = nullptr;
};

} // namespace karma::scene

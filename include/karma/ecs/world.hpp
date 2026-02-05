#pragma once

#include <memory>
#include <stdexcept>
#include <tuple>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "karma/ecs/component_storage.hpp"
#include "karma/ecs/entity_registry.hpp"

namespace karma::ecs {

class World {
 public:
    Entity createEntity() {
        return registry_.create();
    }

    void destroyEntity(Entity entity) {
        if (!registry_.isAlive(entity)) {
            return;
        }

        for (auto& [_, storage] : storages_) {
            storage->removeEntity(entity);
        }
        registry_.destroy(entity);
    }

    bool isAlive(Entity entity) const {
        return registry_.isAlive(entity);
    }

    const std::vector<Entity>& entities() const {
        return registry_.entities();
    }

    template <typename T>
    void add(Entity entity, T component) {
        if (!isAlive(entity)) {
            return;
        }
        getStorage<T>().data.add(entity, std::move(component));
    }

    template <typename T>
    bool has(Entity entity) const {
        if (!isAlive(entity)) {
            return false;
        }
        const Storage<T>* storage = findStorage<T>();
        if (!storage) {
            return false;
        }
        return storage->data.has(entity);
    }

    template <typename T>
    T* tryGet(Entity entity) {
        if (!isAlive(entity)) {
            return nullptr;
        }
        Storage<T>* storage = findStorage<T>();
        if (!storage) {
            return nullptr;
        }
        return storage->data.tryGet(entity);
    }

    template <typename T>
    const T* tryGet(Entity entity) const {
        if (!isAlive(entity)) {
            return nullptr;
        }
        const Storage<T>* storage = findStorage<T>();
        if (!storage) {
            return nullptr;
        }
        return storage->data.tryGet(entity);
    }

    template <typename T>
    T& get(Entity entity) {
        if (T* component = tryGet<T>(entity)) {
            return *component;
        }
        throw std::out_of_range("Component not found");
    }

    template <typename T>
    const T& get(Entity entity) const {
        if (const T* component = tryGet<T>(entity)) {
            return *component;
        }
        throw std::out_of_range("Component not found");
    }

    template <typename T>
    void remove(Entity entity) {
        if (!isAlive(entity)) {
            return;
        }
        Storage<T>* storage = findStorage<T>();
        if (!storage) {
            return;
        }
        storage->data.remove(entity);
    }

    template <typename T>
    ComponentStorage<T>& storage() {
        return getStorage<T>().data;
    }

    template <typename T>
    const ComponentStorage<T>* tryStorage() const {
        const Storage<T>* storage = findStorage<T>();
        if (!storage) {
            return nullptr;
        }
        return &storage->data;
    }

    template <typename... Ts>
    std::vector<Entity> view() const {
        std::vector<Entity> out;

        using BaseType = std::tuple_element_t<0, std::tuple<Ts...>>;
        const ComponentStorage<BaseType>* base = tryStorage<BaseType>();
        if (!base) {
            return out;
        }

        for (const Entity entity : base->denseEntities()) {
            if (!isAlive(entity)) {
                continue;
            }
            if ((has<Ts>(entity) && ...)) {
                out.push_back(entity);
            }
        }
        return out;
    }

 private:
    struct IStorage {
        virtual ~IStorage() = default;
        virtual void removeEntity(Entity entity) = 0;
    };

    template <typename T>
    struct Storage final : IStorage {
        ComponentStorage<T> data;

        void removeEntity(Entity entity) override {
            data.remove(entity);
        }
    };

    template <typename T>
    Storage<T>& getStorage() const {
        const std::type_index id = std::type_index(typeid(T));
        auto it = storages_.find(id);
        if (it == storages_.end()) {
            auto storage = std::make_unique<Storage<T>>();
            auto* storage_ptr = storage.get();
            storages_[id] = std::move(storage);
            return *storage_ptr;
        }
        return *static_cast<Storage<T>*>(it->second.get());
    }

    template <typename T>
    Storage<T>* findStorage() {
        const std::type_index id = std::type_index(typeid(T));
        auto it = storages_.find(id);
        if (it == storages_.end()) {
            return nullptr;
        }
        return static_cast<Storage<T>*>(it->second.get());
    }

    template <typename T>
    const Storage<T>* findStorage() const {
        const std::type_index id = std::type_index(typeid(T));
        auto it = storages_.find(id);
        if (it == storages_.end()) {
            return nullptr;
        }
        return static_cast<const Storage<T>*>(it->second.get());
    }

    EntityRegistry registry_;
    mutable std::unordered_map<std::type_index, std::unique_ptr<IStorage>> storages_;
};

} // namespace karma::ecs

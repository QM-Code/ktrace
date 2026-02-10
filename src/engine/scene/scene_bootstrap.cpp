#include "karma/scene/scene_bootstrap.hpp"

#include "geometry/mesh_loader.hpp"
#include "karma/common/config_store.hpp"
#include "karma/common/logging.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "karma/ecs/world.hpp"
#include "karma/renderer/device.hpp"
#include "karma/renderer/layers.hpp"
#include "karma/scene/components.hpp"

#include <spdlog/spdlog.h>
#include <cstdint>
#include <unordered_map>

namespace karma::scene {
namespace {

std::string DescribeJsonValue(const karma::json::Value* value) {
    if (!value) {
        return "<missing>";
    }
    if (value->is_string()) {
        return "'" + value->get<std::string>() + "'";
    }
    return value->dump();
}

} // namespace

bool PopulateStartupWorld(renderer::GraphicsDevice& graphics,
                          ecs::World& world,
                          StartupSceneResources& out_resources) {
    out_resources.entities.clear();
    out_resources.meshes.clear();
    out_resources.materials.clear();

    const std::filesystem::path world_path =
        config::ConfigStore::ResolveAssetPath("assets.models.world", {});
    if (world_path.empty()) {
        const auto alt_models_world = config::ConfigStore::ResolveAssetPath("models.world", {});
        const auto alt_world = config::ConfigStore::ResolveAssetPath("world", {});
        const auto* config_value = config::ConfigStore::Get("assets.models.world");
        spdlog::error(
            "EngineApp: failed to resolve startup world asset key 'assets.models.world' "
            "(config value: {}, data_root: '{}', diag models.world: '{}', diag world: '{}')",
            DescribeJsonValue(config_value),
            data::DataRoot().string(),
            alt_models_world.empty() ? "<missing>" : alt_models_world.string(),
            alt_world.empty() ? "<missing>" : alt_world.string());
        return false;
    }

    KARMA_TRACE("render.mesh", "EngineApp: loading startup world '{}'", world_path.string());
    std::vector<geometry::SceneMesh> scene_meshes;
    if (!geometry::LoadScene(world_path, scene_meshes)) {
        spdlog::error("EngineApp: failed to load startup world from {}", world_path.string());
        return false;
    }

    auto cleanup_and_fail = [&](const char* message) {
        spdlog::error("EngineApp: {}", message);
        for (const ecs::Entity entity : out_resources.entities) {
            world.destroyEntity(entity);
        }
        for (const renderer::MeshId mesh_id : out_resources.meshes) {
            if (mesh_id != renderer::kInvalidMesh) {
                graphics.destroyMesh(mesh_id);
            }
        }
        for (const renderer::MaterialId material_id : out_resources.materials) {
            if (material_id != renderer::kInvalidMaterial) {
                graphics.destroyMaterial(material_id);
            }
        }
        out_resources.entities.clear();
        out_resources.meshes.clear();
        out_resources.materials.clear();
        return false;
    };

    std::unordered_map<uint32_t, renderer::MaterialId> material_cache{};
    material_cache.reserve(scene_meshes.size());
    std::unordered_map<uint32_t, renderer::MaterialDesc> material_desc_by_index{};
    material_desc_by_index.reserve(scene_meshes.size());
    // Consolidate full MaterialDesc payloads so backend material semantics (including
    // alpha/cull and PBR factors/textures) stay stable for shared material instances.
    for (const auto& entry : scene_meshes) {
        auto [it, inserted] = material_desc_by_index.emplace(entry.material_index, entry.material);
        if (!inserted) {
            if (!it->second.albedo && entry.material.albedo) {
                it->second.albedo = entry.material.albedo;
            }
            if (!it->second.metallic_roughness && entry.material.metallic_roughness) {
                it->second.metallic_roughness = entry.material.metallic_roughness;
            }
            if (!it->second.emissive && entry.material.emissive) {
                it->second.emissive = entry.material.emissive;
            }
            if (!it->second.normal && entry.material.normal) {
                it->second.normal = entry.material.normal;
            }
            if (!it->second.occlusion && entry.material.occlusion) {
                it->second.occlusion = entry.material.occlusion;
            }
        }
    }

    for (const auto& entry : scene_meshes) {
        renderer::MaterialId material_id = renderer::kInvalidMaterial;
        const auto material_it = material_cache.find(entry.material_index);
        if (material_it != material_cache.end()) {
            material_id = material_it->second;
        } else {
            const auto desc_it = material_desc_by_index.find(entry.material_index);
            if (desc_it == material_desc_by_index.end()) {
                spdlog::error("EngineApp: missing material descriptor for startup world material index={}", entry.material_index);
                continue;
            }
            material_id = graphics.createMaterial(desc_it->second);
            if (material_id == renderer::kInvalidMaterial) {
                spdlog::error("EngineApp: failed to create material for startup world {}", world_path.string());
                continue;
            }
            material_cache.emplace(entry.material_index, material_id);
            out_resources.materials.push_back(material_id);
        }

        renderer::MeshData mesh_data = entry.mesh;
        mesh_data.albedo.reset();
        const renderer::MeshId mesh_id = graphics.createMesh(mesh_data);
        if (mesh_id == renderer::kInvalidMesh) {
            spdlog::error("EngineApp: failed to create mesh for startup world {}", world_path.string());
            continue;
        }

        const ecs::Entity entity = world.createEntity();
        TransformComponent transform{};
        transform.local = entry.transform;
        transform.world = entry.transform;
        world.add(entity, transform);

        RenderComponent render_component{};
        render_component.mesh = mesh_id;
        render_component.material = material_id;
        render_component.layer = renderer::kLayerWorld;
        world.add(entity, render_component);

        out_resources.meshes.push_back(mesh_id);
        out_resources.entities.push_back(entity);
    }

    if (out_resources.entities.empty()) {
        return cleanup_and_fail("startup world produced zero renderable entities");
    }

    KARMA_TRACE("ecs.world",
                "EngineApp: startup world entities={} materials={} world_entities={}",
                out_resources.entities.size(),
                out_resources.materials.size(),
                world.entities().size());
    return true;
}

void ReleaseStartupSceneResources(renderer::GraphicsDevice& graphics,
                                  ecs::World& world,
                                  StartupSceneResources& resources) {
    for (const ecs::Entity entity : resources.entities) {
        world.destroyEntity(entity);
    }
    for (const renderer::MeshId mesh_id : resources.meshes) {
        if (mesh_id != renderer::kInvalidMesh) {
            graphics.destroyMesh(mesh_id);
        }
    }
    for (const renderer::MaterialId material_id : resources.materials) {
        if (material_id != renderer::kInvalidMaterial) {
            graphics.destroyMaterial(material_id);
        }
    }
    resources.entities.clear();
    resources.meshes.clear();
    resources.materials.clear();
    KARMA_TRACE("ecs.world", "EngineApp: startup world released world_entities={}", world.entities().size());
}

} // namespace karma::scene

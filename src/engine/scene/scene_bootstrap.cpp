#include "karma/scene/scene_bootstrap.hpp"

#include "karma/geometry/mesh_loader.hpp"
#include "karma/common/config_store.hpp"
#include "karma/common/logging.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "karma/ecs/world.hpp"
#include "karma/renderer/device.hpp"
#include "karma/renderer/layers.hpp"
#include "karma/scene/components.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

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

bool IsFiniteVec3(const glm::vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

struct ReceiverTilePlan {
    bool enabled = false;
    float min_x = 0.0f;
    float max_x = 0.0f;
    float min_y = 0.0f;
    float max_y = 0.0f;
    float min_z = 0.0f;
    float max_z = 0.0f;
    float avg_normal_y = 1.0f;
    int tiles_x = 1;
    int tiles_z = 1;
    bool coarse_planar_patch = false;
};

ReceiverTilePlan BuildReceiverTilePlan(const renderer::MeshData& mesh) {
    ReceiverTilePlan plan{};
    if (mesh.positions.size() < 4u || mesh.indices.size() < 6u) {
        return plan;
    }

    glm::vec3 min_pos(std::numeric_limits<float>::max());
    glm::vec3 max_pos(std::numeric_limits<float>::lowest());
    for (const glm::vec3& position : mesh.positions) {
        if (!IsFiniteVec3(position)) {
            return plan;
        }
        min_pos = glm::min(min_pos, position);
        max_pos = glm::max(max_pos, position);
    }

    const float extent_x = max_pos.x - min_pos.x;
    const float extent_y = max_pos.y - min_pos.y;
    const float extent_z = max_pos.z - min_pos.z;
    if (!std::isfinite(extent_x) || !std::isfinite(extent_y) || !std::isfinite(extent_z)) {
        return plan;
    }

    // Keep this constrained to large, mostly horizontal receiver meshes to avoid over-splitting
    // regular scene geometry.
    if (extent_x < 12.0f || extent_z < 12.0f) {
        return plan;
    }
    if (extent_y > 1.0f) {
        return plan;
    }

    float avg_abs_normal_y = 1.0f;
    float avg_normal_y = 1.0f;
    if (!mesh.normals.empty()) {
        float normal_abs_y_sum = 0.0f;
        float normal_y_sum = 0.0f;
        int normal_count = 0;
        for (const glm::vec3& normal : mesh.normals) {
            if (!IsFiniteVec3(normal)) {
                continue;
            }
            normal_abs_y_sum += std::fabs(normal.y);
            normal_y_sum += normal.y;
            ++normal_count;
        }
        if (normal_count > 0) {
            avg_abs_normal_y = normal_abs_y_sum / static_cast<float>(normal_count);
            avg_normal_y = normal_y_sum / static_cast<float>(normal_count);
            if (avg_abs_normal_y < 0.85f) {
                return plan;
            }
        }
    }

    const float tile_target_size = 10.0f;
    const int tiles_x = std::clamp(static_cast<int>(std::ceil(extent_x / tile_target_size)), 2, 8);
    const int tiles_z = std::clamp(static_cast<int>(std::ceil(extent_z / tile_target_size)), 2, 8);
    if ((tiles_x * tiles_z) <= 1) {
        return plan;
    }

    plan.enabled = true;
    plan.min_x = min_pos.x;
    plan.max_x = max_pos.x;
    plan.min_y = min_pos.y;
    plan.max_y = max_pos.y;
    plan.min_z = min_pos.z;
    plan.max_z = max_pos.z;
    plan.avg_normal_y = avg_normal_y;
    plan.tiles_x = tiles_x;
    plan.tiles_z = tiles_z;
    const std::size_t triangle_count = mesh.indices.size() / 3u;
    plan.coarse_planar_patch = (extent_y <= 0.5f && avg_abs_normal_y >= 0.95f && triangle_count <= 8u);
    return plan;
}

std::vector<renderer::MeshData> BuildRetessellatedPlanarReceiverTiles(const renderer::MeshData& mesh,
                                                                      const ReceiverTilePlan& plan) {
    std::vector<renderer::MeshData> retessellated{};
    if (!plan.enabled || !plan.coarse_planar_patch) {
        return retessellated;
    }

    const int tiles_x = std::max(plan.tiles_x, 1);
    const int tiles_z = std::max(plan.tiles_z, 1);
    retessellated.reserve(static_cast<std::size_t>(tiles_x) * static_cast<std::size_t>(tiles_z));

    float min_u = 0.0f;
    float max_u = 1.0f;
    float min_v = 0.0f;
    float max_v = 1.0f;
    if (!mesh.uvs.empty()) {
        min_u = std::numeric_limits<float>::max();
        max_u = std::numeric_limits<float>::lowest();
        min_v = std::numeric_limits<float>::max();
        max_v = std::numeric_limits<float>::lowest();
        for (const glm::vec2& uv : mesh.uvs) {
            if (!std::isfinite(uv.x) || !std::isfinite(uv.y)) {
                continue;
            }
            min_u = std::min(min_u, uv.x);
            max_u = std::max(max_u, uv.x);
            min_v = std::min(min_v, uv.y);
            max_v = std::max(max_v, uv.y);
        }
        if (!std::isfinite(min_u) || !std::isfinite(max_u) || !std::isfinite(min_v) || !std::isfinite(max_v) ||
            std::fabs(max_u - min_u) < 1e-6f || std::fabs(max_v - min_v) < 1e-6f) {
            min_u = 0.0f;
            max_u = 1.0f;
            min_v = 0.0f;
            max_v = 1.0f;
        }
    }

    const float extent_x = std::max(plan.max_x - plan.min_x, 1e-3f);
    const float extent_z = std::max(plan.max_z - plan.min_z, 1e-3f);
    const float plane_y = 0.5f * (plan.min_y + plan.max_y);
    const float up_sign = (plan.avg_normal_y >= 0.0f) ? 1.0f : -1.0f;
    const glm::vec3 normal(0.0f, up_sign, 0.0f);

    auto map_u = [&](float x) {
        const float nx = std::clamp((x - plan.min_x) / extent_x, 0.0f, 1.0f);
        return min_u + nx * (max_u - min_u);
    };
    auto map_v = [&](float z) {
        const float nz = std::clamp((z - plan.min_z) / extent_z, 0.0f, 1.0f);
        return min_v + nz * (max_v - min_v);
    };

    for (int tz = 0; tz < tiles_z; ++tz) {
        for (int tx = 0; tx < tiles_x; ++tx) {
            const float fx0 = static_cast<float>(tx) / static_cast<float>(tiles_x);
            const float fx1 = static_cast<float>(tx + 1) / static_cast<float>(tiles_x);
            const float fz0 = static_cast<float>(tz) / static_cast<float>(tiles_z);
            const float fz1 = static_cast<float>(tz + 1) / static_cast<float>(tiles_z);

            const float x0 = plan.min_x + fx0 * extent_x;
            const float x1 = plan.min_x + fx1 * extent_x;
            const float z0 = plan.min_z + fz0 * extent_z;
            const float z1 = plan.min_z + fz1 * extent_z;

            renderer::MeshData tile{};
            tile.positions = {
                glm::vec3(x0, plane_y, z0),
                glm::vec3(x0, plane_y, z1),
                glm::vec3(x1, plane_y, z1),
                glm::vec3(x1, plane_y, z0),
            };
            tile.normals = {normal, normal, normal, normal};
            tile.uvs = {
                glm::vec2(map_u(x0), map_v(z0)),
                glm::vec2(map_u(x0), map_v(z1)),
                glm::vec2(map_u(x1), map_v(z1)),
                glm::vec2(map_u(x1), map_v(z0)),
            };
            if (up_sign >= 0.0f) {
                tile.indices = {0u, 1u, 2u, 0u, 2u, 3u};
            } else {
                tile.indices = {0u, 2u, 1u, 0u, 3u, 2u};
            }
            retessellated.push_back(std::move(tile));
        }
    }

    return retessellated;
}

std::vector<renderer::MeshData> BuildTiledReceiverSubmeshes(const renderer::MeshData& mesh,
                                                            const ReceiverTilePlan& plan) {
    std::vector<renderer::MeshData> tiled_meshes{};
    if (!plan.enabled || mesh.indices.size() < 3u) {
        return tiled_meshes;
    }

    tiled_meshes = BuildRetessellatedPlanarReceiverTiles(mesh, plan);
    if (!tiled_meshes.empty()) {
        return tiled_meshes;
    }

    const float extent_x = std::max(plan.max_x - plan.min_x, 1e-3f);
    const float extent_z = std::max(plan.max_z - plan.min_z, 1e-3f);
    const std::size_t tile_count =
        static_cast<std::size_t>(plan.tiles_x) * static_cast<std::size_t>(plan.tiles_z);
    std::vector<std::vector<uint32_t>> tile_indices(tile_count);

    for (std::size_t i = 0; (i + 2u) < mesh.indices.size(); i += 3u) {
        const uint32_t i0 = mesh.indices[i + 0u];
        const uint32_t i1 = mesh.indices[i + 1u];
        const uint32_t i2 = mesh.indices[i + 2u];
        if (i0 >= mesh.positions.size() || i1 >= mesh.positions.size() || i2 >= mesh.positions.size()) {
            continue;
        }

        const glm::vec3 p0 = mesh.positions[i0];
        const glm::vec3 p1 = mesh.positions[i1];
        const glm::vec3 p2 = mesh.positions[i2];
        if (!IsFiniteVec3(p0) || !IsFiniteVec3(p1) || !IsFiniteVec3(p2)) {
            continue;
        }

        const glm::vec3 centroid = (p0 + p1 + p2) / 3.0f;
        const float nx = std::clamp((centroid.x - plan.min_x) / extent_x, 0.0f, 0.9999f);
        const float nz = std::clamp((centroid.z - plan.min_z) / extent_z, 0.0f, 0.9999f);
        const int tx = std::clamp(static_cast<int>(nx * static_cast<float>(plan.tiles_x)), 0, plan.tiles_x - 1);
        const int tz = std::clamp(static_cast<int>(nz * static_cast<float>(plan.tiles_z)), 0, plan.tiles_z - 1);
        const std::size_t tile_index =
            static_cast<std::size_t>(tz) * static_cast<std::size_t>(plan.tiles_x) + static_cast<std::size_t>(tx);
        tile_indices[tile_index].push_back(i0);
        tile_indices[tile_index].push_back(i1);
        tile_indices[tile_index].push_back(i2);
    }

    tiled_meshes.reserve(tile_count);
    for (const auto& triangle_indices : tile_indices) {
        if (triangle_indices.size() < 3u) {
            continue;
        }

        renderer::MeshData tile{};
        tile.positions.reserve(triangle_indices.size());
        tile.normals.reserve(triangle_indices.size());
        tile.uvs.reserve(triangle_indices.size());
        tile.indices.reserve(triangle_indices.size());

        std::unordered_map<uint32_t, uint32_t> remap{};
        remap.reserve(triangle_indices.size());
        for (const uint32_t original_index : triangle_indices) {
            const auto [it, inserted] = remap.emplace(original_index, static_cast<uint32_t>(tile.positions.size()));
            if (inserted) {
                tile.positions.push_back(mesh.positions[original_index]);
                if (original_index < mesh.normals.size()) {
                    tile.normals.push_back(mesh.normals[original_index]);
                } else {
                    tile.normals.push_back(glm::vec3(0.0f, 1.0f, 0.0f));
                }
                if (original_index < mesh.uvs.size()) {
                    tile.uvs.push_back(mesh.uvs[original_index]);
                } else {
                    tile.uvs.push_back(glm::vec2(0.0f, 0.0f));
                }
            }
            tile.indices.push_back(it->second);
        }

        if (!tile.indices.empty()) {
            tiled_meshes.push_back(std::move(tile));
        }
    }

    return tiled_meshes;
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

        const ReceiverTilePlan tile_plan = BuildReceiverTilePlan(mesh_data);
        std::vector<renderer::MeshData> mesh_variants{};
        if (tile_plan.enabled) {
            mesh_variants = BuildTiledReceiverSubmeshes(mesh_data, tile_plan);
            KARMA_TRACE("render.mesh",
                        "EngineApp: tiled startup receiver material={} tiles={}x{} source_vertices={} source_triangles={} variants={}",
                        entry.material_index,
                        tile_plan.tiles_x,
                        tile_plan.tiles_z,
                        mesh_data.positions.size(),
                        mesh_data.indices.size() / 3u,
                        mesh_variants.size());
        }
        if (mesh_variants.empty()) {
            mesh_variants.push_back(std::move(mesh_data));
        }

        for (renderer::MeshData& variant : mesh_variants) {
            const renderer::MeshId mesh_id = graphics.createMesh(variant);
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
            render_component.casts_shadow = !tile_plan.coarse_planar_patch;
            world.add(entity, render_component);

            out_resources.meshes.push_back(mesh_id);
            out_resources.entities.push_back(entity);
        }
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

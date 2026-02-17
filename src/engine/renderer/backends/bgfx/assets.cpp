#include "internal.hpp"

#include "karma/common/logging.hpp"

#include <spdlog/spdlog.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace karma::renderer_backend {
namespace {

struct BgfxMeshVertex {
    float x;
    float y;
    float z;
    float nx;
    float ny;
    float nz;
    float u;
    float v;
};

} // namespace

renderer::MeshId CreateBgfxMeshAsset(
    const renderer::MeshData& mesh,
    const bgfx::VertexLayout& layout,
    renderer::MeshId& next_mesh_id,
    std::unordered_map<renderer::MeshId, BgfxMesh>& meshes) {
    KARMA_TRACE("render.bgfx", "createMesh vertices={} indices={}",
                mesh.positions.size(), mesh.indices.size());
    std::vector<BgfxMeshVertex> vertices;
    vertices.reserve(mesh.positions.size());
    for (size_t i = 0; i < mesh.positions.size(); ++i) {
        const glm::vec3& p = mesh.positions[i];
        const glm::vec3 n = i < mesh.normals.size() ? mesh.normals[i] : glm::vec3(0.0f, 1.0f, 0.0f);
        const glm::vec2 uv = i < mesh.uvs.size() ? mesh.uvs[i] : glm::vec2(0.0f, 0.0f);
        vertices.push_back({p.x, p.y, p.z, n.x, n.y, n.z, uv.x, uv.y});
    }
    const bgfx::Memory* vmem = bgfx::copy(vertices.data(), sizeof(BgfxMeshVertex) * vertices.size());
    const bgfx::Memory* imem = bgfx::copy(mesh.indices.data(), sizeof(uint32_t) * mesh.indices.size());

    BgfxMesh out;
    out.vbh = bgfx::createVertexBuffer(vmem, layout);
    out.ibh = bgfx::createIndexBuffer(imem, BGFX_BUFFER_INDEX32);
    out.num_indices = static_cast<uint32_t>(mesh.indices.size());
    out.shadow_positions = mesh.positions;
    out.shadow_indices = mesh.indices;
    if (!out.shadow_positions.empty()) {
        glm::vec3 min_pos = out.shadow_positions.front();
        glm::vec3 max_pos = out.shadow_positions.front();
        for (const glm::vec3& p : out.shadow_positions) {
            min_pos = glm::min(min_pos, p);
            max_pos = glm::max(max_pos, p);
        }
        out.shadow_center = 0.5f * (min_pos + max_pos);
        float radius_sq = 0.0f;
        for (const glm::vec3& p : out.shadow_positions) {
            const glm::vec3 delta = p - out.shadow_center;
            radius_sq = std::max(radius_sq, glm::dot(delta, delta));
        }
        out.shadow_radius = std::sqrt(std::max(radius_sq, 0.0f));
    }
    if (mesh.albedo && !mesh.albedo->pixels.empty()) {
        out.tex = createTextureFromData(*mesh.albedo);
        KARMA_TRACE("render.bgfx", "createMesh texture={} {}x{}",
                    bgfx::isValid(out.tex) ? 1 : 0,
                    mesh.albedo->width, mesh.albedo->height);
    }

    renderer::MeshId id = next_mesh_id++;
    meshes[id] = out;
    return id;
}

void DestroyBgfxMeshAsset(
    renderer::MeshId mesh,
    std::unordered_map<renderer::MeshId, BgfxMesh>& meshes) {
    auto it = meshes.find(mesh);
    if (it == meshes.end()) return;
    if (bgfx::isValid(it->second.vbh)) bgfx::destroy(it->second.vbh);
    if (bgfx::isValid(it->second.ibh)) bgfx::destroy(it->second.ibh);
    if (bgfx::isValid(it->second.tex)) bgfx::destroy(it->second.tex);
    meshes.erase(it);
}

renderer::MaterialId CreateBgfxMaterialAsset(
    const renderer::MaterialDesc& material,
    bool supports_direct_multi_sampler_inputs,
    renderer::MaterialId& next_material_id,
    std::unordered_map<renderer::MaterialId, BgfxMaterial>& materials) {
    const detail::MaterialTextureSetLifecycleIngestion texture_ingestion =
        detail::IngestMaterialTextureSetForLifecycle(material);
    const detail::MaterialShaderInputContract shader_input_contract =
        detail::ResolveMaterialShaderInputContract(
            material, texture_ingestion, supports_direct_multi_sampler_inputs);
    const detail::ResolvedMaterialSemantics semantics =
        detail::ResolveMaterialSemantics(material);
    if (!detail::ValidateResolvedMaterialSemantics(semantics)) {
        spdlog::error("Graphics(Bgfx): material semantics validation failed");
        return renderer::kInvalidMaterial;
    }
    BgfxMaterial out;
    out.semantics = semantics;
    out.shader_input_path = shader_input_contract.path;
    out.shader_uses_normal_input = shader_input_contract.used_normal_lifecycle_texture;
    out.shader_uses_occlusion_input = shader_input_contract.used_occlusion_lifecycle_texture;
    if (shader_input_contract.path == detail::MaterialShaderTextureInputPath::DirectMultiSampler &&
        shader_input_contract.direct_albedo_texture) {
        out.tex = createTextureFromData(*shader_input_contract.direct_albedo_texture);
    } else if (shader_input_contract.path == detail::MaterialShaderTextureInputPath::CompositeFallback &&
               shader_input_contract.fallback_composite_texture) {
        out.tex = createTextureFromData(*shader_input_contract.fallback_composite_texture);
    } else if (material.albedo && !material.albedo->pixels.empty()) {
        out.tex = createTextureFromData(*material.albedo);
        KARMA_TRACE("render.bgfx", "createMaterial texture={} {}x{}",
                    bgfx::isValid(out.tex) ? 1 : 0,
                    material.albedo->width,
                    material.albedo->height);
    }
    if (shader_input_contract.path == detail::MaterialShaderTextureInputPath::DirectMultiSampler &&
        shader_input_contract.direct_normal_texture) {
        out.normal_tex = createTextureFromData(*shader_input_contract.direct_normal_texture);
    } else if (texture_ingestion.normal.texture) {
        out.normal_tex = createTextureFromData(*texture_ingestion.normal.texture);
    }
    if (shader_input_contract.path == detail::MaterialShaderTextureInputPath::DirectMultiSampler &&
        shader_input_contract.direct_occlusion_texture) {
        out.occlusion_tex = createTextureFromData(*shader_input_contract.direct_occlusion_texture);
    } else if (texture_ingestion.occlusion.texture) {
        out.occlusion_tex = createTextureFromData(*texture_ingestion.occlusion.texture);
    }
    KARMA_TRACE("render.bgfx",
                "createMaterial semantics metallic={:.3f} roughness={:.3f} normalVar={:.3f} occlusion={:.3f} occlusionEdge={:.3f} emissive=({:.3f},{:.3f},{:.3f}) alphaMode={} alpha={} cutoff={:.3f} draw={} blend={} doubleSided={} mrTex={} emissiveTex={} normalTex={} occlusionTex={} normalLifecycleTex={} occlusionLifecycleTex={} normalBounded={} occlusionBounded={} shaderPath={} shaderDirect={} shaderFallbackComposite={} shaderConsumesNormal={} shaderConsumesOcclusion={} shaderUsesAlbedo={} shaderTextureBounded={}",
                out.semantics.metallic, out.semantics.roughness, out.semantics.normal_variation, out.semantics.occlusion,
                out.semantics.occlusion_edge,
                out.semantics.emissive.r, out.semantics.emissive.g, out.semantics.emissive.b,
                static_cast<int>(out.semantics.alpha_mode), out.semantics.base_color.a, out.semantics.alpha_cutoff,
                out.semantics.draw ? 1 : 0, out.semantics.alpha_blend ? 1 : 0, out.semantics.double_sided ? 1 : 0,
                out.semantics.used_metallic_roughness_texture ? 1 : 0,
                out.semantics.used_emissive_texture ? 1 : 0,
                out.semantics.used_normal_texture ? 1 : 0,
                out.semantics.used_occlusion_texture ? 1 : 0,
                bgfx::isValid(out.normal_tex) ? 1 : 0,
                bgfx::isValid(out.occlusion_tex) ? 1 : 0,
                texture_ingestion.normal.bounded ? 1 : 0,
                texture_ingestion.occlusion.bounded ? 1 : 0,
                static_cast<int>(shader_input_contract.path),
                shader_input_contract.path == detail::MaterialShaderTextureInputPath::DirectMultiSampler ? 1 : 0,
                (shader_input_contract.path == detail::MaterialShaderTextureInputPath::CompositeFallback &&
                 shader_input_contract.fallback_composite_texture && bgfx::isValid(out.tex))
                    ? 1
                    : 0,
                shader_input_contract.used_normal_lifecycle_texture ? 1 : 0,
                shader_input_contract.used_occlusion_lifecycle_texture ? 1 : 0,
                shader_input_contract.used_albedo_texture ? 1 : 0,
                shader_input_contract.bounded ? 1 : 0);
    renderer::MaterialId id = next_material_id++;
    materials[id] = out;
    return id;
}

void DestroyBgfxMaterialAsset(
    renderer::MaterialId material,
    std::unordered_map<renderer::MaterialId, BgfxMaterial>& materials) {
    auto it = materials.find(material);
    if (it == materials.end()) return;
    if (bgfx::isValid(it->second.tex)) bgfx::destroy(it->second.tex);
    if (bgfx::isValid(it->second.normal_tex)) bgfx::destroy(it->second.normal_tex);
    if (bgfx::isValid(it->second.occlusion_tex)) bgfx::destroy(it->second.occlusion_tex);
    materials.erase(it);
}

} // namespace karma::renderer_backend

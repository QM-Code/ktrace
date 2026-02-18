#include "internal.hpp"

#include "karma/common/logging/logging.hpp"

#include <spdlog/spdlog.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace karma::renderer_backend {

renderer::MeshId CreateDiligentMeshAsset(
    Diligent::IRenderDevice* device,
    const Diligent::RefCntAutoPtr<Diligent::ITextureView>& white_srv,
    const renderer::MeshData& mesh,
    renderer::MeshId& next_mesh_id,
    std::unordered_map<renderer::MeshId, DiligentMesh>& meshes) {
    if (!device) {
        return renderer::kInvalidMesh;
    }
    DiligentMesh out;

    Diligent::BufferDesc vb_desc{};
    vb_desc.Usage = Diligent::USAGE_IMMUTABLE;
    vb_desc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
    vb_desc.Name = "mesh_vb";

    std::vector<float> interleaved;
    interleaved.reserve(mesh.positions.size() * 8);
    for (size_t i = 0; i < mesh.positions.size(); ++i) {
        const auto& p = mesh.positions[i];
        const auto n = i < mesh.normals.size() ? mesh.normals[i] : glm::vec3(0.0f, 1.0f, 0.0f);
        const auto uv = i < mesh.uvs.size() ? mesh.uvs[i] : glm::vec2(0.0f, 0.0f);
        interleaved.push_back(p.x);
        interleaved.push_back(p.y);
        interleaved.push_back(p.z);
        interleaved.push_back(n.x);
        interleaved.push_back(n.y);
        interleaved.push_back(n.z);
        interleaved.push_back(uv.x);
        interleaved.push_back(uv.y);
    }
    vb_desc.Size = static_cast<uint32_t>(interleaved.size() * sizeof(float));

    Diligent::BufferData vb_data{};
    vb_data.pData = interleaved.data();
    vb_data.DataSize = static_cast<uint32_t>(interleaved.size() * sizeof(float));
    device->CreateBuffer(vb_desc, &vb_data, &out.vb);

    Diligent::BufferDesc ib_desc{};
    ib_desc.Usage = Diligent::USAGE_IMMUTABLE;
    ib_desc.BindFlags = Diligent::BIND_INDEX_BUFFER;
    ib_desc.Size = static_cast<uint32_t>(mesh.indices.size() * sizeof(uint32_t));
    ib_desc.Name = "mesh_ib";

    Diligent::BufferData ib_data{};
    ib_data.pData = mesh.indices.data();
    ib_data.DataSize = static_cast<uint32_t>(mesh.indices.size() * sizeof(uint32_t));
    device->CreateBuffer(ib_desc, &ib_data, &out.ib);

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
        out.srv = CreateDiligentTextureView(device, *mesh.albedo);
        KARMA_TRACE("render.diligent", "createMesh texture={} {}x{}",
                    out.srv ? 1 : 0, mesh.albedo->width, mesh.albedo->height);
    }
    if (!out.srv) {
        out.srv = white_srv;
    }
    KARMA_TRACE("render.diligent", "createMesh vertices={} indices={}",
                mesh.positions.size(), mesh.indices.size());

    renderer::MeshId id = next_mesh_id++;
    meshes[id] = out;
    return id;
}

void DestroyDiligentMeshAsset(
    renderer::MeshId mesh,
    std::unordered_map<renderer::MeshId, DiligentMesh>& meshes) {
    meshes.erase(mesh);
}

renderer::MaterialId CreateDiligentMaterialAsset(
    Diligent::IRenderDevice* device,
    const renderer::MaterialDesc& material,
    bool supports_direct_multi_sampler_inputs,
    renderer::MaterialId& next_material_id,
    std::unordered_map<renderer::MaterialId, DiligentMaterial>& materials) {
    if (!device) {
        return renderer::kInvalidMaterial;
    }
    const detail::MaterialTextureSetLifecycleIngestion texture_ingestion =
        detail::IngestMaterialTextureSetForLifecycle(material);
    const detail::MaterialShaderInputContract shader_input_contract =
        detail::ResolveMaterialShaderInputContract(
            material, texture_ingestion, supports_direct_multi_sampler_inputs);
    const detail::ResolvedMaterialSemantics semantics = detail::ResolveMaterialSemantics(material);
    if (!detail::ValidateResolvedMaterialSemantics(semantics)) {
        spdlog::error("Diligent: material semantics validation failed");
        return renderer::kInvalidMaterial;
    }
    DiligentMaterial out;
    out.semantics = semantics;
    out.shader_input_path = shader_input_contract.path;
    out.shader_uses_normal_input = shader_input_contract.used_normal_lifecycle_texture;
    out.shader_uses_occlusion_input = shader_input_contract.used_occlusion_lifecycle_texture;
    if (shader_input_contract.path == detail::MaterialShaderTextureInputPath::DirectMultiSampler &&
        shader_input_contract.direct_albedo_texture) {
        out.srv = CreateDiligentTextureView(device, *shader_input_contract.direct_albedo_texture);
    } else if (shader_input_contract.path == detail::MaterialShaderTextureInputPath::CompositeFallback &&
               shader_input_contract.fallback_composite_texture) {
        out.srv = CreateDiligentTextureView(device, *shader_input_contract.fallback_composite_texture);
    } else if (material.albedo && !material.albedo->pixels.empty()) {
        out.srv = CreateDiligentTextureView(device, *material.albedo);
        KARMA_TRACE("render.diligent", "createMaterial texture={} {}x{}",
                    out.srv ? 1 : 0, material.albedo->width, material.albedo->height);
    }
    if (shader_input_contract.path == detail::MaterialShaderTextureInputPath::DirectMultiSampler &&
        shader_input_contract.direct_normal_texture) {
        out.normal_srv = CreateDiligentTextureView(device, *shader_input_contract.direct_normal_texture);
    } else if (texture_ingestion.normal.texture) {
        out.normal_srv = CreateDiligentTextureView(device, *texture_ingestion.normal.texture);
    }
    if (shader_input_contract.path == detail::MaterialShaderTextureInputPath::DirectMultiSampler &&
        shader_input_contract.direct_occlusion_texture) {
        out.occlusion_srv = CreateDiligentTextureView(device, *shader_input_contract.direct_occlusion_texture);
    } else if (texture_ingestion.occlusion.texture) {
        out.occlusion_srv = CreateDiligentTextureView(device, *texture_ingestion.occlusion.texture);
    }
    KARMA_TRACE("render.diligent",
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
                out.normal_srv ? 1 : 0,
                out.occlusion_srv ? 1 : 0,
                texture_ingestion.normal.bounded ? 1 : 0,
                texture_ingestion.occlusion.bounded ? 1 : 0,
                static_cast<int>(shader_input_contract.path),
                shader_input_contract.path == detail::MaterialShaderTextureInputPath::DirectMultiSampler ? 1 : 0,
                (shader_input_contract.path == detail::MaterialShaderTextureInputPath::CompositeFallback &&
                 shader_input_contract.fallback_composite_texture && out.srv)
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

void DestroyDiligentMaterialAsset(
    renderer::MaterialId material,
    std::unordered_map<renderer::MaterialId, DiligentMaterial>& materials) {
    materials.erase(material);
}

} // namespace karma::renderer_backend

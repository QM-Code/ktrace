#pragma once

#include "../material_semantics_internal.hpp"

#include "karma/renderer/backend.hpp"

#include <bgfx/bgfx.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace karma::renderer_backend {

class BgfxBackend;

struct BgfxMesh {
    bgfx::VertexBufferHandle vbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle ibh = BGFX_INVALID_HANDLE;
    uint32_t num_indices = 0;
    bgfx::TextureHandle tex = BGFX_INVALID_HANDLE;
    std::vector<glm::vec3> shadow_positions{};
    std::vector<uint32_t> shadow_indices{};
    glm::vec3 shadow_center{0.0f, 0.0f, 0.0f};
    float shadow_radius = 0.0f;
};

struct BgfxMaterial {
    detail::ResolvedMaterialSemantics semantics{};
    bgfx::TextureHandle tex = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle normal_tex = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle occlusion_tex = BGFX_INVALID_HANDLE;
    detail::MaterialShaderTextureInputPath shader_input_path =
        detail::MaterialShaderTextureInputPath::Disabled;
    bool shader_uses_normal_input = false;
    bool shader_uses_occlusion_input = false;
};

struct BgfxDirectSamplerShaderAlignment {
    bool source_exists = false;
    bool source_declares_direct_contract = false;
    bool binary_exists = false;
    bool binary_non_empty = false;
    bool binary_up_to_date = false;
    bool source_absent_integrity_ready = false;
    bool source_present_mode = false;
    bool source_absent_compat_mode = false;
    bool aligned = false;
    std::string source_path{};
    std::string binary_path{};
    std::string integrity_manifest_path{};
    std::string source_absent_integrity_reason{"source_missing_and_integrity_contract_unavailable"};
    std::string reason{"uninitialized"};
};

struct BgfxProgramHandles {
    bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle shadow_depth_program = BGFX_INVALID_HANDLE;
};

struct BgfxUniformSamplerHandles {
    bgfx::UniformHandle u_color = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_light_dir = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_light_color = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_ambient_color = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_unlit = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_texture_mode = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_params0 = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_params1 = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_params2 = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_bias_params = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_axis_right = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_axis_up = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_axis_forward = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_cascade_splits = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_cascade_world_texel = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_cascade_params = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_camera_position = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_camera_forward = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_cascade_uv_proj = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_local_light_count = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_local_light_params = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_local_light_pos_range = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_local_light_color_intensity = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_local_light_shadow_slot = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_point_shadow_params = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_point_shadow_atlas_texel = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_point_shadow_tuning = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_point_shadow_uv_proj = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_tex = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_normal = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_occlusion = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_shadow = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_point_shadow = BGFX_INVALID_HANDLE;
};

bgfx::TextureHandle createWhiteTexture();
bgfx::TextureHandle createTextureFromData(const renderer::MeshData::TextureData& tex);

BgfxProgramHandles CreateBgfxProgramHandles();

BgfxUniformSamplerHandles CreateBgfxUniformSamplerHandles(
    uint16_t directional_shadow_cascade_count,
    uint16_t max_local_lights,
    uint16_t max_point_shadow_matrices);

renderer::MeshId CreateBgfxMeshAsset(
    const renderer::MeshData& mesh,
    const bgfx::VertexLayout& layout,
    renderer::MeshId& next_mesh_id,
    std::unordered_map<renderer::MeshId, BgfxMesh>& meshes);

void DestroyBgfxMeshAsset(
    renderer::MeshId mesh,
    std::unordered_map<renderer::MeshId, BgfxMesh>& meshes);

renderer::MaterialId CreateBgfxMaterialAsset(
    const renderer::MaterialDesc& material,
    bool supports_direct_multi_sampler_inputs,
    renderer::MaterialId& next_material_id,
    std::unordered_map<renderer::MaterialId, BgfxMaterial>& materials);

void DestroyBgfxMaterialAsset(
    renderer::MaterialId material,
    std::unordered_map<renderer::MaterialId, BgfxMaterial>& materials);

BgfxDirectSamplerShaderAlignment EvaluateBgfxDirectSamplerShaderAlignment();

} // namespace karma::renderer_backend

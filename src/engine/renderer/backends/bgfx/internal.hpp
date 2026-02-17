#pragma once

#include "../internal/debug_line.hpp"
#include "../internal/directional_shadow.hpp"
#include "../internal/environment_lighting.hpp"
#include "../internal/material_semantics.hpp"

#include "karma/renderer/backend.hpp"

#include <bgfx/bgfx.h>

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace karma::renderer_backend {

class BgfxBackend;

struct PosNormalVertex {
    float x;
    float y;
    float z;
    float nx;
    float ny;
    float nz;
    float u;
    float v;

    static bgfx::VertexLayout layout() {
        bgfx::VertexLayout layout;
        layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
        return layout;
    }
};

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

constexpr std::size_t kBgfxDirectionalShadowCascadeCount =
    static_cast<std::size_t>(detail::kDirectionalShadowCascadeCount);
constexpr std::size_t kBgfxMaxLocalLights = 4u;
constexpr std::size_t kBgfxMaxPointShadowLights = kBgfxMaxLocalLights;
constexpr std::size_t kBgfxPointShadowFaceCount = static_cast<std::size_t>(detail::kPointShadowFaceCount);
constexpr std::size_t kBgfxMaxPointShadowMatrices =
    kBgfxMaxPointShadowLights * kBgfxPointShadowFaceCount;

struct BgfxRenderableDraw {
    const renderer::DrawItem* item = nullptr;
    BgfxMesh* mesh = nullptr;
    BgfxMaterial* material = nullptr;
};

struct BgfxShadowState {
    bgfx::ProgramHandle shadow_depth_program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_params0 = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_params1 = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_params2 = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_bias_params = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_axis_right = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_axis_up = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_shadow_axis_forward = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle* shadow_tex = nullptr;
    bgfx::TextureHandle* point_shadow_tex = nullptr;
    bgfx::FrameBufferHandle* shadow_fb = nullptr;
    uint16_t* shadow_tex_size = nullptr;
    uint16_t* point_shadow_tex_width = nullptr;
    uint16_t* point_shadow_tex_height = nullptr;
    bool* shadow_tex_is_rt = nullptr;
    bool* shadow_tex_rt_uses_depth = nullptr;
    bool shadow_depth_attachment_supported = false;
    bool* shadow_tex_ready = nullptr;
    bool* point_shadow_tex_ready = nullptr;
    detail::DirectionalShadowMap* cached_shadow_map = nullptr;
    detail::DirectionalShadowCascadeSet* cached_shadow_cascades = nullptr;
    detail::PointShadowMap* cached_point_shadow_map = nullptr;
    int* shadow_update_every_frames = nullptr;
    int* shadow_frames_until_update = nullptr;
    int* point_shadow_frames_until_update = nullptr;
    std::array<int, kBgfxMaxPointShadowLights>* point_shadow_slot_source_index = nullptr;
    std::array<glm::vec3, kBgfxMaxPointShadowLights>* point_shadow_slot_position = nullptr;
    std::array<float, kBgfxMaxPointShadowLights>* point_shadow_slot_range = nullptr;
    std::array<uint8_t, kBgfxMaxPointShadowMatrices>* point_shadow_face_dirty = nullptr;
    uint32_t* point_shadow_face_cursor = nullptr;
    bool* point_shadow_cache_initialized = nullptr;
    float point_shadow_position_threshold = 0.0f;
    float point_shadow_range_threshold = 0.0f;
    std::vector<renderer::DrawItem>* previous_shadow_items = nullptr;
    bool* shadow_cache_inputs_valid = nullptr;
    glm::vec3* cached_shadow_camera_position = nullptr;
    glm::vec3* cached_shadow_camera_forward = nullptr;
    glm::vec3* cached_shadow_light_direction = nullptr;
    float* cached_shadow_camera_aspect = nullptr;
    float* cached_shadow_camera_fov_y_degrees = nullptr;
    float* cached_shadow_camera_near = nullptr;
    float* cached_shadow_camera_far = nullptr;
    float directional_shadow_position_threshold = 0.0f;
    float directional_shadow_angle_threshold_deg = 0.0f;
};

struct BgfxDirectionalShadowUpdateResult {
    glm::vec3 current_camera_forward{0.0f, 0.0f, -1.0f};
    bool point_shadow_structural_change = false;
    std::vector<glm::vec4> moved_point_shadow_caster_bounds{};
};

struct BgfxPointShadowUpdateResult {
    int point_shadow_selected = 0;
    int point_shadow_dirty_faces = 0;
    int point_shadow_updated_faces = 0;
    int point_shadow_budget = 0;
    bool point_shadow_full_refresh = false;
};

uint32_t PackBgfxColorRgba8(const glm::vec4& color);

struct BgfxRenderSubmissionInput {
    renderer::LayerId layer = 0;
    const renderer::DirectionalLightData* light = nullptr;
    const detail::ResolvedEnvironmentLightingSemantics* environment_semantics = nullptr;
    const std::vector<BgfxRenderableDraw>* renderables = nullptr;
    const std::vector<detail::ResolvedDebugLineSemantics>* debug_lines = nullptr;
    const detail::DirectionalShadowMap* shadow_map = nullptr;
    bool supports_direct_multi_sampler_inputs = false;
    const std::string* direct_sampler_disable_reason = nullptr;

    bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
    const bgfx::VertexLayout* layout = nullptr;
    BgfxUniformSamplerHandles uniforms{};
    bgfx::TextureHandle white_tex = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle shadow_tex = BGFX_INVALID_HANDLE;
    bool shadow_tex_ready = false;
    bgfx::TextureHandle point_shadow_tex = BGFX_INVALID_HANDLE;
    bool point_shadow_tex_ready = false;

    const float* shadow_params0 = nullptr;
    const float* shadow_params1 = nullptr;
    const float* shadow_params2 = nullptr;
    const float* shadow_bias_params = nullptr;
    const float* shadow_axis_right = nullptr;
    const float* shadow_axis_up = nullptr;
    const float* shadow_axis_forward = nullptr;
    const float* shadow_cascade_splits_uniform = nullptr;
    const float* shadow_cascade_world_texel_uniform = nullptr;
    const float* shadow_cascade_params_uniform = nullptr;
    const float* shadow_camera_position = nullptr;
    const float* shadow_camera_forward = nullptr;
    const std::array<glm::mat4, kBgfxDirectionalShadowCascadeCount>* shadow_cascade_uv_proj = nullptr;
    const float* local_light_count_uniform = nullptr;
    const float* local_light_params_uniform = nullptr;
    const std::array<float, kBgfxMaxLocalLights * 4u>* local_light_pos_range = nullptr;
    const std::array<float, kBgfxMaxLocalLights * 4u>* local_light_color_intensity = nullptr;
    const std::array<float, kBgfxMaxLocalLights * 4u>* local_light_shadow_slot = nullptr;
    const float* point_shadow_params_uniform = nullptr;
    const float* point_shadow_atlas_texel_uniform = nullptr;
    const float* point_shadow_tuning_uniform = nullptr;
    const std::array<glm::mat4, kBgfxMaxPointShadowMatrices>* point_shadow_uv_proj = nullptr;
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

BgfxDirectionalShadowUpdateResult UpdateBgfxDirectionalShadowCache(
    BgfxShadowState& state,
    renderer::LayerId layer,
    const renderer::CameraData& camera,
    const renderer::DirectionalLightData& light,
    bool world_layer,
    bool gpu_shadow_requested,
    const detail::ResolvedDirectionalShadowSemantics& shadow_semantics,
    float aspect,
    const detail::DirectionalShadowView& shadow_view,
    const std::vector<BgfxRenderableDraw>& renderables,
    const std::vector<detail::DirectionalShadowCaster>& shadow_casters,
    const std::vector<renderer::DrawItem>& current_shadow_items,
    const std::unordered_map<renderer::MeshId, BgfxMesh>& meshes,
    const std::unordered_map<renderer::MaterialId, BgfxMaterial>& materials);

BgfxPointShadowUpdateResult UpdateBgfxPointShadowCache(
    BgfxShadowState& state,
    const detail::ResolvedDirectionalShadowSemantics& shadow_semantics,
    bool world_layer,
    const std::vector<renderer::LightData>& lights,
    const std::vector<detail::DirectionalShadowCaster>& shadow_casters,
    const std::vector<int>& point_shadow_selected_indices,
    bool point_shadow_structural_change,
    const std::vector<glm::vec4>& moved_point_shadow_caster_bounds);

BgfxDirectSamplerShaderAlignment EvaluateBgfxDirectSamplerShaderAlignment();

void SubmitBgfxRenderLayerDraws(const BgfxRenderSubmissionInput& input);

} // namespace karma::renderer_backend

#pragma once

#include "../internal/direct_sampler_observability.hpp"
#include "../internal/debug_line.hpp"
#include "../internal/directional_shadow.hpp"
#include "../internal/environment_lighting.hpp"
#include "../internal/material_semantics.hpp"

#include "karma/renderer/backend.hpp"

#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Shader.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace karma::renderer_backend {

class DiligentBackend;

struct DiligentMesh {
    Diligent::RefCntAutoPtr<Diligent::IBuffer> vb;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> ib;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> srv;
    uint32_t num_indices = 0;
    std::vector<glm::vec3> shadow_positions{};
    std::vector<uint32_t> shadow_indices{};
    glm::vec3 shadow_center{0.0f, 0.0f, 0.0f};
    float shadow_radius = 0.0f;
};

struct DiligentMaterial {
    detail::ResolvedMaterialSemantics semantics{};
    Diligent::RefCntAutoPtr<Diligent::ITextureView> srv;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> normal_srv;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> occlusion_srv;
    detail::MaterialShaderTextureInputPath shader_input_path =
        detail::MaterialShaderTextureInputPath::Disabled;
    bool shader_uses_normal_input = false;
    bool shader_uses_occlusion_input = false;
};

struct DiligentDirectSamplerContractState {
    detail::DiligentDirectSamplerContractReport contract_report{};
    std::string disable_reason = "not_initialized";
    bool supports_direct_multi_sampler_inputs = false;
};

constexpr std::size_t kDiligentDirectionalShadowCascadeCount =
    static_cast<std::size_t>(detail::kDirectionalShadowCascadeCount);
constexpr std::size_t kDiligentMaxLocalLights = 4u;
constexpr std::size_t kDiligentMaxPointShadowLights = kDiligentMaxLocalLights;
constexpr std::size_t kDiligentPointShadowFaceCount =
    static_cast<std::size_t>(detail::kPointShadowFaceCount);
constexpr std::size_t kDiligentPointShadowMatrixCount =
    kDiligentMaxPointShadowLights * kDiligentPointShadowFaceCount;

struct DiligentRenderableDraw {
    const renderer::DrawItem* item = nullptr;
    DiligentMesh* mesh = nullptr;
    DiligentMaterial* material = nullptr;
};

struct DiligentRenderConstants {
    glm::mat4 u_modelViewProj;
    glm::mat4 u_model;
    glm::vec4 u_color;
    glm::vec4 u_lightDir;
    glm::vec4 u_lightColor;
    glm::vec4 u_ambientColor;
    glm::vec4 u_unlit;
    glm::vec4 u_textureMode;
    glm::vec4 u_shadowParams0;
    glm::vec4 u_shadowParams1;
    glm::vec4 u_shadowParams2;
    glm::vec4 u_shadowBiasParams;
    glm::vec4 u_shadowAxisRight;
    glm::vec4 u_shadowAxisUp;
    glm::vec4 u_shadowAxisForward;
    glm::vec4 u_shadowCascadeSplits;
    glm::vec4 u_shadowCascadeWorldTexel;
    glm::vec4 u_shadowCascadeParams;
    glm::vec4 u_shadowCameraPosition;
    glm::vec4 u_shadowCameraForward;
    std::array<glm::mat4, kDiligentDirectionalShadowCascadeCount> u_shadowCascadeUvProj;
    glm::vec4 u_localLightCount;
    glm::vec4 u_localLightParams;
    std::array<glm::vec4, kDiligentMaxLocalLights> u_localLightPosRange;
    std::array<glm::vec4, kDiligentMaxLocalLights> u_localLightColorIntensity;
    std::array<glm::vec4, kDiligentMaxLocalLights> u_localLightShadowSlot;
    glm::vec4 u_pointShadowParams;
    glm::vec4 u_pointShadowAtlasTexel;
    glm::vec4 u_pointShadowTuning;
    std::array<glm::mat4, kDiligentPointShadowMatrixCount> u_pointShadowUvProj;
};

struct DiligentLineVertex {
    float x;
    float y;
    float z;
    float nx;
    float ny;
    float nz;
    float u;
    float v;
};

struct DiligentPipelineVariant {
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> pso;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> srb;
};

struct DiligentLinePipeline {
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> pso;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> srb;
};

struct DiligentShadowState {
    Diligent::IRenderDevice* device = nullptr;
    Diligent::IDeviceContext* context = nullptr;
    Diligent::IPipelineState* shadow_depth_pso = nullptr;
    Diligent::IShaderResourceBinding* shadow_depth_srb = nullptr;
    Diligent::IBuffer* constant_buffer = nullptr;
    Diligent::RefCntAutoPtr<Diligent::ITexture>* shadow_tex = nullptr;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>* shadow_srv = nullptr;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>* shadow_rtv = nullptr;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>* shadow_dsv = nullptr;
    Diligent::RefCntAutoPtr<Diligent::ITexture>* point_shadow_tex = nullptr;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>* point_shadow_srv = nullptr;
    uint32_t* shadow_tex_size = nullptr;
    uint32_t* point_shadow_tex_width = nullptr;
    uint32_t* point_shadow_tex_height = nullptr;
    bool* shadow_tex_is_rt = nullptr;
    bool* shadow_tex_rt_uses_depth = nullptr;
    bool* shadow_tex_ready = nullptr;
    bool* point_shadow_tex_ready = nullptr;
    detail::DirectionalShadowMap* cached_shadow_map = nullptr;
    detail::DirectionalShadowCascadeSet* cached_shadow_cascades = nullptr;
    detail::PointShadowMap* cached_point_shadow_map = nullptr;
    int* shadow_update_every_frames = nullptr;
    int* shadow_frames_until_update = nullptr;
    int* point_shadow_frames_until_update = nullptr;
    std::array<int, kDiligentMaxPointShadowLights>* point_shadow_slot_source_index = nullptr;
    std::array<glm::vec3, kDiligentMaxPointShadowLights>* point_shadow_slot_position = nullptr;
    std::array<float, kDiligentMaxPointShadowLights>* point_shadow_slot_range = nullptr;
    std::array<uint8_t, kDiligentPointShadowMatrixCount>* point_shadow_face_dirty = nullptr;
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

struct DiligentDirectionalShadowUpdateResult {
    glm::vec3 current_camera_forward{0.0f, 0.0f, -1.0f};
    bool point_shadow_structural_change = false;
    std::vector<glm::vec4> moved_point_shadow_caster_bounds{};
};

struct DiligentPointShadowUpdateResult {
    int point_shadow_selected = 0;
    int point_shadow_dirty_faces = 0;
    int point_shadow_updated_faces = 0;
    int point_shadow_budget = 0;
    bool point_shadow_full_refresh = false;
};

struct DiligentRenderSubmissionInput {
    renderer::LayerId layer = 0;
    const renderer::CameraData* camera = nullptr;
    int width = 1;
    int height = 1;
    const renderer::DirectionalLightData* light = nullptr;
    const detail::ResolvedEnvironmentLightingSemantics* environment_semantics = nullptr;
    const std::vector<DiligentRenderableDraw>* renderables = nullptr;
    const std::vector<detail::ResolvedDebugLineSemantics>* debug_lines = nullptr;
    const detail::DirectionalShadowMap* shadow_map = nullptr;
    bool supports_direct_multi_sampler_inputs = false;
    const std::string* direct_sampler_disable_reason = nullptr;

    Diligent::IDeviceContext* context = nullptr;
    const std::array<DiligentPipelineVariant, detail::kDiligentMaterialVariantCount>* pipeline_variants =
        nullptr;
    const DiligentLinePipeline* line_pipeline = nullptr;
    Diligent::IBuffer* constant_buffer = nullptr;
    Diligent::IBuffer* line_vertex_buffer = nullptr;
    const Diligent::RefCntAutoPtr<Diligent::ITextureView>* white_srv = nullptr;
    const Diligent::RefCntAutoPtr<Diligent::ITextureView>* shadow_srv = nullptr;
    bool shadow_tex_ready = false;
    const Diligent::RefCntAutoPtr<Diligent::ITextureView>* point_shadow_srv = nullptr;
    bool point_shadow_tex_ready = false;

    const glm::vec4* shadow_params0 = nullptr;
    const glm::vec4* shadow_params1 = nullptr;
    const glm::vec4* shadow_params2 = nullptr;
    const glm::vec4* shadow_bias_params = nullptr;
    const glm::vec4* shadow_axis_right = nullptr;
    const glm::vec4* shadow_axis_up = nullptr;
    const glm::vec4* shadow_axis_forward = nullptr;
    const glm::vec4* shadow_cascade_splits_uniform = nullptr;
    const glm::vec4* shadow_cascade_world_texel_uniform = nullptr;
    const glm::vec4* shadow_cascade_params_uniform = nullptr;
    const glm::vec4* shadow_camera_position = nullptr;
    const glm::vec4* shadow_camera_forward = nullptr;
    const std::array<glm::mat4, kDiligentDirectionalShadowCascadeCount>* shadow_cascade_uv_proj =
        nullptr;
    const glm::vec4* local_light_count_uniform = nullptr;
    const glm::vec4* local_light_params_uniform = nullptr;
    const std::array<glm::vec4, kDiligentMaxLocalLights>* local_light_pos_range = nullptr;
    const std::array<glm::vec4, kDiligentMaxLocalLights>* local_light_color_intensity = nullptr;
    const std::array<glm::vec4, kDiligentMaxLocalLights>* local_light_shadow_slot = nullptr;
    const glm::vec4* point_shadow_params_uniform = nullptr;
    const glm::vec4* point_shadow_atlas_texel_uniform = nullptr;
    const glm::vec4* point_shadow_tuning_uniform = nullptr;
    const std::array<glm::mat4, kDiligentPointShadowMatrixCount>* point_shadow_uv_proj = nullptr;
};

Diligent::RefCntAutoPtr<Diligent::ITextureView> CreateDiligentWhiteTexture(
    Diligent::IRenderDevice* device);

Diligent::RefCntAutoPtr<Diligent::ITextureView> CreateDiligentTextureView(
    Diligent::IRenderDevice* device,
    const renderer::MeshData::TextureData& tex);

renderer::MeshId CreateDiligentMeshAsset(
    Diligent::IRenderDevice* device,
    const Diligent::RefCntAutoPtr<Diligent::ITextureView>& white_srv,
    const renderer::MeshData& mesh,
    renderer::MeshId& next_mesh_id,
    std::unordered_map<renderer::MeshId, DiligentMesh>& meshes);

void DestroyDiligentMeshAsset(
    renderer::MeshId mesh,
    std::unordered_map<renderer::MeshId, DiligentMesh>& meshes);

renderer::MaterialId CreateDiligentMaterialAsset(
    Diligent::IRenderDevice* device,
    const renderer::MaterialDesc& material,
    bool supports_direct_multi_sampler_inputs,
    renderer::MaterialId& next_material_id,
    std::unordered_map<renderer::MaterialId, DiligentMaterial>& materials);

void DestroyDiligentMaterialAsset(
    renderer::MaterialId material,
    std::unordered_map<renderer::MaterialId, DiligentMaterial>& materials);

DiligentDirectionalShadowUpdateResult UpdateDiligentDirectionalShadowCache(
    DiligentShadowState& state,
    renderer::LayerId layer,
    const renderer::CameraData& camera,
    const renderer::DirectionalLightData& light,
    bool world_layer,
    bool gpu_shadow_requested,
    const detail::ResolvedDirectionalShadowSemantics& shadow_semantics,
    float aspect,
    const detail::DirectionalShadowView& shadow_view,
    const std::vector<DiligentRenderableDraw>& renderables,
    const std::vector<detail::DirectionalShadowCaster>& shadow_casters,
    const std::vector<renderer::DrawItem>& current_shadow_items,
    const std::unordered_map<renderer::MeshId, DiligentMesh>& meshes,
    const std::unordered_map<renderer::MaterialId, DiligentMaterial>& materials);

DiligentPointShadowUpdateResult UpdateDiligentPointShadowCache(
    DiligentShadowState& state,
    const detail::ResolvedDirectionalShadowSemantics& shadow_semantics,
    bool world_layer,
    const std::vector<renderer::LightData>& lights,
    const std::vector<detail::DirectionalShadowCaster>& shadow_casters,
    const std::vector<int>& point_shadow_selected_indices,
    bool point_shadow_structural_change,
    const std::vector<glm::vec4>& moved_point_shadow_caster_bounds);

void ResetDiligentShadowResources(DiligentShadowState& state);

DiligentDirectSamplerContractState EvaluateDiligentDirectSamplerContractState(
    const std::array<detail::SamplerVariableAvailability, detail::kDiligentMaterialVariantCount>&
        material_sampler_availability,
    const detail::SamplerVariableAvailability& line_sampler_availability);

bool CreateDiligentMainPipelineShadersAndConstants(
    Diligent::IRenderDevice* device,
    uint32_t constants_buffer_size,
    Diligent::RefCntAutoPtr<Diligent::IBuffer>& constant_buffer,
    Diligent::RefCntAutoPtr<Diligent::IShader>& out_vs,
    Diligent::RefCntAutoPtr<Diligent::IShader>& out_ps);

void CreateDiligentPipelineVariant(
    Diligent::IRenderDevice* device,
    Diligent::ISwapChain* swapchain,
    Diligent::IBuffer* constant_buffer,
    const char* name,
    Diligent::IShader* vs,
    Diligent::IShader* ps,
    bool alpha_blend,
    bool double_sided,
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>& out_pso,
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding>& out_srb);

void CreateDiligentShadowDepthPipeline(
    Diligent::IRenderDevice* device,
    Diligent::IBuffer* constant_buffer,
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>& out_pso,
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding>& out_srb);

void CreateDiligentLinePipeline(
    Diligent::IRenderDevice* device,
    Diligent::ISwapChain* swapchain,
    Diligent::IBuffer* constant_buffer,
    uint32_t line_vertex_stride,
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>& out_pso,
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding>& out_srb,
    Diligent::RefCntAutoPtr<Diligent::IBuffer>& out_line_vertex_buffer);

void SetDiligentViewport(Diligent::IDeviceContext* context, uint32_t width, uint32_t height);
void SubmitDiligentRenderLayerDraws(const DiligentRenderSubmissionInput& input);

} // namespace karma::renderer_backend

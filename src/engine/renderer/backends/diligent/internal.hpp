#pragma once

#include "../direct_sampler_observability_internal.hpp"
#include "../material_semantics_internal.hpp"

#include "karma/renderer/backend.hpp"

#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
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

} // namespace karma::renderer_backend

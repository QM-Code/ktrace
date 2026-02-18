#include "internal.hpp"

#include "../internal/direct_sampler_observability.hpp"
#include "../internal/material_lighting.hpp"

#include "karma/common/logging/logging.hpp"

#include <spdlog/spdlog.h>

#include <DiligentCore/Graphics/GraphicsTools/interface/MapHelper.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace karma::renderer_backend {
namespace {

constexpr std::size_t kMaxLocalLights = kDiligentMaxLocalLights;
constexpr std::size_t kPointShadowMatrixCount = kDiligentPointShadowMatrixCount;

std::size_t PipelineVariantIndex(bool alpha_blend, bool double_sided) {
    return (alpha_blend ? 2u : 0u) + (double_sided ? 1u : 0u);
}

const DiligentPipelineVariant* PipelineForMaterial(
    const std::array<DiligentPipelineVariant, detail::kDiligentMaterialVariantCount>& pipeline_variants,
    const detail::ResolvedMaterialSemantics& semantics) {
    return &pipeline_variants[PipelineVariantIndex(semantics.alpha_blend, semantics.double_sided)];
}

bool IsCompleteDiligentRenderSubmissionInput(const DiligentRenderSubmissionInput& input) {
    return input.camera &&
           input.light &&
           input.environment_semantics &&
           input.renderables &&
           input.debug_lines &&
           input.shadow_map &&
           input.direct_sampler_disable_reason &&
           input.context &&
           input.pipeline_variants &&
           input.line_pipeline &&
           input.constant_buffer &&
           input.white_srv &&
           input.shadow_srv &&
           input.point_shadow_srv &&
           input.shadow_params0 &&
           input.shadow_params1 &&
           input.shadow_params2 &&
           input.shadow_bias_params &&
           input.shadow_axis_right &&
           input.shadow_axis_up &&
           input.shadow_axis_forward &&
           input.shadow_cascade_splits_uniform &&
           input.shadow_cascade_world_texel_uniform &&
           input.shadow_cascade_params_uniform &&
           input.shadow_camera_position &&
           input.shadow_camera_forward &&
           input.shadow_cascade_uv_proj &&
           input.local_light_count_uniform &&
           input.local_light_params_uniform &&
           input.local_light_pos_range &&
           input.local_light_color_intensity &&
           input.local_light_shadow_slot &&
           input.point_shadow_params_uniform &&
           input.point_shadow_atlas_texel_uniform &&
           input.point_shadow_tuning_uniform &&
           input.point_shadow_uv_proj;
}

} // namespace

void SetDiligentViewport(Diligent::IDeviceContext* context, uint32_t width, uint32_t height) {
    if (!context) {
        return;
    }
    const uint32_t clamped_width = std::max(1u, width);
    const uint32_t clamped_height = std::max(1u, height);
    Diligent::Viewport viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(clamped_width);
    viewport.Height = static_cast<float>(clamped_height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context->SetViewports(1, &viewport, clamped_width, clamped_height);
}

void SubmitDiligentRenderLayerDraws(const DiligentRenderSubmissionInput& input) {
    if (!IsCompleteDiligentRenderSubmissionInput(input)) {
        return;
    }

    const auto& camera = *input.camera;
    const auto& light = *input.light;
    const auto& environment_semantics = *input.environment_semantics;
    const auto& renderables = *input.renderables;
    const auto& debug_lines = *input.debug_lines;
    const auto& shadow_map = *input.shadow_map;

    int shadow_covered = -1;
    if (shadow_map.ready && !shadow_map.depth.empty()) {
        int covered = 0;
        for (float value : shadow_map.depth) {
            if (std::isfinite(value) && value < 0.999f) {
                ++covered;
            }
        }
        shadow_covered = covered;
    }

    std::size_t direct_sampler_draws = 0u;
    std::size_t fallback_sampler_draws = 0u;
    std::size_t direct_contract_draws = 0u;
    std::size_t forced_fallback_draws = 0u;
    std::size_t unexpected_direct_draws = 0u;

    for (const DiligentRenderableDraw& renderable : renderables) {
        const auto& item = *renderable.item;
        const auto& mesh = *renderable.mesh;
        const auto& mat = *renderable.material;
        const auto& semantics = mat.semantics;

        const DiligentPipelineVariant* pipeline =
            PipelineForMaterial(*input.pipeline_variants, semantics);
        if (!pipeline || !pipeline->pso || !pipeline->srb) {
            continue;
        }
        input.context->SetPipelineState(pipeline->pso);

        float aspect = (input.height > 0) ? float(input.width) / float(input.height) : 1.0f;
        glm::mat4 view = glm::lookAt(camera.position, camera.target, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 proj = glm::perspective(glm::radians(camera.fov_y_degrees), aspect, camera.near_clip, camera.far_clip);
        DiligentRenderConstants constants{};
        constants.u_modelViewProj = proj * view * item.transform;
        constants.u_model = item.transform;
        const detail::ResolvedMaterialLighting lighting =
            detail::ResolveMaterialLighting(semantics, light, environment_semantics, 1.0f);
        if (!detail::ValidateResolvedMaterialLighting(lighting)) {
            continue;
        }
        constants.u_color = lighting.color;
        const glm::vec3 lighting_direction = -light.direction;
        constants.u_lightDir = glm::vec4(lighting_direction, 0.0f);
        constants.u_lightColor = lighting.light_color;
        constants.u_ambientColor = lighting.ambient_color;
        constants.u_unlit = glm::vec4(light.unlit, 0.0f, 0.0f, 0.0f);
        constants.u_shadowParams0 = *input.shadow_params0;
        constants.u_shadowParams1 = *input.shadow_params1;
        constants.u_shadowParams2 = *input.shadow_params2;
        constants.u_shadowBiasParams = *input.shadow_bias_params;
        constants.u_shadowAxisRight = *input.shadow_axis_right;
        constants.u_shadowAxisUp = *input.shadow_axis_up;
        constants.u_shadowAxisForward = *input.shadow_axis_forward;
        constants.u_shadowCascadeSplits = *input.shadow_cascade_splits_uniform;
        constants.u_shadowCascadeWorldTexel = *input.shadow_cascade_world_texel_uniform;
        constants.u_shadowCascadeParams = *input.shadow_cascade_params_uniform;
        constants.u_shadowCameraPosition = *input.shadow_camera_position;
        constants.u_shadowCameraForward = *input.shadow_camera_forward;
        constants.u_shadowCascadeUvProj = *input.shadow_cascade_uv_proj;
        constants.u_localLightCount = *input.local_light_count_uniform;
        constants.u_localLightParams = *input.local_light_params_uniform;
        constants.u_localLightPosRange = *input.local_light_pos_range;
        constants.u_localLightColorIntensity = *input.local_light_color_intensity;
        constants.u_localLightShadowSlot = *input.local_light_shadow_slot;
        constants.u_pointShadowParams = *input.point_shadow_params_uniform;
        constants.u_pointShadowAtlasTexel = *input.point_shadow_atlas_texel_uniform;
        constants.u_pointShadowTuning = *input.point_shadow_tuning_uniform;
        constants.u_pointShadowUvProj = *input.point_shadow_uv_proj;
        if (semantics.alpha_blend) {
            constants.u_shadowParams0.x = 0.0f;
            constants.u_pointShadowParams.x = 0.0f;
            constants.u_pointShadowParams.w = 0.0f;
        }

        const bool use_direct_sampler_path =
            input.supports_direct_multi_sampler_inputs &&
            mat.shader_input_path == detail::MaterialShaderTextureInputPath::DirectMultiSampler;
        const bool contract_requests_direct =
            mat.shader_input_path == detail::MaterialShaderTextureInputPath::DirectMultiSampler;
        if (contract_requests_direct) {
            ++direct_contract_draws;
        }
        if (use_direct_sampler_path) {
            ++direct_sampler_draws;
        } else {
            ++fallback_sampler_draws;
        }
        if (use_direct_sampler_path && !contract_requests_direct) {
            ++unexpected_direct_draws;
        }
        if (!use_direct_sampler_path && contract_requests_direct) {
            ++forced_fallback_draws;
        }
        constants.u_textureMode = glm::vec4(
            (use_direct_sampler_path && mat.shader_uses_normal_input) ? 1.0f : 0.0f,
            (use_direct_sampler_path && mat.shader_uses_occlusion_input) ? 1.0f : 0.0f,
            0.0f,
            0.0f);

        Diligent::MapHelper<DiligentRenderConstants> cb_data(
            input.context,
            input.constant_buffer,
            Diligent::MAP_WRITE,
            Diligent::MAP_FLAG_DISCARD);
        *cb_data = constants;

        if (pipeline->srb) {
            if (auto* tex_var = pipeline->srb->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "s_tex")) {
                auto texture_view = use_direct_sampler_path
                    ? (mat.srv ? mat.srv : *input.white_srv)
                    : (mat.srv ? mat.srv : mesh.srv);
                if (!texture_view) {
                    texture_view = *input.white_srv;
                }
                tex_var->Set(texture_view, Diligent::SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
            }
            if (auto* normal_var = pipeline->srb->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "s_normal")) {
                auto normal_view = (use_direct_sampler_path && mat.normal_srv) ? mat.normal_srv : *input.white_srv;
                if (!normal_view) {
                    normal_view = *input.white_srv;
                }
                normal_var->Set(normal_view, Diligent::SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
            }
            if (auto* occlusion_var = pipeline->srb->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "s_occlusion")) {
                auto occlusion_view = (use_direct_sampler_path && mat.occlusion_srv) ? mat.occlusion_srv : *input.white_srv;
                if (!occlusion_view) {
                    occlusion_view = *input.white_srv;
                }
                occlusion_var->Set(occlusion_view, Diligent::SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
            }
            if (auto* shadow_var = pipeline->srb->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "s_shadow")) {
                auto shadow_view = input.shadow_tex_ready ? *input.shadow_srv : *input.white_srv;
                if (!shadow_view) {
                    shadow_view = *input.white_srv;
                }
                shadow_var->Set(shadow_view, Diligent::SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
            }
            if (auto* point_shadow_var =
                    pipeline->srb->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "s_pointShadow")) {
                auto point_shadow_view = input.point_shadow_tex_ready ? *input.point_shadow_srv : *input.white_srv;
                if (!point_shadow_view) {
                    point_shadow_view = *input.white_srv;
                }
                point_shadow_var->Set(
                    point_shadow_view,
                    Diligent::SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
            }
        }
        input.context->CommitShaderResources(
            pipeline->srb,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        const uint64_t offsets[] = {0};
        Diligent::IBuffer* vbs[] = {mesh.vb};
        input.context->SetVertexBuffers(
            0,
            1,
            vbs,
            offsets,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
        input.context->SetIndexBuffer(mesh.ib, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        Diligent::DrawIndexedAttribs draw{};
        draw.IndexType = Diligent::VT_UINT32;
        draw.NumIndices = mesh.num_indices;
        draw.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;
        input.context->DrawIndexed(draw);
    }

    if (shadow_map.ready) {
        const char* shadow_source = shadow_map.depth.empty() ? "gpu_projection" : "cpu_raster";
        KARMA_TRACE_CHANGED(
            "render.diligent",
            std::to_string(input.layer) + ":" +
                std::to_string(shadow_map.ready ? 1 : 0) + ":" +
                shadow_source + ":" +
                std::to_string(shadow_covered) + ":" +
                std::to_string(shadow_map.size) + ":" +
                std::to_string(input.shadow_tex_ready ? 1 : 0),
            "shadow map layer={} source={} mapReady={} covered={} size={} extent={:.1f} uploaded={}",
            input.layer,
            shadow_source,
            shadow_map.ready ? 1 : 0,
            shadow_covered,
            shadow_map.size,
            shadow_map.extent,
            input.shadow_tex_ready ? 1 : 0);
    }

    KARMA_TRACE_CHANGED(
        "render.diligent",
        std::to_string(input.layer) + ":" +
            std::to_string(renderables.size()) + ":" +
            std::to_string(direct_sampler_draws) + ":" +
            std::to_string(fallback_sampler_draws) + ":" +
            std::to_string(input.supports_direct_multi_sampler_inputs ? 1 : 0) + ":" +
            *input.direct_sampler_disable_reason,
        "direct sampler frame layer={} draws={} direct={} fallback={} enabled={} reason={}",
        input.layer,
        renderables.size(),
        direct_sampler_draws,
        fallback_sampler_draws,
        input.supports_direct_multi_sampler_inputs ? 1 : 0,
        *input.direct_sampler_disable_reason);

    const detail::DirectSamplerDrawInvariantReport draw_invariants =
        detail::EvaluateDirectSamplerDrawInvariants(
            detail::DirectSamplerDrawInvariantInput{
                input.supports_direct_multi_sampler_inputs,
                renderables.size(),
                direct_contract_draws,
                direct_sampler_draws,
                fallback_sampler_draws,
                forced_fallback_draws,
                unexpected_direct_draws,
            });
    if (!draw_invariants.ok) {
        spdlog::error(
            "Diligent: direct sampler assertion failed (enabled={}, directContract={}, directDraws={}, fallbackDraws={}, forcedFallback={}, unexpectedDirect={}, reason={}, invariant={})",
            input.supports_direct_multi_sampler_inputs ? 1 : 0,
            direct_contract_draws,
            direct_sampler_draws,
            fallback_sampler_draws,
            forced_fallback_draws,
            unexpected_direct_draws,
            *input.direct_sampler_disable_reason,
            draw_invariants.reason);
    }

    KARMA_TRACE_CHANGED(
        "render.diligent",
        std::to_string(input.layer) + ":" +
            std::to_string(renderables.size()) + ":" +
            std::to_string(direct_contract_draws) + ":" +
            std::to_string(direct_sampler_draws) + ":" +
            std::to_string(fallback_sampler_draws) + ":" +
            std::to_string(forced_fallback_draws) + ":" +
            std::to_string(unexpected_direct_draws) + ":" +
            std::to_string(draw_invariants.ok ? 1 : 0) + ":" +
            draw_invariants.reason,
        "direct sampler assertions layer={} draws={} contractDirect={} actualDirect={} fallback={} forcedFallback={} unexpectedDirect={} ok={} enabled={} reason={} invariant={}",
        input.layer,
        renderables.size(),
        direct_contract_draws,
        direct_sampler_draws,
        fallback_sampler_draws,
        forced_fallback_draws,
        unexpected_direct_draws,
        draw_invariants.ok ? 1 : 0,
        input.supports_direct_multi_sampler_inputs ? 1 : 0,
        *input.direct_sampler_disable_reason,
        draw_invariants.reason);

    if (input.line_pipeline->pso && input.line_pipeline->srb && input.line_vertex_buffer) {
        float aspect = (input.height > 0) ? float(input.width) / float(input.height) : 1.0f;
        glm::mat4 view = glm::lookAt(camera.position, camera.target, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 proj =
            glm::perspective(glm::radians(camera.fov_y_degrees), aspect, camera.near_clip, camera.far_clip);

        for (const auto& line : debug_lines) {
            if (line.layer != input.layer) {
                continue;
            }

            input.context->SetPipelineState(input.line_pipeline->pso);
            {
                Diligent::MapHelper<DiligentLineVertex> line_data(
                    input.context,
                    input.line_vertex_buffer,
                    Diligent::MAP_WRITE,
                    Diligent::MAP_FLAG_DISCARD);
                line_data[0] = DiligentLineVertex{
                    line.start.x,
                    line.start.y,
                    line.start.z,
                    0.0f,
                    1.0f,
                    0.0f,
                    0.0f,
                    0.0f};
                line_data[1] = DiligentLineVertex{
                    line.end.x,
                    line.end.y,
                    line.end.z,
                    0.0f,
                    1.0f,
                    0.0f,
                    0.0f,
                    0.0f};
            }

            DiligentRenderConstants line_constants{};
            line_constants.u_modelViewProj = proj * view;
            line_constants.u_color = line.color;
            line_constants.u_lightDir = glm::vec4(0.0f);
            line_constants.u_lightColor = glm::vec4(0.0f);
            line_constants.u_ambientColor = glm::vec4(0.0f);
            line_constants.u_unlit = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
            line_constants.u_textureMode = glm::vec4(0.0f);
            Diligent::MapHelper<DiligentRenderConstants> cb_data(
                input.context,
                input.constant_buffer,
                Diligent::MAP_WRITE,
                Diligent::MAP_FLAG_DISCARD);
            *cb_data = line_constants;

            if (auto* tex_var =
                    input.line_pipeline->srb->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "s_tex")) {
                tex_var->Set(*input.white_srv, Diligent::SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
            }
            if (auto* tex_var =
                    input.line_pipeline->srb->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "s_normal")) {
                tex_var->Set(*input.white_srv, Diligent::SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
            }
            if (auto* tex_var =
                    input.line_pipeline->srb->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "s_occlusion")) {
                tex_var->Set(*input.white_srv, Diligent::SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
            }
            input.context->CommitShaderResources(
                input.line_pipeline->srb,
                Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            const uint64_t offsets[] = {0};
            Diligent::IBuffer* vbs[] = {input.line_vertex_buffer};
            input.context->SetVertexBuffers(
                0,
                1,
                vbs,
                offsets,
                Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
            input.context->SetIndexBuffer(nullptr, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            Diligent::DrawAttribs draw{};
            draw.NumVertices = 2;
            draw.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;
            input.context->Draw(draw);
        }
    }
}

} // namespace karma::renderer_backend

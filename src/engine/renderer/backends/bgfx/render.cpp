#include "internal.hpp"

#include "../internal/direct_sampler_observability.hpp"
#include "../internal/material_lighting.hpp"

#include "karma/common/logging.hpp"

#include <spdlog/spdlog.h>

#include <bgfx/bgfx.h>
#include <bx/math.h>

#include <glm/glm.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

namespace karma::renderer_backend {
namespace {

constexpr bgfx::ViewId kBgfxShadowViewIdBase = 0;
constexpr std::size_t kDirectionalShadowCascadeCount = kBgfxDirectionalShadowCascadeCount;
constexpr bgfx::ViewId kBgfxMainViewId =
    static_cast<bgfx::ViewId>(kBgfxShadowViewIdBase + kDirectionalShadowCascadeCount);
constexpr std::size_t kMaxLocalLights = kBgfxMaxLocalLights;
constexpr std::size_t kMaxPointShadowMatrices = kBgfxMaxPointShadowMatrices;

bool IsCompleteBgfxRenderSubmissionInput(const BgfxRenderSubmissionInput& input) {
    return input.light &&
           input.environment_semantics &&
           input.renderables &&
           input.debug_lines &&
           input.shadow_map &&
           input.direct_sampler_disable_reason &&
           input.layout &&
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

uint32_t PackBgfxColorRgba8(const glm::vec4& color) {
    const auto to_u8 = [](float value) -> uint32_t {
        const float scaled = std::clamp(value, 0.0f, 1.0f) * 255.0f;
        return static_cast<uint32_t>(scaled + 0.5f);
    };
    return (to_u8(color.r) << 24u) |
           (to_u8(color.g) << 16u) |
           (to_u8(color.b) << 8u) |
           to_u8(color.a);
}

void SubmitBgfxRenderLayerDraws(const BgfxRenderSubmissionInput& input) {
    if (!IsCompleteBgfxRenderSubmissionInput(input) || !bgfx::isValid(input.program)) {
        return;
    }

    const auto& light = *input.light;
    const auto& environment_semantics = *input.environment_semantics;
    const auto& renderables = *input.renderables;
    const auto& debug_lines = *input.debug_lines;
    const auto& shadow_map = *input.shadow_map;
    const auto& uniforms = input.uniforms;

    std::size_t direct_sampler_draws = 0u;
    std::size_t fallback_sampler_draws = 0u;
    std::size_t direct_contract_draws = 0u;
    std::size_t forced_fallback_draws = 0u;
    std::size_t unexpected_direct_draws = 0u;

    for (const BgfxRenderableDraw& renderable : renderables) {
        const auto& item = *renderable.item;
        const auto& semantics = renderable.material->semantics;

        bgfx::setVertexBuffer(0, renderable.mesh->vbh);
        bgfx::setIndexBuffer(renderable.mesh->ibh);
        bgfx::setTransform(&item.transform[0][0]);

        const detail::ResolvedMaterialLighting lighting =
            detail::ResolveMaterialLighting(semantics, light, environment_semantics, 1.0f);
        if (!detail::ValidateResolvedMaterialLighting(lighting)) {
            continue;
        }

        const glm::vec3 lighting_direction = -light.direction;
        const float light_dir[4] = {lighting_direction.x, lighting_direction.y, lighting_direction.z, 0.0f};
        const float material_color[4] = {
            lighting.color.r,
            lighting.color.g,
            lighting.color.b,
            lighting.color.a};
        const float light_color[4] = {
            lighting.light_color.r,
            lighting.light_color.g,
            lighting.light_color.b,
            lighting.light_color.a};
        const float ambient_color[4] = {
            lighting.ambient_color.r,
            lighting.ambient_color.g,
            lighting.ambient_color.b,
            lighting.ambient_color.a};
        const float unlit[4] = {light.unlit, 0.0f, 0.0f, 0.0f};
        bgfx::setUniform(uniforms.u_color, material_color);
        bgfx::setUniform(uniforms.u_light_dir, light_dir);
        bgfx::setUniform(uniforms.u_light_color, light_color);
        bgfx::setUniform(uniforms.u_ambient_color, ambient_color);
        bgfx::setUniform(uniforms.u_unlit, unlit);
        bgfx::setUniform(uniforms.u_shadow_params0, input.shadow_params0);
        bgfx::setUniform(uniforms.u_shadow_params1, input.shadow_params1);
        bgfx::setUniform(uniforms.u_shadow_params2, input.shadow_params2);
        bgfx::setUniform(uniforms.u_shadow_bias_params, input.shadow_bias_params);
        bgfx::setUniform(uniforms.u_shadow_axis_right, input.shadow_axis_right);
        bgfx::setUniform(uniforms.u_shadow_axis_up, input.shadow_axis_up);
        bgfx::setUniform(uniforms.u_shadow_axis_forward, input.shadow_axis_forward);
        if (bgfx::isValid(uniforms.u_shadow_cascade_splits)) {
            bgfx::setUniform(uniforms.u_shadow_cascade_splits, input.shadow_cascade_splits_uniform);
        }
        if (bgfx::isValid(uniforms.u_shadow_cascade_world_texel)) {
            bgfx::setUniform(uniforms.u_shadow_cascade_world_texel, input.shadow_cascade_world_texel_uniform);
        }
        if (bgfx::isValid(uniforms.u_shadow_cascade_params)) {
            bgfx::setUniform(uniforms.u_shadow_cascade_params, input.shadow_cascade_params_uniform);
        }
        if (bgfx::isValid(uniforms.u_shadow_camera_position)) {
            bgfx::setUniform(uniforms.u_shadow_camera_position, input.shadow_camera_position);
        }
        if (bgfx::isValid(uniforms.u_shadow_camera_forward)) {
            bgfx::setUniform(uniforms.u_shadow_camera_forward, input.shadow_camera_forward);
        }
        if (bgfx::isValid(uniforms.u_shadow_cascade_uv_proj)) {
            bgfx::setUniform(
                uniforms.u_shadow_cascade_uv_proj,
                input.shadow_cascade_uv_proj->data(),
                static_cast<uint16_t>(kDirectionalShadowCascadeCount));
        }
        if (bgfx::isValid(uniforms.u_local_light_count)) {
            bgfx::setUniform(uniforms.u_local_light_count, input.local_light_count_uniform);
        }
        if (bgfx::isValid(uniforms.u_local_light_params)) {
            bgfx::setUniform(uniforms.u_local_light_params, input.local_light_params_uniform);
        }
        if (bgfx::isValid(uniforms.u_local_light_pos_range)) {
            bgfx::setUniform(
                uniforms.u_local_light_pos_range,
                input.local_light_pos_range->data(),
                static_cast<uint16_t>(kMaxLocalLights));
        }
        if (bgfx::isValid(uniforms.u_local_light_color_intensity)) {
            bgfx::setUniform(
                uniforms.u_local_light_color_intensity,
                input.local_light_color_intensity->data(),
                static_cast<uint16_t>(kMaxLocalLights));
        }
        if (bgfx::isValid(uniforms.u_local_light_shadow_slot)) {
            bgfx::setUniform(
                uniforms.u_local_light_shadow_slot,
                input.local_light_shadow_slot->data(),
                static_cast<uint16_t>(kMaxLocalLights));
        }
        if (bgfx::isValid(uniforms.u_point_shadow_params)) {
            bgfx::setUniform(uniforms.u_point_shadow_params, input.point_shadow_params_uniform);
        }
        if (bgfx::isValid(uniforms.u_point_shadow_atlas_texel)) {
            bgfx::setUniform(uniforms.u_point_shadow_atlas_texel, input.point_shadow_atlas_texel_uniform);
        }
        if (bgfx::isValid(uniforms.u_point_shadow_tuning)) {
            bgfx::setUniform(uniforms.u_point_shadow_tuning, input.point_shadow_tuning_uniform);
        }
        if (bgfx::isValid(uniforms.u_point_shadow_uv_proj)) {
            bgfx::setUniform(
                uniforms.u_point_shadow_uv_proj,
                input.point_shadow_uv_proj->data(),
                static_cast<uint16_t>(kMaxPointShadowMatrices));
        }

        const bool use_direct_sampler_path =
            input.supports_direct_multi_sampler_inputs &&
            renderable.material->shader_input_path == detail::MaterialShaderTextureInputPath::DirectMultiSampler;
        const bool contract_requests_direct =
            renderable.material->shader_input_path == detail::MaterialShaderTextureInputPath::DirectMultiSampler;
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

        bgfx::TextureHandle direct_albedo_texture = input.white_tex;
        if (bgfx::isValid(renderable.material->tex)) {
            direct_albedo_texture = renderable.material->tex;
        }
        bgfx::TextureHandle fallback_base_texture = input.white_tex;
        if (bgfx::isValid(renderable.material->tex)) {
            fallback_base_texture = renderable.material->tex;
        } else if (bgfx::isValid(renderable.mesh->tex)) {
            fallback_base_texture = renderable.mesh->tex;
        }
        const bgfx::TextureHandle base_texture =
            use_direct_sampler_path ? direct_albedo_texture : fallback_base_texture;
        if (bgfx::isValid(base_texture)) {
            bgfx::setTexture(0, uniforms.s_tex, base_texture);
        }
        if (bgfx::isValid(uniforms.s_normal)) {
            const bgfx::TextureHandle normal_texture =
                (use_direct_sampler_path && bgfx::isValid(renderable.material->normal_tex))
                    ? renderable.material->normal_tex
                    : input.white_tex;
            if (bgfx::isValid(normal_texture)) {
                bgfx::setTexture(1, uniforms.s_normal, normal_texture);
            }
        }
        if (bgfx::isValid(uniforms.s_occlusion)) {
            const bgfx::TextureHandle occlusion_texture =
                (use_direct_sampler_path && bgfx::isValid(renderable.material->occlusion_tex))
                    ? renderable.material->occlusion_tex
                    : input.white_tex;
            if (bgfx::isValid(occlusion_texture)) {
                bgfx::setTexture(2, uniforms.s_occlusion, occlusion_texture);
            }
        }
        if (bgfx::isValid(uniforms.s_shadow)) {
            const bgfx::TextureHandle shadow_texture =
                input.shadow_tex_ready ? input.shadow_tex : input.white_tex;
            if (bgfx::isValid(shadow_texture)) {
                bgfx::setTexture(3, uniforms.s_shadow, shadow_texture);
            }
        }
        if (bgfx::isValid(uniforms.s_point_shadow)) {
            const bgfx::TextureHandle point_shadow_texture =
                input.point_shadow_tex_ready ? input.point_shadow_tex : input.white_tex;
            if (bgfx::isValid(point_shadow_texture)) {
                bgfx::setTexture(4, uniforms.s_point_shadow, point_shadow_texture);
            }
        }
        if (bgfx::isValid(uniforms.u_texture_mode)) {
            const float texture_mode[4] = {
                (use_direct_sampler_path && renderable.material->shader_uses_normal_input) ? 1.0f : 0.0f,
                (use_direct_sampler_path && renderable.material->shader_uses_occlusion_input) ? 1.0f : 0.0f,
                0.0f,
                0.0f,
            };
            bgfx::setUniform(uniforms.u_texture_mode, texture_mode);
        }
        uint64_t state =
            BGFX_STATE_WRITE_RGB |
            BGFX_STATE_WRITE_A |
            BGFX_STATE_WRITE_Z |
            BGFX_STATE_DEPTH_TEST_LESS |
            BGFX_STATE_MSAA;
        if (!semantics.double_sided) {
            state |= BGFX_STATE_CULL_CW;
        }
        if (semantics.alpha_blend) {
            state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);
        }
        bgfx::setState(state);
        bgfx::submit(kBgfxMainViewId, input.program);
    }

    if (shadow_map.ready) {
        const char* shadow_source = shadow_map.depth.empty() ? "gpu_projection" : "cpu_raster";
        int shadow_covered = -1;
        if (!shadow_map.depth.empty()) {
            int covered = 0;
            for (float value : shadow_map.depth) {
                if (std::isfinite(value)) {
                    ++covered;
                }
            }
            shadow_covered = covered;
        }
        KARMA_TRACE_CHANGED(
            "render.bgfx",
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
        "render.bgfx",
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
            "Graphics(Bgfx): direct sampler assertion failed (enabled={}, directContract={}, directDraws={}, fallbackDraws={}, forcedFallback={}, unexpectedDirect={}, reason={}, invariant={})",
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
        "render.bgfx",
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

    for (const auto& line : debug_lines) {
        if (line.layer != input.layer) {
            continue;
        }

        if (bgfx::getAvailTransientVertexBuffer(2, *input.layout) < 2) {
            continue;
        }
        bgfx::TransientVertexBuffer tvb{};
        bgfx::allocTransientVertexBuffer(&tvb, 2, *input.layout);

        PosNormalVertex vertices[2]{
            {line.start.x, line.start.y, line.start.z, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
            {line.end.x, line.end.y, line.end.z, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
        };
        std::memcpy(tvb.data, vertices, sizeof(vertices));
        bgfx::setVertexBuffer(0, &tvb, 0, 2);

        const float line_color[4] = {line.color.r, line.color.g, line.color.b, line.color.a};
        const float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        const float unlit[4] = {1.0f, 0.0f, 0.0f, 0.0f};
        bgfx::setUniform(uniforms.u_color, line_color);
        bgfx::setUniform(uniforms.u_light_dir, zeros);
        bgfx::setUniform(uniforms.u_light_color, zeros);
        bgfx::setUniform(uniforms.u_ambient_color, zeros);
        bgfx::setUniform(uniforms.u_unlit, unlit);
        bgfx::setUniform(uniforms.u_shadow_params0, zeros);
        bgfx::setUniform(uniforms.u_shadow_params1, zeros);
        bgfx::setUniform(uniforms.u_shadow_params2, zeros);
        bgfx::setUniform(uniforms.u_shadow_bias_params, zeros);
        bgfx::setUniform(uniforms.u_shadow_axis_right, zeros);
        bgfx::setUniform(uniforms.u_shadow_axis_up, zeros);
        bgfx::setUniform(uniforms.u_shadow_axis_forward, zeros);
        if (bgfx::isValid(uniforms.u_shadow_cascade_splits)) {
            bgfx::setUniform(uniforms.u_shadow_cascade_splits, zeros);
        }
        if (bgfx::isValid(uniforms.u_shadow_cascade_world_texel)) {
            bgfx::setUniform(uniforms.u_shadow_cascade_world_texel, zeros);
        }
        if (bgfx::isValid(uniforms.u_shadow_cascade_params)) {
            bgfx::setUniform(uniforms.u_shadow_cascade_params, zeros);
        }
        if (bgfx::isValid(uniforms.u_shadow_camera_position)) {
            bgfx::setUniform(uniforms.u_shadow_camera_position, zeros);
        }
        if (bgfx::isValid(uniforms.u_shadow_camera_forward)) {
            bgfx::setUniform(uniforms.u_shadow_camera_forward, zeros);
        }
        if (bgfx::isValid(uniforms.u_shadow_cascade_uv_proj)) {
            std::array<glm::mat4, kDirectionalShadowCascadeCount> shadow_cascade_mats{};
            for (glm::mat4& matrix : shadow_cascade_mats) {
                matrix = glm::mat4(1.0f);
            }
            bgfx::setUniform(
                uniforms.u_shadow_cascade_uv_proj,
                shadow_cascade_mats.data(),
                static_cast<uint16_t>(kDirectionalShadowCascadeCount));
        }
        if (bgfx::isValid(uniforms.u_local_light_count)) {
            bgfx::setUniform(uniforms.u_local_light_count, zeros);
        }
        if (bgfx::isValid(uniforms.u_local_light_params)) {
            bgfx::setUniform(uniforms.u_local_light_params, zeros);
        }
        if (bgfx::isValid(uniforms.u_local_light_pos_range)) {
            const std::array<float, kMaxLocalLights * 4u> light_zero{};
            bgfx::setUniform(
                uniforms.u_local_light_pos_range,
                light_zero.data(),
                static_cast<uint16_t>(kMaxLocalLights));
        }
        if (bgfx::isValid(uniforms.u_local_light_color_intensity)) {
            const std::array<float, kMaxLocalLights * 4u> light_zero{};
            bgfx::setUniform(
                uniforms.u_local_light_color_intensity,
                light_zero.data(),
                static_cast<uint16_t>(kMaxLocalLights));
        }
        if (bgfx::isValid(uniforms.u_local_light_shadow_slot)) {
            std::array<float, kMaxLocalLights * 4u> shadow_slot_zero{};
            for (std::size_t slot = 0; slot < kMaxLocalLights; ++slot) {
                shadow_slot_zero[slot * 4u] = -1.0f;
            }
            bgfx::setUniform(
                uniforms.u_local_light_shadow_slot,
                shadow_slot_zero.data(),
                static_cast<uint16_t>(kMaxLocalLights));
        }
        if (bgfx::isValid(uniforms.u_point_shadow_params)) {
            bgfx::setUniform(uniforms.u_point_shadow_params, zeros);
        }
        if (bgfx::isValid(uniforms.u_point_shadow_atlas_texel)) {
            bgfx::setUniform(uniforms.u_point_shadow_atlas_texel, zeros);
        }
        if (bgfx::isValid(uniforms.u_point_shadow_tuning)) {
            bgfx::setUniform(uniforms.u_point_shadow_tuning, zeros);
        }
        if (bgfx::isValid(uniforms.u_point_shadow_uv_proj)) {
            std::array<glm::mat4, kMaxPointShadowMatrices> point_shadow_mats{};
            for (glm::mat4& matrix : point_shadow_mats) {
                matrix = glm::mat4(1.0f);
            }
            bgfx::setUniform(
                uniforms.u_point_shadow_uv_proj,
                point_shadow_mats.data(),
                static_cast<uint16_t>(kMaxPointShadowMatrices));
        }
        if (bgfx::isValid(uniforms.u_texture_mode)) {
            const float texture_mode[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            bgfx::setUniform(uniforms.u_texture_mode, texture_mode);
        }

        if (bgfx::isValid(input.white_tex)) {
            bgfx::setTexture(0, uniforms.s_tex, input.white_tex);
            if (bgfx::isValid(uniforms.s_normal)) {
                bgfx::setTexture(1, uniforms.s_normal, input.white_tex);
            }
            if (bgfx::isValid(uniforms.s_occlusion)) {
                bgfx::setTexture(2, uniforms.s_occlusion, input.white_tex);
            }
            if (bgfx::isValid(uniforms.s_shadow) && bgfx::isValid(input.shadow_tex)) {
                bgfx::setTexture(3, uniforms.s_shadow, input.shadow_tex);
            }
            if (bgfx::isValid(uniforms.s_point_shadow) && bgfx::isValid(input.point_shadow_tex)) {
                bgfx::setTexture(4, uniforms.s_point_shadow, input.point_shadow_tex);
            }
        }

        float identity[16];
        bx::mtxIdentity(identity);
        bgfx::setTransform(identity);

        uint64_t state =
            BGFX_STATE_WRITE_RGB |
            BGFX_STATE_WRITE_A |
            BGFX_STATE_DEPTH_TEST_LESS |
            BGFX_STATE_PT_LINES |
            BGFX_STATE_MSAA;
        if (line.color.a < 0.999f) {
            state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);
        }
        bgfx::setState(state);
        bgfx::submit(kBgfxMainViewId, input.program);
    }
}

} // namespace karma::renderer_backend

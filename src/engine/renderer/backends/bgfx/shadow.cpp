#include "internal.hpp"

#include "karma/common/logging.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace karma::renderer_backend {
namespace {

bool directionChangedBeyondThreshold(const glm::vec3& a, const glm::vec3& b, float threshold_deg) {
    const float a_len = glm::length(a);
    const float b_len = glm::length(b);
    if (!std::isfinite(a_len) || !std::isfinite(b_len) || a_len <= 1e-6f || b_len <= 1e-6f) {
        return true;
    }
    const glm::vec3 an = a / a_len;
    const glm::vec3 bn = b / b_len;
    const float cos_threshold = std::cos(glm::radians(std::max(0.0f, threshold_deg)));
    const float dot_v = glm::dot(an, bn);
    return dot_v < cos_threshold;
}

bool matrixChangedBeyondEpsilon(const glm::mat4& a, const glm::mat4& b, float eps = 1e-5f) {
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            if (std::abs(a[col][row] - b[col][row]) > eps) {
                return true;
            }
        }
    }
    return false;
}

glm::vec3 resolveCameraForward(const renderer::CameraData& camera) {
    const glm::vec3 forward = camera.target - camera.position;
    const float len = glm::length(forward);
    if (!std::isfinite(len) || len <= 1e-6f) {
        return glm::vec3{0.0f, 0.0f, -1.0f};
    }
    return forward / len;
}

glm::vec3 normalizeDirectionOrFallback(const glm::vec3& value, const glm::vec3& fallback) {
    const float len = glm::length(value);
    if (!std::isfinite(len) || len <= 1e-6f) {
        return fallback;
    }
    return value / len;
}

float maxTransformScale(const glm::mat4& transform) {
    const float sx = glm::length(glm::vec3(transform[0]));
    const float sy = glm::length(glm::vec3(transform[1]));
    const float sz = glm::length(glm::vec3(transform[2]));
    return std::max(sx, std::max(sy, sz));
}

void computeWorldShadowSphere(const glm::mat4& transform,
                              const glm::vec3& local_center,
                              float local_radius,
                              glm::vec3& out_center,
                              float& out_radius) {
    const glm::vec4 world_center = transform * glm::vec4(local_center, 1.0f);
    out_center = glm::vec3(world_center);
    const float scale = maxTransformScale(transform);
    out_radius = std::max(0.0f, local_radius * std::max(scale, 0.0f));
}

std::vector<float> BuildShadowDepthUploadData(const detail::DirectionalShadowMap& map) {
    const std::size_t expected =
        static_cast<std::size_t>(map.size) * static_cast<std::size_t>(map.size);
    std::vector<float> pixels(expected, 1.0f);
    if (map.depth.size() != expected) {
        return pixels;
    }
    for (std::size_t i = 0; i < expected; ++i) {
        const float value = map.depth[i];
        pixels[i] = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 1.0f;
    }
    return pixels;
}

std::vector<float> BuildPointShadowDepthUploadData(const detail::PointShadowMap& map) {
    const std::size_t expected =
        static_cast<std::size_t>(std::max(map.atlas_width, 0)) *
        static_cast<std::size_t>(std::max(map.atlas_height, 0));
    std::vector<float> pixels(expected, 1.0f);
    if (map.depth.size() != expected) {
        return pixels;
    }
    for (std::size_t i = 0; i < expected; ++i) {
        const float value = map.depth[i];
        pixels[i] = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 1.0f;
    }
    return pixels;
}

void ResetBgfxPointShadowCacheState(BgfxShadowState& state) {
    *state.cached_point_shadow_map = detail::PointShadowMap{};
    *state.point_shadow_tex_ready = false;
    *state.point_shadow_frames_until_update = 0;
    *state.point_shadow_cache_initialized = false;
    *state.point_shadow_face_cursor = 0u;
    state.point_shadow_face_dirty->fill(1u);
    state.point_shadow_slot_source_index->fill(-1);
    state.point_shadow_slot_position->fill(glm::vec3(0.0f, 0.0f, 0.0f));
    state.point_shadow_slot_range->fill(0.0f);
}

bool EnsureBgfxShadowTexture(BgfxShadowState& state, uint16_t size, bool render_target) {
    if (size == 0u) {
        return false;
    }
    const bool prefer_depth_attachment = render_target && state.shadow_depth_attachment_supported;
    const bool needs_recreate =
        !bgfx::isValid(*state.shadow_tex) ||
        *state.shadow_tex_size != size ||
        *state.shadow_tex_is_rt != render_target ||
        (render_target && (*state.shadow_tex_rt_uses_depth != prefer_depth_attachment));
    if (needs_recreate) {
        if (bgfx::isValid(*state.shadow_fb)) {
            bgfx::destroy(*state.shadow_fb);
            *state.shadow_fb = BGFX_INVALID_HANDLE;
        }
        if (bgfx::isValid(*state.shadow_tex)) {
            bgfx::destroy(*state.shadow_tex);
            *state.shadow_tex = BGFX_INVALID_HANDLE;
        }
        *state.shadow_tex_rt_uses_depth = false;

        const auto create_shadow_texture = [&](bgfx::TextureFormat::Enum format, bool rt_uses_depth) -> bool {
            uint64_t flags =
                BGFX_SAMPLER_U_CLAMP |
                BGFX_SAMPLER_V_CLAMP |
                BGFX_SAMPLER_COMPARE_LEQUAL;
            if (render_target) {
                flags |= BGFX_TEXTURE_RT;
            }
            *state.shadow_tex = bgfx::createTexture2D(
                size,
                size,
                false,
                1,
                format,
                flags,
                nullptr);
            if (!bgfx::isValid(*state.shadow_tex)) {
                return false;
            }
            *state.shadow_tex_size = size;
            *state.shadow_tex_is_rt = render_target;
            *state.shadow_tex_rt_uses_depth = render_target && rt_uses_depth;
            if (render_target) {
                bgfx::TextureHandle attachments[] = {*state.shadow_tex};
                *state.shadow_fb = bgfx::createFrameBuffer(1, attachments, false);
                if (!bgfx::isValid(*state.shadow_fb)) {
                    bgfx::destroy(*state.shadow_tex);
                    *state.shadow_tex = BGFX_INVALID_HANDLE;
                    *state.shadow_tex_size = 0u;
                    *state.shadow_tex_is_rt = false;
                    *state.shadow_tex_rt_uses_depth = false;
                    return false;
                }
            }
            return true;
        };

        bool created = false;
        if (render_target && prefer_depth_attachment) {
            created = create_shadow_texture(bgfx::TextureFormat::D32F, true);
            if (!created) {
                KARMA_TRACE("render.bgfx", "shadow depth attachment create failed; using color-min fallback");
            }
        }
        if (!created) {
            created = create_shadow_texture(bgfx::TextureFormat::D32F, false);
        }
        if (!created) {
            created = create_shadow_texture(bgfx::TextureFormat::R32F, false);
        }
        if (!created) {
            *state.shadow_tex_size = 0u;
            *state.shadow_tex_is_rt = false;
            *state.shadow_tex_rt_uses_depth = false;
        }
    }

    if (!bgfx::isValid(*state.shadow_tex)) {
        return false;
    }
    if (render_target && !bgfx::isValid(*state.shadow_fb)) {
        return false;
    }
    return true;
}

bool EnsureBgfxPointShadowTexture(BgfxShadowState& state, uint16_t width, uint16_t height) {
    if (width == 0u || height == 0u) {
        return false;
    }
    const bool needs_recreate =
        !bgfx::isValid(*state.point_shadow_tex) ||
        *state.point_shadow_tex_width != width ||
        *state.point_shadow_tex_height != height;
    if (needs_recreate) {
        if (bgfx::isValid(*state.point_shadow_tex)) {
            bgfx::destroy(*state.point_shadow_tex);
            *state.point_shadow_tex = BGFX_INVALID_HANDLE;
        }
        const uint64_t flags =
            BGFX_SAMPLER_U_CLAMP |
            BGFX_SAMPLER_V_CLAMP |
            BGFX_SAMPLER_COMPARE_LEQUAL;
        *state.point_shadow_tex = bgfx::createTexture2D(
            width,
            height,
            false,
            1,
            bgfx::TextureFormat::D32F,
            flags,
            nullptr);
        if (!bgfx::isValid(*state.point_shadow_tex)) {
            *state.point_shadow_tex_width = 0u;
            *state.point_shadow_tex_height = 0u;
            return false;
        }
        *state.point_shadow_tex_width = width;
        *state.point_shadow_tex_height = height;
    }
    return bgfx::isValid(*state.point_shadow_tex);
}

bool RenderBgfxGpuShadowMap(BgfxShadowState& state,
                            const renderer::DirectionalLightData& light,
                            const detail::DirectionalShadowCascadeSet& cascades,
                            const std::vector<BgfxRenderableDraw>& renderables) {
    if (!cascades.ready ||
        cascades.map_size <= 0 ||
        cascades.atlas_size <= 0 ||
        !bgfx::isValid(state.shadow_depth_program)) {
        return false;
    }
    if (cascades.map_size > static_cast<int>(std::numeric_limits<uint16_t>::max()) ||
        cascades.atlas_size > static_cast<int>(std::numeric_limits<uint16_t>::max())) {
        return false;
    }
    const uint16_t cascade_size = static_cast<uint16_t>(cascades.map_size);
    const uint16_t atlas_size = static_cast<uint16_t>(cascades.atlas_size);
    if (!EnsureBgfxShadowTexture(state, atlas_size, true)) {
        return false;
    }

    const bgfx::Caps* caps = bgfx::getCaps();
    const detail::ShadowClipDepthTransform clip_depth =
        detail::ResolveShadowClipDepthTransform(caps && caps->homogeneousDepth);
    const float clip_y_sign =
        detail::ResolveShadowClipYSign(caps && caps->originBottomLeft);
    const detail::ResolvedDirectionalShadowSemantics shadow_semantics =
        detail::ResolveDirectionalShadowSemantics(light);
    const bool depth_attachment = *state.shadow_tex_rt_uses_depth;
    const int cascade_count = std::clamp(
        cascades.cascade_count,
        1,
        static_cast<int>(kBgfxDirectionalShadowCascadeCount));

    std::size_t submitted = 0u;
    std::size_t culled = 0u;
    constexpr bgfx::ViewId kBgfxShadowViewIdBase = 0;
    for (int cascade_idx = 0; cascade_idx < cascade_count; ++cascade_idx) {
        const detail::DirectionalShadowCascade& cascade =
            cascades.cascades[static_cast<std::size_t>(cascade_idx)];
        if (!cascade.ready) {
            continue;
        }
        const bgfx::ViewId view_id =
            static_cast<bgfx::ViewId>(kBgfxShadowViewIdBase + cascade_idx);
        const uint16_t view_x = static_cast<uint16_t>(
            std::clamp<int>(static_cast<int>(std::floor(cascade.atlas_offset.x *
                                                        static_cast<float>(atlas_size) + 0.5f)),
                            0,
                            std::max(static_cast<int>(atlas_size) - 1, 0)));
        const uint16_t view_y = static_cast<uint16_t>(
            std::clamp<int>(static_cast<int>(std::floor(cascade.atlas_offset.y *
                                                        static_cast<float>(atlas_size) + 0.5f)),
                            0,
                            std::max(static_cast<int>(atlas_size) - 1, 0)));
        bgfx::setViewFrameBuffer(view_id, *state.shadow_fb);
        bgfx::setViewRect(view_id, view_x, view_y, cascade_size, cascade_size);
        bgfx::setViewTransform(view_id, nullptr, nullptr);
        bgfx::setViewClear(
            view_id,
            depth_attachment ? BGFX_CLEAR_DEPTH : BGFX_CLEAR_COLOR,
            0xffffffff,
            1.0f,
            0);
        bgfx::touch(view_id);

        const float inv_depth_range =
            cascade.depth_range > 1e-6f ? (1.0f / cascade.depth_range) : 1.0f;
        const float shadow_params1[4] = {
            cascade.extent,
            cascade.center_x,
            cascade.center_y,
            cascade.min_depth,
        };
        const float shadow_params0[4] = {
            1.0f,
            cascades.strength,
            cascades.bias,
            cascades.map_size > 0
                ? (1.0f / static_cast<float>(cascades.map_size))
                : 0.0f,
        };
        const float shadow_params2[4] = {
            inv_depth_range,
            static_cast<float>(cascades.pcf_radius),
            clip_depth.scale,
            clip_depth.bias,
        };
        const float shadow_bias_params[4] = {
            shadow_semantics.receiver_bias_scale,
            shadow_semantics.normal_bias_scale,
            shadow_semantics.raster_depth_bias,
            shadow_semantics.raster_slope_bias,
        };
        const float shadow_axis_right[4] = {
            cascades.axis_right.x,
            cascades.axis_right.y,
            cascades.axis_right.z,
            0.0f,
        };
        const float shadow_axis_up[4] = {
            cascades.axis_up.x,
            cascades.axis_up.y,
            cascades.axis_up.z,
            clip_y_sign,
        };
        const float shadow_axis_forward[4] = {
            cascades.axis_forward.x,
            cascades.axis_forward.y,
            cascades.axis_forward.z,
            0.0f,
        };
        bgfx::setUniform(state.u_shadow_params0, shadow_params0);
        bgfx::setUniform(state.u_shadow_params1, shadow_params1);
        bgfx::setUniform(state.u_shadow_params2, shadow_params2);
        bgfx::setUniform(state.u_shadow_bias_params, shadow_bias_params);
        bgfx::setUniform(state.u_shadow_axis_right, shadow_axis_right);
        bgfx::setUniform(state.u_shadow_axis_up, shadow_axis_up);
        bgfx::setUniform(state.u_shadow_axis_forward, shadow_axis_forward);

        for (const BgfxRenderableDraw& renderable : renderables) {
            const auto& item = *renderable.item;
            const auto& semantics = renderable.material->semantics;
            if (!item.casts_shadow || semantics.alpha_blend || !semantics.draw) {
                continue;
            }
            if (renderable.mesh->shadow_radius > 0.0f) {
                glm::vec3 world_center{0.0f, 0.0f, 0.0f};
                float world_radius = 0.0f;
                computeWorldShadowSphere(
                    item.transform,
                    renderable.mesh->shadow_center,
                    renderable.mesh->shadow_radius,
                    world_center,
                    world_radius);
                if (!detail::IntersectsDirectionalShadowCascade(
                        cascades,
                        cascade_idx,
                        world_center,
                        world_radius)) {
                    ++culled;
                    continue;
                }
            }

            bgfx::setVertexBuffer(0, renderable.mesh->vbh);
            bgfx::setIndexBuffer(renderable.mesh->ibh);
            bgfx::setTransform(&item.transform[0][0]);
            uint64_t state_bits = BGFX_STATE_MSAA;
            if (depth_attachment) {
                state_bits |= BGFX_STATE_WRITE_Z |
                    BGFX_STATE_DEPTH_TEST_LESS;
            } else {
                state_bits |= BGFX_STATE_WRITE_RGB |
                    BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_ONE) |
                    BGFX_STATE_BLEND_EQUATION_MIN;
            }
            bgfx::setState(state_bits);
            bgfx::submit(view_id, state.shadow_depth_program);
            ++submitted;
        }
    }

    KARMA_TRACE_CHANGED(
        "render.bgfx",
        std::to_string(cascades.map_size) + ":" +
            std::to_string(cascade_count) + ":" +
            std::to_string(submitted) + ":" +
            std::to_string(culled) + ":" +
            (depth_attachment ? "depth" : "color_min"),
        "gpu shadow pass size={} cascades={} draws={} culled={} attachment={}",
        cascades.map_size,
        cascade_count,
        submitted,
        culled,
        depth_attachment ? "depth" : "color_min");
    return submitted > 0u;
}

void UpdateBgfxShadowTexture(BgfxShadowState& state, const detail::DirectionalShadowMap& map) {
    *state.shadow_tex_ready = false;
    if (!map.ready || map.size <= 0 || map.depth.empty()) {
        return;
    }
    const uint16_t size = static_cast<uint16_t>(map.size);
    if (!EnsureBgfxShadowTexture(state, size, false)) {
        return;
    }

    std::vector<float> upload = BuildShadowDepthUploadData(map);
    if (upload.empty()) {
        return;
    }
    const bgfx::Memory* mem =
        bgfx::copy(upload.data(), static_cast<uint32_t>(upload.size() * sizeof(float)));
    bgfx::updateTexture2D(*state.shadow_tex, 0, 0, 0, 0, size, size, mem);
    *state.shadow_tex_ready = true;
}

void UpdateBgfxPointShadowTexture(BgfxShadowState& state, const detail::PointShadowMap& map) {
    *state.point_shadow_tex_ready = false;
    if (!map.ready || map.atlas_width <= 0 || map.atlas_height <= 0 || map.depth.empty()) {
        return;
    }
    if (map.atlas_width > static_cast<int>(std::numeric_limits<uint16_t>::max()) ||
        map.atlas_height > static_cast<int>(std::numeric_limits<uint16_t>::max())) {
        return;
    }
    const uint16_t width = static_cast<uint16_t>(map.atlas_width);
    const uint16_t height = static_cast<uint16_t>(map.atlas_height);
    if (!EnsureBgfxPointShadowTexture(state, width, height)) {
        return;
    }
    std::vector<float> upload = BuildPointShadowDepthUploadData(map);
    if (upload.empty()) {
        return;
    }
    const bgfx::Memory* mem =
        bgfx::copy(upload.data(), static_cast<uint32_t>(upload.size() * sizeof(float)));
    bgfx::updateTexture2D(*state.point_shadow_tex, 0, 0, 0, 0, width, height, mem);
    *state.point_shadow_tex_ready = true;
}

} // namespace

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
    const std::unordered_map<renderer::MaterialId, BgfxMaterial>& materials) {
    BgfxDirectionalShadowUpdateResult result{};

    const int requested_update_every_frames = std::max(1, light.shadow.update_every_frames);
    if (*state.shadow_update_every_frames != requested_update_every_frames) {
        *state.shadow_update_every_frames = requested_update_every_frames;
        *state.shadow_frames_until_update = 0;
        *state.point_shadow_frames_until_update = 0;
        KARMA_TRACE_CHANGED(
            "render.bgfx",
            std::to_string(*state.shadow_update_every_frames),
            "shadow update cadence everyFrames={}",
            *state.shadow_update_every_frames);
    }

    result.current_camera_forward = resolveCameraForward(camera);
    const glm::vec3 current_light_direction =
        normalizeDirectionOrFallback(light.direction, *state.cached_shadow_light_direction);
    bool shadow_inputs_changed = false;
    if (world_layer) {
        if (!*state.shadow_cache_inputs_valid) {
            shadow_inputs_changed = true;
            result.point_shadow_structural_change = true;
        } else if (glm::length(camera.position - *state.cached_shadow_camera_position) >
                   state.directional_shadow_position_threshold) {
            shadow_inputs_changed = true;
        } else if (directionChangedBeyondThreshold(
                       result.current_camera_forward,
                       *state.cached_shadow_camera_forward,
                       state.directional_shadow_angle_threshold_deg)) {
            shadow_inputs_changed = true;
        } else if (directionChangedBeyondThreshold(
                       current_light_direction,
                       *state.cached_shadow_light_direction,
                       state.directional_shadow_angle_threshold_deg)) {
            shadow_inputs_changed = true;
        } else if (std::abs(aspect - *state.cached_shadow_camera_aspect) > 1e-3f) {
            shadow_inputs_changed = true;
        } else if (std::abs(camera.fov_y_degrees - *state.cached_shadow_camera_fov_y_degrees) > 1e-3f) {
            shadow_inputs_changed = true;
        } else if (std::abs(camera.near_clip - *state.cached_shadow_camera_near) > 1e-4f) {
            shadow_inputs_changed = true;
        } else if (std::abs(camera.far_clip - *state.cached_shadow_camera_far) > 1e-2f) {
            shadow_inputs_changed = true;
        } else if (current_shadow_items.size() != state.previous_shadow_items->size()) {
            shadow_inputs_changed = true;
            result.point_shadow_structural_change = true;
        } else {
            for (std::size_t i = 0; i < current_shadow_items.size(); ++i) {
                const renderer::DrawItem& curr = current_shadow_items[i];
                const renderer::DrawItem& prev = (*state.previous_shadow_items)[i];
                if (curr.mesh != prev.mesh ||
                    curr.material != prev.material ||
                    curr.layer != prev.layer ||
                    curr.casts_shadow != prev.casts_shadow) {
                    shadow_inputs_changed = true;
                    result.point_shadow_structural_change = true;
                    break;
                }
                if (matrixChangedBeyondEpsilon(curr.transform, prev.transform)) {
                    shadow_inputs_changed = true;
                    if (!curr.casts_shadow) {
                        continue;
                    }
                    auto mesh_it = meshes.find(curr.mesh);
                    auto mat_it = materials.find(curr.material);
                    if (mesh_it == meshes.end() ||
                        mat_it == materials.end() ||
                        mat_it->second.semantics.alpha_blend ||
                        mesh_it->second.shadow_radius <= 0.0f) {
                        result.point_shadow_structural_change = true;
                        continue;
                    }
                    glm::vec3 world_center{0.0f, 0.0f, 0.0f};
                    float world_radius = 0.0f;
                    computeWorldShadowSphere(
                        curr.transform,
                        mesh_it->second.shadow_center,
                        mesh_it->second.shadow_radius,
                        world_center,
                        world_radius);
                    result.moved_point_shadow_caster_bounds.emplace_back(world_center, world_radius);
                }
            }
        }
    }

    if (world_layer &&
        *state.shadow_cache_inputs_valid &&
        current_shadow_items.size() == state.previous_shadow_items->size() &&
        result.moved_point_shadow_caster_bounds.empty() &&
        !result.point_shadow_structural_change) {
        for (std::size_t i = 0; i < current_shadow_items.size(); ++i) {
            const renderer::DrawItem& curr = current_shadow_items[i];
            const renderer::DrawItem& prev = (*state.previous_shadow_items)[i];
            if (curr.mesh != prev.mesh ||
                curr.material != prev.material ||
                curr.layer != prev.layer ||
                curr.casts_shadow != prev.casts_shadow) {
                result.point_shadow_structural_change = true;
                break;
            }
            if (!matrixChangedBeyondEpsilon(curr.transform, prev.transform) ||
                !curr.casts_shadow) {
                continue;
            }
            auto mesh_it = meshes.find(curr.mesh);
            auto mat_it = materials.find(curr.material);
            if (mesh_it == meshes.end() ||
                mat_it == materials.end() ||
                mat_it->second.semantics.alpha_blend ||
                mesh_it->second.shadow_radius <= 0.0f) {
                result.point_shadow_structural_change = true;
                continue;
            }
            glm::vec3 world_center{0.0f, 0.0f, 0.0f};
            float world_radius = 0.0f;
            computeWorldShadowSphere(
                curr.transform,
                mesh_it->second.shadow_center,
                mesh_it->second.shadow_radius,
                world_center,
                world_radius);
            result.moved_point_shadow_caster_bounds.emplace_back(world_center, world_radius);
        }
    }

    if (world_layer &&
        (!*state.shadow_cache_inputs_valid ||
         current_shadow_items.size() != state.previous_shadow_items->size())) {
        result.point_shadow_structural_change = true;
    }

    const bool gpu_shadow_capable =
        bgfx::isValid(state.shadow_depth_program) && world_layer && !shadow_casters.empty();
    const bool use_gpu_shadow_path =
        gpu_shadow_requested && shadow_semantics.enabled && gpu_shadow_capable;
    if (gpu_shadow_requested && shadow_semantics.enabled && world_layer && !use_gpu_shadow_path) {
        const char* reason = "gpu_shadow_pass_not_implemented";
        if (!bgfx::isValid(state.shadow_depth_program)) {
            reason = "gpu_shadow_shader_unavailable";
        } else if (shadow_casters.empty()) {
            reason = "gpu_shadow_no_casters";
        }
        KARMA_TRACE_CHANGED(
            "render.bgfx",
            std::to_string(layer) + ":" + reason,
            "shadow execution mode requested={} active={} reason={}",
            renderer::DirectionalLightData::ShadowExecutionModeToken(light.shadow.execution_mode),
            renderer::DirectionalLightData::ShadowExecutionModeToken(
                renderer::DirectionalLightData::ShadowExecutionMode::CpuReference),
            reason);
    }

    if (!shadow_semantics.enabled) {
        *state.shadow_frames_until_update = 0;
        *state.cached_shadow_map = detail::DirectionalShadowMap{};
        *state.cached_shadow_cascades = detail::DirectionalShadowCascadeSet{};
        *state.shadow_tex_ready = false;
        *state.shadow_cache_inputs_valid = false;
        state.previous_shadow_items->clear();
    } else if (world_layer && shadow_casters.empty()) {
        *state.shadow_frames_until_update = 0;
        *state.cached_shadow_map = detail::DirectionalShadowMap{};
        *state.cached_shadow_cascades = detail::DirectionalShadowCascadeSet{};
        *state.shadow_tex_ready = false;
        *state.shadow_cache_inputs_valid = false;
        state.previous_shadow_items->clear();
    } else if (world_layer && !shadow_casters.empty()) {
        if (*state.shadow_frames_until_update <= 0 ||
            shadow_inputs_changed ||
            !state.cached_shadow_map->ready ||
            state.cached_shadow_map->size != shadow_semantics.map_size) {
            if (use_gpu_shadow_path) {
                *state.cached_shadow_cascades = detail::BuildDirectionalShadowCascades(
                    shadow_semantics,
                    light.direction,
                    shadow_casters,
                    shadow_view);
                *state.shadow_tex_ready = RenderBgfxGpuShadowMap(
                    state,
                    light,
                    *state.cached_shadow_cascades,
                    renderables);
                if (*state.shadow_tex_ready) {
                    *state.cached_shadow_map = detail::BuildDirectionalShadowProjection(
                        shadow_semantics,
                        light.direction,
                        shadow_casters,
                        &shadow_view);
                } else {
                    *state.cached_shadow_map = detail::BuildDirectionalShadowMap(
                        shadow_semantics,
                        light.direction,
                        shadow_casters,
                        &shadow_view);
                    UpdateBgfxShadowTexture(state, *state.cached_shadow_map);
                    *state.cached_shadow_cascades =
                        detail::BuildDirectionalShadowCascadeSetFromSingleMap(*state.cached_shadow_map);
                }
            } else {
                *state.cached_shadow_map = detail::BuildDirectionalShadowMap(
                    shadow_semantics,
                    light.direction,
                    shadow_casters,
                    &shadow_view);
                UpdateBgfxShadowTexture(state, *state.cached_shadow_map);
                *state.cached_shadow_cascades =
                    detail::BuildDirectionalShadowCascadeSetFromSingleMap(*state.cached_shadow_map);
            }
            *state.shadow_frames_until_update = *state.shadow_update_every_frames - 1;
        } else {
            --(*state.shadow_frames_until_update);
        }
        *state.previous_shadow_items = current_shadow_items;
        *state.cached_shadow_camera_position = camera.position;
        *state.cached_shadow_camera_forward = result.current_camera_forward;
        *state.cached_shadow_light_direction = current_light_direction;
        *state.cached_shadow_camera_aspect = aspect;
        *state.cached_shadow_camera_fov_y_degrees = camera.fov_y_degrees;
        *state.cached_shadow_camera_near = camera.near_clip;
        *state.cached_shadow_camera_far = camera.far_clip;
        *state.shadow_cache_inputs_valid = true;
    }

    return result;
}

BgfxPointShadowUpdateResult UpdateBgfxPointShadowCache(
    BgfxShadowState& state,
    const detail::ResolvedDirectionalShadowSemantics& shadow_semantics,
    bool world_layer,
    const std::vector<renderer::LightData>& lights,
    const std::vector<detail::DirectionalShadowCaster>& shadow_casters,
    const std::vector<int>& point_shadow_selected_indices,
    bool point_shadow_structural_change,
    const std::vector<glm::vec4>& moved_point_shadow_caster_bounds) {
    BgfxPointShadowUpdateResult result{};
    result.point_shadow_selected = static_cast<int>(point_shadow_selected_indices.size());
    result.point_shadow_budget = std::clamp(shadow_semantics.point_faces_per_frame_budget, 1, 24);

    if (!shadow_semantics.enabled ||
        shadow_semantics.point_max_shadow_lights <= 0 ||
        result.point_shadow_selected <= 0 ||
        (world_layer && shadow_casters.empty())) {
        ResetBgfxPointShadowCacheState(state);
        return result;
    }

    if (!world_layer) {
        return result;
    }

    const int active_face_count = result.point_shadow_selected * detail::kPointShadowFaceCount;
    const bool layout_compatible = detail::IsPointShadowMapLayoutCompatible(
        *state.cached_point_shadow_map,
        shadow_semantics.point_map_size,
        point_shadow_selected_indices);
    if (!layout_compatible) {
        *state.point_shadow_cache_initialized = false;
        state.point_shadow_face_dirty->fill(1u);
        result.point_shadow_full_refresh = true;
    }

    auto mark_point_shadow_slot_dirty = [&](int slot) {
        if (slot < 0 || slot >= static_cast<int>(kBgfxMaxPointShadowLights)) {
            return;
        }
        const int face_base = slot * detail::kPointShadowFaceCount;
        for (int face = 0; face < detail::kPointShadowFaceCount; ++face) {
            const int matrix_idx = face_base + face;
            if (matrix_idx < 0 ||
                matrix_idx >= static_cast<int>(kBgfxMaxPointShadowMatrices)) {
                continue;
            }
            (*state.point_shadow_face_dirty)[static_cast<std::size_t>(matrix_idx)] = 1u;
        }
    };

    for (int slot = 0; slot < result.point_shadow_selected; ++slot) {
        const int source_idx = point_shadow_selected_indices[static_cast<std::size_t>(slot)];
        if (source_idx < 0 || source_idx >= static_cast<int>(lights.size())) {
            continue;
        }
        const renderer::LightData& light = lights[static_cast<std::size_t>(source_idx)];
        const bool slot_identity_changed =
            (*state.point_shadow_slot_source_index)[static_cast<std::size_t>(slot)] != source_idx;
        const bool slot_position_changed =
            glm::length(light.position - (*state.point_shadow_slot_position)[static_cast<std::size_t>(slot)]) >
            state.point_shadow_position_threshold;
        const bool slot_range_changed =
            std::fabs(light.range - (*state.point_shadow_slot_range)[static_cast<std::size_t>(slot)]) >
            state.point_shadow_range_threshold;
        if (!*state.point_shadow_cache_initialized || slot_identity_changed) {
            mark_point_shadow_slot_dirty(slot);
            result.point_shadow_full_refresh = true;
        } else if (slot_position_changed || slot_range_changed) {
            mark_point_shadow_slot_dirty(slot);
        }
        (*state.point_shadow_slot_source_index)[static_cast<std::size_t>(slot)] = source_idx;
        (*state.point_shadow_slot_position)[static_cast<std::size_t>(slot)] = light.position;
        (*state.point_shadow_slot_range)[static_cast<std::size_t>(slot)] = light.range;
    }

    for (int slot = result.point_shadow_selected;
         slot < static_cast<int>(kBgfxMaxPointShadowLights);
         ++slot) {
        (*state.point_shadow_slot_source_index)[static_cast<std::size_t>(slot)] = -1;
        (*state.point_shadow_slot_position)[static_cast<std::size_t>(slot)] = glm::vec3(0.0f, 0.0f, 0.0f);
        (*state.point_shadow_slot_range)[static_cast<std::size_t>(slot)] = 0.0f;
    }

    if (point_shadow_structural_change) {
        result.point_shadow_full_refresh = true;
        for (int slot = 0; slot < result.point_shadow_selected; ++slot) {
            mark_point_shadow_slot_dirty(slot);
        }
    } else if (!moved_point_shadow_caster_bounds.empty()) {
        for (int slot = 0; slot < result.point_shadow_selected; ++slot) {
            const int source_idx =
                point_shadow_selected_indices[static_cast<std::size_t>(slot)];
            if (source_idx < 0 || source_idx >= static_cast<int>(lights.size())) {
                continue;
            }
            const renderer::LightData& point_light =
                lights[static_cast<std::size_t>(source_idx)];
            const float light_range = std::max(point_light.range, 0.0f);
            bool affects_slot = false;
            for (const glm::vec4& caster_sphere : moved_point_shadow_caster_bounds) {
                const glm::vec3 delta = glm::vec3(caster_sphere) - point_light.position;
                const float influence_radius = light_range + std::max(caster_sphere.w, 0.0f);
                if (influence_radius <= 0.0f) {
                    continue;
                }
                if (glm::dot(delta, delta) <= influence_radius * influence_radius) {
                    affects_slot = true;
                    break;
                }
            }
            if (affects_slot) {
                mark_point_shadow_slot_dirty(slot);
            }
        }
    }

    for (int idx = 0; idx < active_face_count; ++idx) {
        result.point_shadow_dirty_faces +=
            (*state.point_shadow_face_dirty)[static_cast<std::size_t>(idx)] != 0u ? 1 : 0;
    }

    std::array<uint8_t, kBgfxMaxPointShadowMatrices> point_shadow_faces_to_update{};
    point_shadow_faces_to_update.fill(0u);
    if (result.point_shadow_full_refresh) {
        for (int idx = 0; idx < active_face_count; ++idx) {
            if ((*state.point_shadow_face_dirty)[static_cast<std::size_t>(idx)] == 0u) {
                continue;
            }
            point_shadow_faces_to_update[static_cast<std::size_t>(idx)] = 1u;
            ++result.point_shadow_updated_faces;
        }
    } else {
        int visited = 0;
        const uint32_t cursor_mod =
            static_cast<uint32_t>(std::max(active_face_count, 1));
        while (result.point_shadow_updated_faces < result.point_shadow_budget &&
               visited < active_face_count) {
            const int matrix_idx =
                static_cast<int>(*state.point_shadow_face_cursor % cursor_mod);
            *state.point_shadow_face_cursor = (*state.point_shadow_face_cursor + 1u) % cursor_mod;
            ++visited;
            if ((*state.point_shadow_face_dirty)[static_cast<std::size_t>(matrix_idx)] == 0u) {
                continue;
            }
            point_shadow_faces_to_update[static_cast<std::size_t>(matrix_idx)] = 1u;
            ++result.point_shadow_updated_faces;
        }
    }

    if (!layout_compatible ||
        result.point_shadow_updated_faces > 0 ||
        !*state.point_shadow_cache_initialized) {
        std::vector<uint8_t> face_update_mask(
            static_cast<std::size_t>(active_face_count), 0u);
        for (int idx = 0; idx < active_face_count; ++idx) {
            face_update_mask[static_cast<std::size_t>(idx)] =
                point_shadow_faces_to_update[static_cast<std::size_t>(idx)];
        }
        *state.cached_point_shadow_map = detail::BuildPointShadowMap(
            shadow_semantics,
            lights,
            shadow_casters,
            layout_compatible ? state.cached_point_shadow_map : nullptr,
            &face_update_mask);
        UpdateBgfxPointShadowTexture(state, *state.cached_point_shadow_map);
        if (state.cached_point_shadow_map->ready && *state.point_shadow_tex_ready) {
            for (int idx = 0; idx < active_face_count; ++idx) {
                if (face_update_mask[static_cast<std::size_t>(idx)] != 0u) {
                    (*state.point_shadow_face_dirty)[static_cast<std::size_t>(idx)] = 0u;
                }
            }
            *state.point_shadow_cache_initialized = true;
        } else {
            *state.point_shadow_cache_initialized = false;
        }
    }

    return result;
}

} // namespace karma::renderer_backend

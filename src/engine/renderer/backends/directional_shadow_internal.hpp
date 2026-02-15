#pragma once

#include "karma/renderer/types.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace karma::renderer_backend::detail {

struct ResolvedDirectionalShadowSemantics {
    bool enabled = false;
    float strength = 0.65f;
    float bias = 0.0015f;
    float receiver_bias_scale = 0.08f;
    float normal_bias_scale = 0.35f;
    float raster_depth_bias = 0.0f;
    float raster_slope_bias = 0.0f;
    float extent = 24.0f;
    int map_size = 256;
    int pcf_radius = 1;
    int triangle_budget = 4096;
};

struct DirectionalShadowCaster {
    glm::mat4 transform{1.0f};
    const std::vector<glm::vec3>* positions = nullptr;
    const std::vector<uint32_t>* indices = nullptr;
    glm::vec3 sample_center{0.0f, 0.0f, 0.0f};
    bool casts_shadow = true;
};

struct DirectionalShadowMap {
    bool ready = false;
    int size = 0;
    int pcf_radius = 0;
    float bias = 0.0f;
    float strength = 0.0f;
    float extent = 1.0f;
    float center_x = 0.0f;
    float center_y = 0.0f;
    float min_depth = 0.0f;
    float depth_range = 1.0f;
    glm::vec3 axis_right{1.0f, 0.0f, 0.0f};
    glm::vec3 axis_up{0.0f, 1.0f, 0.0f};
    glm::vec3 axis_forward{0.0f, 0.0f, -1.0f};
    std::vector<float> depth{};
};

struct ShadowClipDepthTransform {
    float scale = 1.0f;
    float bias = 0.0f;
};

inline ShadowClipDepthTransform ResolveShadowClipDepthTransform(const bool homogeneous_depth) {
    ShadowClipDepthTransform transform{};
    if (homogeneous_depth) {
        transform.scale = 2.0f;
        transform.bias = -1.0f;
    }
    return transform;
}

inline float ResolveShadowClipYSign(const bool origin_bottom_left) {
    // GPU shadow depth pass outputs clip-space Y. When render-target origin is top-left
    // (for example Vulkan), negate Y so texture sampling space matches CPU reference projection.
    return origin_bottom_left ? 1.0f : -1.0f;
}

struct DirectionalShadowView {
    renderer::CameraData camera{};
    float aspect_ratio = 16.0f / 9.0f;
};

inline bool IsFiniteVec3(const glm::vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

inline float ClampFinite(float value, float fallback, float min_value, float max_value) {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, min_value, max_value);
}

inline int ClampRange(int value, int fallback, int min_value, int max_value) {
    if (value < min_value || value > max_value) {
        return fallback;
    }
    return value;
}

inline glm::vec3 TransformPoint(const glm::mat4& transform, const glm::vec3& point) {
    const glm::vec4 world = transform * glm::vec4(point, 1.0f);
    return glm::vec3(world.x, world.y, world.z);
}

inline bool BuildLightBasis(const glm::vec3& light_direction,
                            glm::vec3& out_right,
                            glm::vec3& out_up,
                            glm::vec3& out_forward) {
    out_forward = light_direction;
    if (!IsFiniteVec3(out_forward) || glm::length(out_forward) <= 1e-6f) {
        out_forward = glm::vec3(0.3f, -1.0f, -0.2f);
    }
    out_forward = glm::normalize(out_forward);

    glm::vec3 world_up{0.0f, 1.0f, 0.0f};
    if (std::fabs(glm::dot(out_forward, world_up)) >= 0.95f) {
        world_up = glm::vec3(1.0f, 0.0f, 0.0f);
    }
    out_right = glm::cross(world_up, out_forward);
    const float right_len = glm::length(out_right);
    if (right_len <= 1e-6f) {
        return false;
    }
    out_right /= right_len;
    out_up = glm::normalize(glm::cross(out_forward, out_right));
    return IsFiniteVec3(out_right) && IsFiniteVec3(out_up) && IsFiniteVec3(out_forward);
}

inline bool BuildShadowViewBounds(const DirectionalShadowView& view,
                                  const float max_distance,
                                  const glm::vec3& axis_right,
                                  const glm::vec3& axis_up,
                                  const glm::vec3& axis_forward,
                                  float& out_min_x,
                                  float& out_max_x,
                                  float& out_min_y,
                                  float& out_max_y,
                                  float& out_min_z,
                                  float& out_max_z,
                                  float& out_center_x,
                                  float& out_center_y) {
    const renderer::CameraData& camera = view.camera;
    if (!IsFiniteVec3(camera.position) || !IsFiniteVec3(camera.target)) {
        return false;
    }
    glm::vec3 forward = camera.target - camera.position;
    const float forward_len = glm::length(forward);
    if (!std::isfinite(forward_len) || forward_len <= 1e-6f) {
        return false;
    }
    forward /= forward_len;
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    if (std::fabs(glm::dot(forward, up)) >= 0.95f) {
        up = glm::vec3(0.0f, 0.0f, 1.0f);
    }
    glm::vec3 right = glm::cross(forward, up);
    const float right_len = glm::length(right);
    if (!std::isfinite(right_len) || right_len <= 1e-6f) {
        return false;
    }
    right /= right_len;
    up = glm::normalize(glm::cross(right, forward));
    if (!IsFiniteVec3(up)) {
        return false;
    }

    const float aspect = ClampFinite(view.aspect_ratio, 16.0f / 9.0f, 0.25f, 8.0f);
    const float fov_y_degrees = ClampFinite(camera.fov_y_degrees, 60.0f, 20.0f, 120.0f);
    const float tan_half_fov = std::tan(glm::radians(fov_y_degrees) * 0.5f);
    if (!std::isfinite(tan_half_fov) || tan_half_fov <= 0.0f) {
        return false;
    }

    const float near_d = ClampFinite(camera.near_clip, 0.1f, 0.05f, 10.0f);
    float far_d = std::max(near_d + 1.0f, max_distance);
    if (std::isfinite(camera.far_clip) && camera.far_clip > (near_d + 0.1f)) {
        far_d = std::min(far_d, camera.far_clip);
    }
    if (far_d <= near_d + 0.01f) {
        far_d = near_d + 1.0f;
    }

    const float near_h = tan_half_fov * near_d;
    const float near_w = near_h * aspect;
    const float far_h = tan_half_fov * far_d;
    const float far_w = far_h * aspect;

    const glm::vec3 near_center = camera.position + (forward * near_d);
    const glm::vec3 far_center = camera.position + (forward * far_d);
    const std::array<glm::vec3, 8> corners{
        near_center + (up * near_h) - (right * near_w),
        near_center + (up * near_h) + (right * near_w),
        near_center - (up * near_h) - (right * near_w),
        near_center - (up * near_h) + (right * near_w),
        far_center + (up * far_h) - (right * far_w),
        far_center + (up * far_h) + (right * far_w),
        far_center - (up * far_h) - (right * far_w),
        far_center - (up * far_h) + (right * far_w),
    };

    out_min_x = std::numeric_limits<float>::max();
    out_max_x = std::numeric_limits<float>::lowest();
    out_min_y = std::numeric_limits<float>::max();
    out_max_y = std::numeric_limits<float>::lowest();
    out_min_z = std::numeric_limits<float>::max();
    out_max_z = std::numeric_limits<float>::lowest();
    for (const glm::vec3& corner : corners) {
        const float lx = glm::dot(corner, axis_right);
        const float ly = glm::dot(corner, axis_up);
        const float lz = glm::dot(corner, axis_forward);
        if (!std::isfinite(lx) || !std::isfinite(ly) || !std::isfinite(lz)) {
            return false;
        }
        out_min_x = std::min(out_min_x, lx);
        out_max_x = std::max(out_max_x, lx);
        out_min_y = std::min(out_min_y, ly);
        out_max_y = std::max(out_max_y, ly);
        out_min_z = std::min(out_min_z, lz);
        out_max_z = std::max(out_max_z, lz);
    }

    out_center_x = 0.5f * (out_min_x + out_max_x);
    out_center_y = 0.5f * (out_min_y + out_max_y);
    return std::isfinite(out_center_x) && std::isfinite(out_center_y);
}

inline ResolvedDirectionalShadowSemantics ResolveDirectionalShadowSemantics(const renderer::DirectionalLightData& light) {
    ResolvedDirectionalShadowSemantics semantics{};
    semantics.enabled = light.shadow.enabled;
    semantics.strength = ClampFinite(light.shadow.strength, 0.65f, 0.0f, 1.0f);
    semantics.bias = ClampFinite(light.shadow.bias, 0.0008f, 0.0f, 0.02f);
    semantics.receiver_bias_scale = ClampFinite(light.shadow.receiver_bias_scale, 0.08f, 0.0f, 4.0f);
    semantics.normal_bias_scale = ClampFinite(light.shadow.normal_bias_scale, 0.35f, 0.0f, 8.0f);
    semantics.raster_depth_bias = ClampFinite(light.shadow.raster_depth_bias, 0.0f, 0.0f, 0.02f);
    semantics.raster_slope_bias = ClampFinite(light.shadow.raster_slope_bias, 0.0f, 0.0f, 8.0f);
    semantics.extent = ClampFinite(light.shadow.extent, 24.0f, 2.0f, 512.0f);
    semantics.map_size = ClampRange(light.shadow.map_size, 256, 64, 2048);
    semantics.pcf_radius = ClampRange(light.shadow.pcf_radius, 1, 0, 4);
    semantics.triangle_budget = ClampRange(light.shadow.triangle_budget, 4096, 1, 65536);
    return semantics;
}

inline bool ValidateResolvedDirectionalShadowSemantics(const ResolvedDirectionalShadowSemantics& semantics) {
    if (!std::isfinite(semantics.strength) ||
        !std::isfinite(semantics.bias) ||
        !std::isfinite(semantics.receiver_bias_scale) ||
        !std::isfinite(semantics.normal_bias_scale) ||
        !std::isfinite(semantics.raster_depth_bias) ||
        !std::isfinite(semantics.raster_slope_bias) ||
        !std::isfinite(semantics.extent)) {
        return false;
    }
    if (semantics.strength < 0.0f || semantics.strength > 1.0f) {
        return false;
    }
    if (semantics.bias < 0.0f || semantics.bias > 0.02f) {
        return false;
    }
    if (semantics.receiver_bias_scale < 0.0f || semantics.receiver_bias_scale > 4.0f) {
        return false;
    }
    if (semantics.normal_bias_scale < 0.0f || semantics.normal_bias_scale > 8.0f) {
        return false;
    }
    if (semantics.raster_depth_bias < 0.0f || semantics.raster_depth_bias > 0.02f) {
        return false;
    }
    if (semantics.raster_slope_bias < 0.0f || semantics.raster_slope_bias > 8.0f) {
        return false;
    }
    if (semantics.extent < 2.0f || semantics.extent > 512.0f) {
        return false;
    }
    if (semantics.map_size < 64 || semantics.map_size > 2048) {
        return false;
    }
    if (semantics.pcf_radius < 0 || semantics.pcf_radius > 4) {
        return false;
    }
    if (semantics.triangle_budget < 1 || semantics.triangle_budget > 65536) {
        return false;
    }
    return true;
}

inline void ProjectShadowPoint(const DirectionalShadowMap& map,
                               const glm::vec3& world_point,
                               float& out_u,
                               float& out_v,
                               float& out_depth) {
    const float lx = glm::dot(world_point, map.axis_right);
    const float ly = glm::dot(world_point, map.axis_up);
    const float lz = glm::dot(world_point, map.axis_forward);

    out_u = 0.5f + ((lx - map.center_x) / (2.0f * map.extent));
    out_v = 0.5f + ((ly - map.center_y) / (2.0f * map.extent));
    out_depth = (lz - map.min_depth) / map.depth_range;
}

inline float EdgeFunction(float ax, float ay, float bx, float by, float px, float py) {
    return ((px - ax) * (by - ay)) - ((py - ay) * (bx - ax));
}

inline DirectionalShadowMap BuildDirectionalShadowProjection(const ResolvedDirectionalShadowSemantics& semantics,
                                                             const glm::vec3& light_direction,
                                                             const std::vector<DirectionalShadowCaster>& casters,
                                                             const DirectionalShadowView* view = nullptr) {
    DirectionalShadowMap map{};
    if (!semantics.enabled || !ValidateResolvedDirectionalShadowSemantics(semantics) || casters.empty()) {
        return map;
    }

    glm::vec3 axis_right{};
    glm::vec3 axis_up{};
    glm::vec3 axis_forward{};
    if (!BuildLightBasis(light_direction, axis_right, axis_up, axis_forward)) {
        return map;
    }

    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    float min_z = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float max_y = std::numeric_limits<float>::lowest();
    float max_z = std::numeric_limits<float>::lowest();
    std::size_t bounds_points = 0;

    for (const DirectionalShadowCaster& caster : casters) {
        if (!caster.positions || caster.positions->empty()) {
            continue;
        }
        const glm::vec3 center_world = TransformPoint(caster.transform, caster.sample_center);
        if (!IsFiniteVec3(center_world)) {
            continue;
        }
        const float lx = glm::dot(center_world, axis_right);
        const float ly = glm::dot(center_world, axis_up);
        const float lz = glm::dot(center_world, axis_forward);
        if (!std::isfinite(lx) || !std::isfinite(ly) || !std::isfinite(lz)) {
            continue;
        }
        min_x = std::min(min_x, lx);
        min_y = std::min(min_y, ly);
        min_z = std::min(min_z, lz);
        max_x = std::max(max_x, lx);
        max_y = std::max(max_y, ly);
        max_z = std::max(max_z, lz);
        ++bounds_points;
    }

    if (bounds_points == 0) {
        return map;
    }

    float center_x = 0.5f * (min_x + max_x);
    float center_y = 0.5f * (min_y + max_y);
    bool view_bounds_valid = false;
    float view_min_x = 0.0f;
    float view_max_x = 0.0f;
    float view_min_y = 0.0f;
    float view_max_y = 0.0f;
    float view_min_z = 0.0f;
    float view_max_z = 0.0f;
    float view_center_x = 0.0f;
    float view_center_y = 0.0f;
    if (view) {
        view_bounds_valid = BuildShadowViewBounds(
            *view,
            semantics.extent,
            axis_right,
            axis_up,
            axis_forward,
            view_min_x,
            view_max_x,
            view_min_y,
            view_max_y,
            view_min_z,
            view_max_z,
            view_center_x,
            view_center_y);
    }
    if (view_bounds_valid) {
        min_x = view_min_x;
        max_x = view_max_x;
        min_y = view_min_y;
        max_y = view_max_y;
        center_x = view_center_x;
        center_y = view_center_y;
    }

    float resolved_extent = semantics.extent;
    if (view_bounds_valid) {
        const float view_half_x = 0.5f * std::fabs(view_max_x - view_min_x);
        const float view_half_y = 0.5f * std::fabs(view_max_y - view_min_y);
        const float required_half_extent = std::max(view_half_x, view_half_y);
        if (std::isfinite(required_half_extent) && required_half_extent > 0.0f) {
            resolved_extent = std::max(resolved_extent, required_half_extent * 1.05f);
        }
    }
    resolved_extent = std::clamp(resolved_extent, 2.0f, 1024.0f);
    const float texel_world = (2.0f * resolved_extent) / static_cast<float>(semantics.map_size);
    if (std::isfinite(texel_world) && texel_world > 1e-6f) {
        center_x = std::floor((center_x / texel_world) + 0.5f) * texel_world;
        center_y = std::floor((center_y / texel_world) + 0.5f) * texel_world;
    }

    float depth_min = min_z;
    float depth_max = max_z;
    if (view_bounds_valid) {
        const float z_margin_xy = std::max(2.0f, resolved_extent * 0.2f);
        float caster_depth_min = std::numeric_limits<float>::max();
        float caster_depth_max = std::numeric_limits<float>::lowest();
        std::size_t caster_depth_samples = 0u;
        for (const DirectionalShadowCaster& caster : casters) {
            if (!caster.positions || caster.positions->empty()) {
                continue;
            }
            const glm::vec3 center_world = TransformPoint(caster.transform, caster.sample_center);
            if (!IsFiniteVec3(center_world)) {
                continue;
            }
            const float lx = glm::dot(center_world, axis_right);
            const float ly = glm::dot(center_world, axis_up);
            const float lz = glm::dot(center_world, axis_forward);
            if (!std::isfinite(lx) || !std::isfinite(ly) || !std::isfinite(lz)) {
                continue;
            }
            if (std::fabs(lx - center_x) > (resolved_extent + z_margin_xy) ||
                std::fabs(ly - center_y) > (resolved_extent + z_margin_xy)) {
                continue;
            }
            caster_depth_min = std::min(caster_depth_min, lz);
            caster_depth_max = std::max(caster_depth_max, lz);
            ++caster_depth_samples;
        }
        depth_min = view_min_z;
        depth_max = view_max_z;
        if (caster_depth_samples > 0u) {
            depth_min = std::min(depth_min, caster_depth_min);
            depth_max = std::max(depth_max, caster_depth_max);
        }
    }

    const float depth_padding = std::max(1.0f, resolved_extent * 0.1f);
    const float receiver_depth_extension = std::max(2.0f, resolved_extent * 2.0f);
    const float resolved_min_depth = depth_min - depth_padding;
    const float resolved_max_depth = depth_max + depth_padding + receiver_depth_extension;
    const float resolved_depth_range = std::max(resolved_max_depth - resolved_min_depth, 0.01f);

    map.ready = true;
    map.size = semantics.map_size;
    map.pcf_radius = semantics.pcf_radius;
    map.bias = semantics.bias;
    map.strength = semantics.strength;
    map.extent = resolved_extent;
    map.center_x = center_x;
    map.center_y = center_y;
    map.min_depth = resolved_min_depth;
    map.depth_range = resolved_depth_range;
    map.axis_right = axis_right;
    map.axis_up = axis_up;
    map.axis_forward = axis_forward;
    return map;
}

inline DirectionalShadowMap BuildDirectionalShadowMap(const ResolvedDirectionalShadowSemantics& semantics,
                                                      const glm::vec3& light_direction,
                                                      const std::vector<DirectionalShadowCaster>& casters,
                                                      const DirectionalShadowView* view = nullptr) {
    DirectionalShadowMap map =
        BuildDirectionalShadowProjection(semantics, light_direction, casters, view);
    if (!map.ready) {
        return map;
    }
    map.depth.assign(static_cast<std::size_t>(map.size) * static_cast<std::size_t>(map.size),
                     std::numeric_limits<float>::infinity());

    const float raster_scale = static_cast<float>(map.size - 1);
    int triangles_rasterized = 0;

    for (const DirectionalShadowCaster& caster : casters) {
        if (!caster.casts_shadow || !caster.positions || !caster.indices) {
            continue;
        }
        const auto& positions = *caster.positions;
        const auto& indices = *caster.indices;
        if (positions.empty() || indices.size() < 3) {
            continue;
        }

        for (std::size_t i = 0; (i + 2) < indices.size(); i += 3) {
            if (triangles_rasterized >= semantics.triangle_budget) {
                break;
            }

            const uint32_t i0 = indices[i + 0];
            const uint32_t i1 = indices[i + 1];
            const uint32_t i2 = indices[i + 2];
            if (i0 >= positions.size() || i1 >= positions.size() || i2 >= positions.size()) {
                continue;
            }

            const glm::vec3 p0 = TransformPoint(caster.transform, positions[i0]);
            const glm::vec3 p1 = TransformPoint(caster.transform, positions[i1]);
            const glm::vec3 p2 = TransformPoint(caster.transform, positions[i2]);
            if (!IsFiniteVec3(p0) || !IsFiniteVec3(p1) || !IsFiniteVec3(p2)) {
                continue;
            }

            float u0 = 0.0f;
            float v0 = 0.0f;
            float d0 = 0.0f;
            float u1 = 0.0f;
            float v1 = 0.0f;
            float d1 = 0.0f;
            float u2 = 0.0f;
            float v2 = 0.0f;
            float d2 = 0.0f;
            ProjectShadowPoint(map, p0, u0, v0, d0);
            ProjectShadowPoint(map, p1, u1, v1, d1);
            ProjectShadowPoint(map, p2, u2, v2, d2);

            const float tx0 = u0 * raster_scale;
            const float ty0 = v0 * raster_scale;
            const float tx1 = u1 * raster_scale;
            const float ty1 = v1 * raster_scale;
            const float tx2 = u2 * raster_scale;
            const float ty2 = v2 * raster_scale;
            if (!std::isfinite(tx0) || !std::isfinite(ty0) || !std::isfinite(tx1) || !std::isfinite(ty1) ||
                !std::isfinite(tx2) || !std::isfinite(ty2)) {
                continue;
            }

            const float tri_min_x = std::min(tx0, std::min(tx1, tx2));
            const float tri_max_x = std::max(tx0, std::max(tx1, tx2));
            const float tri_min_y = std::min(ty0, std::min(ty1, ty2));
            const float tri_max_y = std::max(ty0, std::max(ty1, ty2));
            if (tri_max_x < 0.0f || tri_max_y < 0.0f ||
                tri_min_x > raster_scale || tri_min_y > raster_scale) {
                continue;
            }

            const int min_tx = std::max(0, static_cast<int>(std::floor(tri_min_x)));
            const int max_tx = std::min(map.size - 1, static_cast<int>(std::ceil(tri_max_x)));
            const int min_ty = std::max(0, static_cast<int>(std::floor(tri_min_y)));
            const int max_ty = std::min(map.size - 1, static_cast<int>(std::ceil(tri_max_y)));
            if (min_tx > max_tx || min_ty > max_ty) {
                continue;
            }

            const float area = EdgeFunction(tx0, ty0, tx1, ty1, tx2, ty2);
            if (std::fabs(area) <= 1e-6f) {
                continue;
            }

            for (int y = min_ty; y <= max_ty; ++y) {
                for (int x = min_tx; x <= max_tx; ++x) {
                    const float px = static_cast<float>(x) + 0.5f;
                    const float py = static_cast<float>(y) + 0.5f;
                    const float w0 = EdgeFunction(tx1, ty1, tx2, ty2, px, py) / area;
                    const float w1 = EdgeFunction(tx2, ty2, tx0, ty0, px, py) / area;
                    const float w2 = EdgeFunction(tx0, ty0, tx1, ty1, px, py) / area;
                    if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
                        continue;
                    }

                    const float depth = (w0 * d0) + (w1 * d1) + (w2 * d2);
                    if (!std::isfinite(depth)) {
                        continue;
                    }
                    const std::size_t idx =
                        static_cast<std::size_t>(y) * static_cast<std::size_t>(map.size) + static_cast<std::size_t>(x);
                    map.depth[idx] = std::min(map.depth[idx], depth);
                }
            }

            ++triangles_rasterized;
        }
        if (triangles_rasterized >= semantics.triangle_budget) {
            break;
        }
    }

    return map;
}

inline float SampleDirectionalShadowVisibility(const DirectionalShadowMap& map, const glm::vec3& world_point) {
    if (!map.ready || map.size <= 1 || map.depth.empty()) {
        return 1.0f;
    }

    float u = 0.0f;
    float v = 0.0f;
    float depth = 0.0f;
    ProjectShadowPoint(map, world_point, u, v, depth);

    if (!std::isfinite(u) || !std::isfinite(v) || !std::isfinite(depth)) {
        return 1.0f;
    }
    if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f || depth < 0.0f || depth > 1.0f) {
        return 1.0f;
    }

    const float texel = static_cast<float>(map.size - 1);
    const float inv_texel = 1.0f / texel;
    const int radius = std::max(0, map.pcf_radius);
    const float effective_bias =
        std::clamp(map.bias + (static_cast<float>(radius) * (0.10f * inv_texel)), 0.0f, 0.02f);
    float lit_sum = 0.0f;
    float weight_sum = 0.0f;

    const auto lookup_lit = [&map, effective_bias, depth](int x, int y) -> float {
        const std::size_t idx =
            static_cast<std::size_t>(y) * static_cast<std::size_t>(map.size) + static_cast<std::size_t>(x);
        const float map_depth = map.depth[idx];
        const bool lit = !std::isfinite(map_depth) || ((depth - effective_bias) <= map_depth);
        return lit ? 1.0f : 0.0f;
    };

    const auto sample_lit_bilinear = [&](float sample_u, float sample_v) -> float {
        sample_u = std::clamp(sample_u, 0.0f, 1.0f);
        sample_v = std::clamp(sample_v, 0.0f, 1.0f);
        const float fx = sample_u * texel;
        const float fy = sample_v * texel;
        const int x0 = std::clamp(static_cast<int>(std::floor(fx)), 0, map.size - 1);
        const int y0 = std::clamp(static_cast<int>(std::floor(fy)), 0, map.size - 1);
        const int x1 = std::min(x0 + 1, map.size - 1);
        const int y1 = std::min(y0 + 1, map.size - 1);
        const float tx = std::clamp(fx - static_cast<float>(x0), 0.0f, 1.0f);
        const float ty = std::clamp(fy - static_cast<float>(y0), 0.0f, 1.0f);

        const float l00 = lookup_lit(x0, y0);
        const float l10 = lookup_lit(x1, y0);
        const float l01 = lookup_lit(x0, y1);
        const float l11 = lookup_lit(x1, y1);
        const float lx0 = (l00 * (1.0f - tx)) + (l10 * tx);
        const float lx1 = (l01 * (1.0f - tx)) + (l11 * tx);
        return (lx0 * (1.0f - ty)) + (lx1 * ty);
    };

    for (int oy = -radius; oy <= radius; ++oy) {
        for (int ox = -radius; ox <= radius; ++ox) {
            const float sample_u = u + (static_cast<float>(ox) * inv_texel);
            const float sample_v = v + (static_cast<float>(oy) * inv_texel);
            const float weight = 1.0f / (1.0f + static_cast<float>((ox * ox) + (oy * oy)));
            lit_sum += sample_lit_bilinear(sample_u, sample_v) * weight;
            weight_sum += weight;
        }
    }

    if (weight_sum <= 0.0f) {
        return 1.0f;
    }
    return lit_sum / weight_sum;
}

inline float ComputeDirectionalShadowVisibilityForReceiver(const DirectionalShadowMap& map,
                                                           const glm::mat4& transform,
                                                           const std::vector<glm::vec3>* positions,
                                                           const std::vector<uint32_t>* indices,
                                                           const glm::vec3& sample_center) {
    if (!map.ready) {
        return 1.0f;
    }

    std::vector<glm::vec3> local_samples{};
    local_samples.reserve(9u);
    local_samples.push_back(sample_center);

    if (positions && !positions->empty()) {
        glm::vec3 min_pos = positions->front();
        glm::vec3 max_pos = positions->front();
        bool valid_bounds = IsFiniteVec3(min_pos);
        for (const glm::vec3& pos : *positions) {
            if (!IsFiniteVec3(pos)) {
                continue;
            }
            if (!valid_bounds) {
                min_pos = pos;
                max_pos = pos;
                valid_bounds = true;
            } else {
                min_pos = glm::min(min_pos, pos);
                max_pos = glm::max(max_pos, pos);
            }
        }

        if (valid_bounds) {
            const float center_y = 0.5f * (min_pos.y + max_pos.y);
            local_samples.push_back(glm::vec3(min_pos.x, center_y, min_pos.z));
            local_samples.push_back(glm::vec3(min_pos.x, center_y, max_pos.z));
            local_samples.push_back(glm::vec3(max_pos.x, center_y, min_pos.z));
            local_samples.push_back(glm::vec3(max_pos.x, center_y, max_pos.z));
        }
    }

    if (positions && indices && !positions->empty() && indices->size() >= 3u) {
        const std::size_t tri_count = indices->size() / 3u;
        const std::size_t tri_budget = 4u;
        const std::size_t tri_step = std::max<std::size_t>(1u, tri_count / tri_budget);
        for (std::size_t tri = 0; tri < tri_count && local_samples.size() < 9u; tri += tri_step) {
            const std::size_t base = tri * 3u;
            const uint32_t i0 = (*indices)[base + 0u];
            const uint32_t i1 = (*indices)[base + 1u];
            const uint32_t i2 = (*indices)[base + 2u];
            if (i0 >= positions->size() || i1 >= positions->size() || i2 >= positions->size()) {
                continue;
            }
            const glm::vec3 p0 = (*positions)[i0];
            const glm::vec3 p1 = (*positions)[i1];
            const glm::vec3 p2 = (*positions)[i2];
            if (!IsFiniteVec3(p0) || !IsFiniteVec3(p1) || !IsFiniteVec3(p2)) {
                continue;
            }
            local_samples.push_back((p0 + p1 + p2) / 3.0f);
        }
    }

    float min_visibility = 1.0f;
    float visibility_sum = 0.0f;
    int sample_count = 0;
    for (const glm::vec3& local : local_samples) {
        const glm::vec3 world = TransformPoint(transform, local);
        if (!IsFiniteVec3(world)) {
            continue;
        }
        const float visibility = SampleDirectionalShadowVisibility(map, world);
        if (!std::isfinite(visibility)) {
            continue;
        }
        min_visibility = std::min(min_visibility, visibility);
        visibility_sum += visibility;
        ++sample_count;
    }

    if (sample_count <= 0) {
        return 1.0f;
    }
    const float avg_visibility = visibility_sum / static_cast<float>(sample_count);
    return std::clamp((0.6f * min_visibility) + (0.4f * avg_visibility), 0.0f, 1.0f);
}

inline float ComputeDirectionalShadowFactor(const DirectionalShadowMap& map, float visibility) {
    const float safe_visibility = std::clamp(visibility, 0.0f, 1.0f);
    const float safe_strength = std::clamp(map.strength, 0.0f, 1.0f);
    return std::clamp(1.0f - (safe_strength * (1.0f - safe_visibility)), 0.0f, 1.0f);
}

} // namespace karma::renderer_backend::detail

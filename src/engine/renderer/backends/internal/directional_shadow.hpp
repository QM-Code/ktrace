#pragma once

#include "karma/renderer/types.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

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
    int point_map_size = 1024;
    int point_max_shadow_lights = 2;
    int point_faces_per_frame_budget = 2;
    float point_constant_bias = 0.0012f;
    float point_slope_bias_scale = 2.0f;
    float point_normal_bias_scale = 1.5f;
    float point_receiver_bias_scale = 0.35f;
    float local_light_distance_damping = 0.08f;
    float local_light_range_falloff_exponent = 1.1f;
    bool ao_affects_local_lights = false;
    float local_light_directional_shadow_lift_strength = 0.85f;
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

constexpr int kDirectionalShadowCascadeCount = 4;

struct DirectionalShadowCascade {
    bool ready = false;
    float extent = 1.0f;
    float center_x = 0.0f;
    float center_y = 0.0f;
    float min_depth = 0.0f;
    float depth_range = 1.0f;
    float world_texel = 0.0f;
    glm::vec2 atlas_offset{0.0f, 0.0f};
    float atlas_scale = 1.0f;
    glm::mat4 shadow_uv_proj{1.0f};
};

struct DirectionalShadowCascadeSet {
    bool ready = false;
    bool single_map_fallback = true;
    int map_size = 0;
    int atlas_size = 0;
    int cascade_count = 1;
    int pcf_radius = 0;
    float bias = 0.0f;
    float strength = 0.0f;
    float split_lambda = 0.7f;
    float transition_fraction = 0.08f;
    glm::vec3 axis_right{1.0f, 0.0f, 0.0f};
    glm::vec3 axis_up{0.0f, 1.0f, 0.0f};
    glm::vec3 axis_forward{0.0f, 0.0f, -1.0f};
    std::array<DirectionalShadowCascade, kDirectionalShadowCascadeCount> cascades{};
    std::array<float, kDirectionalShadowCascadeCount> splits{};
};

constexpr int kPointShadowFaceCount = 6;

struct PointShadowMap {
    bool ready = false;
    int size = 0;
    int atlas_width = 0;
    int atlas_height = 0;
    int light_count = 0;
    float texel_size = 0.0f;
    std::vector<glm::mat4> uv_proj{};
    std::vector<int> source_light_indices{};
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
    semantics.point_map_size = ClampRange(light.shadow.point_map_size, 1024, 128, 2048);
    semantics.point_max_shadow_lights = ClampRange(light.shadow.point_max_shadow_lights, 2, 0, 4);
    semantics.point_faces_per_frame_budget = ClampRange(light.shadow.point_faces_per_frame_budget, 2, 1, 24);
    semantics.point_constant_bias = ClampFinite(light.shadow.point_constant_bias, 0.0012f, 0.0f, 0.02f);
    semantics.point_slope_bias_scale = ClampFinite(light.shadow.point_slope_bias_scale, 2.0f, 0.0f, 8.0f);
    semantics.point_normal_bias_scale = ClampFinite(light.shadow.point_normal_bias_scale, 1.5f, 0.0f, 8.0f);
    semantics.point_receiver_bias_scale =
        ClampFinite(light.shadow.point_receiver_bias_scale, 0.35f, 0.0f, 4.0f);
    semantics.local_light_distance_damping =
        ClampFinite(light.shadow.local_light_distance_damping, 0.08f, 0.0f, 2.0f);
    semantics.local_light_range_falloff_exponent =
        ClampFinite(light.shadow.local_light_range_falloff_exponent, 1.1f, 0.1f, 8.0f);
    semantics.ao_affects_local_lights = light.shadow.ao_affects_local_lights;
    semantics.local_light_directional_shadow_lift_strength =
        ClampFinite(light.shadow.local_light_directional_shadow_lift_strength, 0.85f, 0.0f, 4.0f);
    return semantics;
}

inline bool ValidateResolvedDirectionalShadowSemantics(const ResolvedDirectionalShadowSemantics& semantics) {
    if (!std::isfinite(semantics.strength) ||
        !std::isfinite(semantics.bias) ||
        !std::isfinite(semantics.receiver_bias_scale) ||
        !std::isfinite(semantics.normal_bias_scale) ||
        !std::isfinite(semantics.raster_depth_bias) ||
        !std::isfinite(semantics.raster_slope_bias) ||
        !std::isfinite(semantics.extent) ||
        !std::isfinite(semantics.point_constant_bias) ||
        !std::isfinite(semantics.point_slope_bias_scale) ||
        !std::isfinite(semantics.point_normal_bias_scale) ||
        !std::isfinite(semantics.point_receiver_bias_scale) ||
        !std::isfinite(semantics.local_light_distance_damping) ||
        !std::isfinite(semantics.local_light_range_falloff_exponent) ||
        !std::isfinite(semantics.local_light_directional_shadow_lift_strength)) {
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
    if (semantics.point_map_size < 128 || semantics.point_map_size > 2048) {
        return false;
    }
    if (semantics.point_max_shadow_lights < 0 || semantics.point_max_shadow_lights > 4) {
        return false;
    }
    if (semantics.point_faces_per_frame_budget < 1 || semantics.point_faces_per_frame_budget > 24) {
        return false;
    }
    if (semantics.point_constant_bias < 0.0f || semantics.point_constant_bias > 0.02f) {
        return false;
    }
    if (semantics.point_slope_bias_scale < 0.0f || semantics.point_slope_bias_scale > 8.0f) {
        return false;
    }
    if (semantics.point_normal_bias_scale < 0.0f || semantics.point_normal_bias_scale > 8.0f) {
        return false;
    }
    if (semantics.point_receiver_bias_scale < 0.0f || semantics.point_receiver_bias_scale > 4.0f) {
        return false;
    }
    if (semantics.local_light_distance_damping < 0.0f || semantics.local_light_distance_damping > 2.0f) {
        return false;
    }
    if (semantics.local_light_range_falloff_exponent < 0.1f ||
        semantics.local_light_range_falloff_exponent > 8.0f) {
        return false;
    }
    if (semantics.local_light_directional_shadow_lift_strength < 0.0f ||
        semantics.local_light_directional_shadow_lift_strength > 4.0f) {
        return false;
    }
    return true;
}

inline glm::mat4 BuildDirectionalShadowCascadeUvProjection(const glm::vec3& axis_right,
                                                           const glm::vec3& axis_up,
                                                           const glm::vec3& axis_forward,
                                                           const DirectionalShadowCascade& cascade) {
    glm::mat4 projection{1.0f};
    const float safe_extent = std::max(cascade.extent, 1e-4f);
    const float safe_depth_range = std::max(cascade.depth_range, 1e-4f);
    const float atlas_scale = std::max(cascade.atlas_scale, 0.0f);
    const float x_scale = atlas_scale / (2.0f * safe_extent);
    const float y_scale = atlas_scale / (2.0f * safe_extent);
    const float z_scale = 1.0f / safe_depth_range;
    const float x_bias =
        cascade.atlas_offset.x +
        (0.5f * atlas_scale) -
        (cascade.center_x * x_scale);
    const float y_bias =
        cascade.atlas_offset.y +
        (0.5f * atlas_scale) -
        (cascade.center_y * y_scale);
    const float z_bias = -cascade.min_depth * z_scale;

    projection[0][0] = axis_right.x * x_scale;
    projection[1][0] = axis_right.y * x_scale;
    projection[2][0] = axis_right.z * x_scale;
    projection[3][0] = x_bias;

    projection[0][1] = axis_up.x * y_scale;
    projection[1][1] = axis_up.y * y_scale;
    projection[2][1] = axis_up.z * y_scale;
    projection[3][1] = y_bias;

    projection[0][2] = axis_forward.x * z_scale;
    projection[1][2] = axis_forward.y * z_scale;
    projection[2][2] = axis_forward.z * z_scale;
    projection[3][2] = z_bias;

    projection[0][3] = 0.0f;
    projection[1][3] = 0.0f;
    projection[2][3] = 0.0f;
    projection[3][3] = 1.0f;
    return projection;
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

inline bool ResolvePointShadowFaceBasis(int face, glm::vec3& out_dir, glm::vec3& out_up) {
    static constexpr std::array<glm::vec3, kPointShadowFaceCount> kFaceDirs{{
        {1.0f, 0.0f, 0.0f},
        {-1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, -1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, -1.0f},
    }};
    static constexpr std::array<glm::vec3, kPointShadowFaceCount> kFaceUps{{
        {0.0f, -1.0f, 0.0f},
        {0.0f, -1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, -1.0f},
        {0.0f, -1.0f, 0.0f},
        {0.0f, -1.0f, 0.0f},
    }};
    if (face < 0 || face >= kPointShadowFaceCount) {
        return false;
    }
    out_dir = kFaceDirs[static_cast<std::size_t>(face)];
    out_up = kFaceUps[static_cast<std::size_t>(face)];
    return IsFiniteVec3(out_dir) && IsFiniteVec3(out_up);
}

inline bool ProjectPointShadowPoint(const glm::mat4& view_proj,
                                    const glm::vec3& world_point,
                                    float& out_u,
                                    float& out_v,
                                    float& out_depth) {
    const glm::vec4 clip = view_proj * glm::vec4(world_point, 1.0f);
    if (!std::isfinite(clip.x) || !std::isfinite(clip.y) ||
        !std::isfinite(clip.z) || !std::isfinite(clip.w)) {
        return false;
    }
    const float w = clip.w;
    // Point-shadow faces are perspective projections; reject points behind the
    // face camera to avoid folding invalid geometry into the atlas.
    if (!std::isfinite(w) || w <= 1e-6f) {
        return false;
    }
    const glm::vec3 ndc = glm::vec3(clip) / w;
    if (!IsFiniteVec3(ndc)) {
        return false;
    }
    out_u = (ndc.x * 0.5f) + 0.5f;
    out_v = (ndc.y * 0.5f) + 0.5f;
    out_depth = (ndc.z * 0.5f) + 0.5f;
    return std::isfinite(out_u) && std::isfinite(out_v) && std::isfinite(out_depth);
}

inline std::vector<int> SelectPointShadowLightIndices(const ResolvedDirectionalShadowSemantics& semantics,
                                                       const std::vector<renderer::LightData>& lights) {
    std::vector<int> selected_light_indices{};
    if (!semantics.enabled || semantics.point_max_shadow_lights <= 0) {
        return selected_light_indices;
    }
    selected_light_indices.reserve(static_cast<std::size_t>(semantics.point_max_shadow_lights));
    for (std::size_t i = 0; i < lights.size(); ++i) {
        const renderer::LightData& light = lights[i];
        if (!light.enabled ||
            light.type != renderer::LightType::Point ||
            !light.casts_shadows) {
            continue;
        }
        if (!IsFiniteVec3(light.position)) {
            continue;
        }
        if (!std::isfinite(light.range) ||
            !std::isfinite(light.intensity) ||
            light.range <= 1e-3f ||
            light.intensity <= 0.0f) {
            continue;
        }
        selected_light_indices.push_back(static_cast<int>(i));
        if (static_cast<int>(selected_light_indices.size()) >= semantics.point_max_shadow_lights) {
            break;
        }
    }
    return selected_light_indices;
}

inline bool IsPointShadowMapLayoutCompatible(const PointShadowMap& map,
                                             int map_size,
                                             const std::vector<int>& selected_light_indices) {
    if (!map.ready || map.size != map_size) {
        return false;
    }
    const int expected_light_count = static_cast<int>(selected_light_indices.size());
    if (map.light_count != expected_light_count) {
        return false;
    }
    if (map.atlas_width != map_size * 3 * expected_light_count ||
        map.atlas_height != map_size * 2) {
        return false;
    }
    if (map.source_light_indices != selected_light_indices) {
        return false;
    }
    const std::size_t expected_face_count =
        static_cast<std::size_t>(expected_light_count) * static_cast<std::size_t>(kPointShadowFaceCount);
    if (map.uv_proj.size() < expected_face_count) {
        return false;
    }
    const std::size_t expected_pixels =
        static_cast<std::size_t>(map.atlas_width) * static_cast<std::size_t>(map.atlas_height);
    if (map.depth.size() != expected_pixels) {
        return false;
    }
    return true;
}

inline PointShadowMap BuildPointShadowMap(const ResolvedDirectionalShadowSemantics& semantics,
                                          const std::vector<renderer::LightData>& lights,
                                          const std::vector<DirectionalShadowCaster>& casters,
                                          const PointShadowMap* previous_map = nullptr,
                                          const std::vector<uint8_t>* face_update_mask = nullptr) {
    PointShadowMap map{};
    if (!semantics.enabled ||
        !ValidateResolvedDirectionalShadowSemantics(semantics) ||
        semantics.point_max_shadow_lights <= 0 ||
        semantics.point_map_size <= 0 ||
        casters.empty()) {
        return map;
    }

    std::vector<int> selected_light_indices = SelectPointShadowLightIndices(semantics, lights);
    if (selected_light_indices.empty()) {
        return map;
    }

    map.ready = true;
    map.size = semantics.point_map_size;
    map.light_count = static_cast<int>(selected_light_indices.size());
    map.atlas_width = map.size * 3 * map.light_count;
    map.atlas_height = map.size * 2;
    map.texel_size = map.size > 0 ? (1.0f / static_cast<float>(map.size)) : 0.0f;
    map.source_light_indices = selected_light_indices;
    const std::size_t active_face_count =
        static_cast<std::size_t>(map.light_count) * static_cast<std::size_t>(kPointShadowFaceCount);
    map.uv_proj.assign(active_face_count, glm::mat4(1.0f));

    if (map.atlas_width <= 0 || map.atlas_height <= 0) {
        return PointShadowMap{};
    }
    const std::size_t atlas_pixels =
        static_cast<std::size_t>(map.atlas_width) * static_cast<std::size_t>(map.atlas_height);
    constexpr std::size_t kMaxPointShadowPixels = 33554432u; // 128MB at R32F
    if (atlas_pixels == 0u || atlas_pixels > kMaxPointShadowPixels) {
        return PointShadowMap{};
    }

    const bool previous_compatible =
        previous_map &&
        IsPointShadowMapLayoutCompatible(*previous_map, map.size, selected_light_indices);
    if (previous_compatible) {
        map.depth = previous_map->depth;
        map.uv_proj = previous_map->uv_proj;
    } else {
        map.depth.assign(atlas_pixels, std::numeric_limits<float>::infinity());
    }

    std::vector<uint8_t> resolved_face_update_mask(active_face_count, 1u);
    if (face_update_mask && face_update_mask->size() == active_face_count) {
        bool any_face_selected = false;
        for (std::size_t i = 0; i < active_face_count; ++i) {
            const uint8_t selected = ((*face_update_mask)[i] != 0u) ? 1u : 0u;
            resolved_face_update_mask[i] = selected;
            any_face_selected = any_face_selected || (selected != 0u);
        }
        if (!any_face_selected) {
            if (previous_compatible) {
                return map;
            }
            std::fill(resolved_face_update_mask.begin(), resolved_face_update_mask.end(), 1u);
        }
    }

    glm::mat4 ndc_to_uv_depth(1.0f);
    ndc_to_uv_depth[0][0] = 0.5f;
    ndc_to_uv_depth[1][1] = 0.5f;
    ndc_to_uv_depth[2][2] = 0.5f;
    ndc_to_uv_depth[3][0] = 0.5f;
    ndc_to_uv_depth[3][1] = 0.5f;
    ndc_to_uv_depth[3][2] = 0.5f;

    const float face_raster_scale = static_cast<float>(std::max(map.size - 1, 1));
    const int face_triangle_budget = std::max(1, semantics.triangle_budget);
    const float inv_atlas_cols = 1.0f / static_cast<float>(3 * map.light_count);
    const float inv_atlas_rows = 0.5f;

    for (int slot = 0; slot < map.light_count; ++slot) {
        const int source_idx = map.source_light_indices[static_cast<std::size_t>(slot)];
        if (source_idx < 0 || source_idx >= static_cast<int>(lights.size())) {
            continue;
        }
        const renderer::LightData& light = lights[static_cast<std::size_t>(source_idx)];
        const float range_ws = std::max(light.range, 0.1f);
        const float near_plane = std::max(range_ws * 0.02f, 0.05f);
        const float far_plane = std::max(range_ws, near_plane + 0.1f);
        const glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, near_plane, far_plane);

        for (int face = 0; face < kPointShadowFaceCount; ++face) {
            glm::vec3 face_dir{0.0f, 0.0f, 1.0f};
            glm::vec3 face_up{0.0f, 1.0f, 0.0f};
            if (!ResolvePointShadowFaceBasis(face, face_dir, face_up)) {
                continue;
            }
            const glm::mat4 view = glm::lookAt(light.position, light.position + face_dir, face_up);
            const glm::mat4 view_proj = proj * view;
            const int face_grid_x = (slot * 3) + (face % 3);
            const int face_grid_y = face / 3;
            const int face_origin_x = face_grid_x * map.size;
            const int face_origin_y = face_grid_y * map.size;
            const int face_max_x = face_origin_x + map.size - 1;
            const int face_max_y = face_origin_y + map.size - 1;

            const std::size_t matrix_idx = static_cast<std::size_t>(slot * kPointShadowFaceCount + face);
            if (matrix_idx >= resolved_face_update_mask.size() ||
                resolved_face_update_mask[matrix_idx] == 0u) {
                continue;
            }

            glm::mat4 atlas_tile(1.0f);
            atlas_tile[0][0] = inv_atlas_cols;
            atlas_tile[1][1] = inv_atlas_rows;
            atlas_tile[3][0] = static_cast<float>(face_grid_x) * inv_atlas_cols;
            atlas_tile[3][1] = static_cast<float>(face_grid_y) * inv_atlas_rows;
            map.uv_proj[matrix_idx] = atlas_tile * ndc_to_uv_depth * view_proj;

            for (int y = face_origin_y; y <= face_max_y; ++y) {
                const std::size_t row_offset =
                    static_cast<std::size_t>(y) * static_cast<std::size_t>(map.atlas_width);
                for (int x = face_origin_x; x <= face_max_x; ++x) {
                    map.depth[row_offset + static_cast<std::size_t>(x)] =
                        std::numeric_limits<float>::infinity();
                }
            }

            int triangles_rasterized = 0;
            for (const DirectionalShadowCaster& caster : casters) {
                if (!caster.casts_shadow || !caster.positions || !caster.indices) {
                    continue;
                }
                const auto& positions = *caster.positions;
                const auto& indices = *caster.indices;
                if (positions.empty() || indices.size() < 3u) {
                    continue;
                }

                for (std::size_t tri = 0; (tri + 2u) < indices.size(); tri += 3u) {
                    if (triangles_rasterized >= face_triangle_budget) {
                        break;
                    }
                    const uint32_t i0 = indices[tri + 0u];
                    const uint32_t i1 = indices[tri + 1u];
                    const uint32_t i2 = indices[tri + 2u];
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
                    if (!ProjectPointShadowPoint(view_proj, p0, u0, v0, d0) ||
                        !ProjectPointShadowPoint(view_proj, p1, u1, v1, d1) ||
                        !ProjectPointShadowPoint(view_proj, p2, u2, v2, d2)) {
                        continue;
                    }

                    const float tx0 = static_cast<float>(face_origin_x) + (u0 * face_raster_scale);
                    const float ty0 = static_cast<float>(face_origin_y) + (v0 * face_raster_scale);
                    const float tx1 = static_cast<float>(face_origin_x) + (u1 * face_raster_scale);
                    const float ty1 = static_cast<float>(face_origin_y) + (v1 * face_raster_scale);
                    const float tx2 = static_cast<float>(face_origin_x) + (u2 * face_raster_scale);
                    const float ty2 = static_cast<float>(face_origin_y) + (v2 * face_raster_scale);
                    if (!std::isfinite(tx0) || !std::isfinite(ty0) ||
                        !std::isfinite(tx1) || !std::isfinite(ty1) ||
                        !std::isfinite(tx2) || !std::isfinite(ty2)) {
                        continue;
                    }

                    const float tri_min_x = std::min(tx0, std::min(tx1, tx2));
                    const float tri_max_x = std::max(tx0, std::max(tx1, tx2));
                    const float tri_min_y = std::min(ty0, std::min(ty1, ty2));
                    const float tri_max_y = std::max(ty0, std::max(ty1, ty2));
                    if (tri_max_x < static_cast<float>(face_origin_x) ||
                        tri_max_y < static_cast<float>(face_origin_y) ||
                        tri_min_x > static_cast<float>(face_max_x) ||
                        tri_min_y > static_cast<float>(face_max_y)) {
                        continue;
                    }

                    const int min_tx =
                        std::max(face_origin_x, static_cast<int>(std::floor(tri_min_x)));
                    const int max_tx =
                        std::min(face_max_x, static_cast<int>(std::ceil(tri_max_x)));
                    const int min_ty =
                        std::max(face_origin_y, static_cast<int>(std::floor(tri_min_y)));
                    const int max_ty =
                        std::min(face_max_y, static_cast<int>(std::ceil(tri_max_y)));
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
                            const std::size_t atlas_idx =
                                static_cast<std::size_t>(y) * static_cast<std::size_t>(map.atlas_width) +
                                static_cast<std::size_t>(x);
                            map.depth[atlas_idx] = std::min(map.depth[atlas_idx], depth);
                        }
                    }

                    ++triangles_rasterized;
                }
                if (triangles_rasterized >= face_triangle_budget) {
                    break;
                }
            }
        }
    }

    return map;
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

inline DirectionalShadowCascadeSet BuildDirectionalShadowCascadeSetFromSingleMap(
    const DirectionalShadowMap& map) {
    DirectionalShadowCascadeSet cascades{};
    cascades.single_map_fallback = true;
    cascades.cascade_count = 1;
    cascades.map_size = std::max(map.size, 0);
    cascades.atlas_size = std::max(map.size, 0);
    cascades.pcf_radius = std::max(map.pcf_radius, 0);
    cascades.bias = std::max(map.bias, 0.0f);
    cascades.strength = std::clamp(map.strength, 0.0f, 1.0f);
    cascades.transition_fraction = 0.0f;
    cascades.axis_right = map.axis_right;
    cascades.axis_up = map.axis_up;
    cascades.axis_forward = map.axis_forward;
    cascades.splits.fill(std::numeric_limits<float>::max());

    if (!map.ready || map.size <= 0) {
        return cascades;
    }

    DirectionalShadowCascade cascade{};
    cascade.ready = true;
    cascade.extent = std::max(map.extent, 1e-4f);
    cascade.center_x = map.center_x;
    cascade.center_y = map.center_y;
    cascade.min_depth = map.min_depth;
    cascade.depth_range = std::max(map.depth_range, 1e-4f);
    cascade.world_texel = (map.size > 0)
        ? ((2.0f * cascade.extent) / static_cast<float>(map.size))
        : 0.0f;
    cascade.atlas_offset = glm::vec2(0.0f, 0.0f);
    cascade.atlas_scale = 1.0f;
    cascade.shadow_uv_proj = BuildDirectionalShadowCascadeUvProjection(
        map.axis_right,
        map.axis_up,
        map.axis_forward,
        cascade);

    cascades.cascades[0] = cascade;
    cascades.ready = true;
    return cascades;
}

inline DirectionalShadowCascadeSet BuildDirectionalShadowCascades(
    const ResolvedDirectionalShadowSemantics& semantics,
    const glm::vec3& light_direction,
    const std::vector<DirectionalShadowCaster>& casters,
    const DirectionalShadowView& view,
    const bool force_single_map_fallback = false) {
    DirectionalShadowCascadeSet cascades{};
    if (!semantics.enabled ||
        !ValidateResolvedDirectionalShadowSemantics(semantics) ||
        casters.empty()) {
        return cascades;
    }

    const DirectionalShadowMap fallback_projection =
        BuildDirectionalShadowProjection(semantics, light_direction, casters, &view);
    if (!fallback_projection.ready) {
        return cascades;
    }
    if (force_single_map_fallback) {
        return BuildDirectionalShadowCascadeSetFromSingleMap(fallback_projection);
    }

    cascades.single_map_fallback = false;
    cascades.map_size = std::max(semantics.map_size, 1);
    cascades.atlas_size = cascades.map_size * 2;
    cascades.cascade_count = kDirectionalShadowCascadeCount;
    cascades.pcf_radius = semantics.pcf_radius;
    cascades.bias = semantics.bias;
    cascades.strength = semantics.strength;
    cascades.split_lambda = 0.7f;
    cascades.transition_fraction = 0.08f;
    cascades.axis_right = fallback_projection.axis_right;
    cascades.axis_up = fallback_projection.axis_up;
    cascades.axis_forward = fallback_projection.axis_forward;
    cascades.splits.fill(std::numeric_limits<float>::max());

    const renderer::CameraData& camera = view.camera;
    if (!IsFiniteVec3(camera.position) || !IsFiniteVec3(camera.target)) {
        return BuildDirectionalShadowCascadeSetFromSingleMap(fallback_projection);
    }

    glm::vec3 cam_forward = camera.target - camera.position;
    const float cam_forward_len = glm::length(cam_forward);
    if (!std::isfinite(cam_forward_len) || cam_forward_len <= 1e-6f) {
        return BuildDirectionalShadowCascadeSetFromSingleMap(fallback_projection);
    }
    cam_forward /= cam_forward_len;
    glm::vec3 cam_up{0.0f, 1.0f, 0.0f};
    if (std::fabs(glm::dot(cam_forward, cam_up)) >= 0.95f) {
        cam_up = glm::vec3(0.0f, 0.0f, 1.0f);
    }
    glm::vec3 cam_right = glm::cross(cam_forward, cam_up);
    const float cam_right_len = glm::length(cam_right);
    if (!std::isfinite(cam_right_len) || cam_right_len <= 1e-6f) {
        return BuildDirectionalShadowCascadeSetFromSingleMap(fallback_projection);
    }
    cam_right /= cam_right_len;
    cam_up = glm::normalize(glm::cross(cam_right, cam_forward));
    if (!IsFiniteVec3(cam_up)) {
        return BuildDirectionalShadowCascadeSetFromSingleMap(fallback_projection);
    }

    const float aspect = ClampFinite(view.aspect_ratio, 16.0f / 9.0f, 0.25f, 8.0f);
    const float fov_y_degrees = ClampFinite(camera.fov_y_degrees, 60.0f, 20.0f, 120.0f);
    const float tan_half_fov = std::tan(glm::radians(fov_y_degrees) * 0.5f);
    if (!std::isfinite(tan_half_fov) || tan_half_fov <= 1e-6f) {
        return BuildDirectionalShadowCascadeSetFromSingleMap(fallback_projection);
    }

    const float shadow_near = std::max(ClampFinite(camera.near_clip, 0.1f, 0.05f, 32.0f), 0.05f);
    float shadow_far = std::max(semantics.extent, shadow_near + 1.0f);
    if (std::isfinite(camera.far_clip) && camera.far_clip > shadow_near + 0.1f) {
        shadow_far = std::min(shadow_far, camera.far_clip);
    }
    shadow_far = std::max(shadow_far, shadow_near + 1.0f);
    const float split_lambda = std::clamp(cascades.split_lambda, 0.0f, 1.0f);

    for (int cascade = 0; cascade < kDirectionalShadowCascadeCount; ++cascade) {
        const float p =
            static_cast<float>(cascade + 1) / static_cast<float>(kDirectionalShadowCascadeCount);
        const float uniform_split = shadow_near + ((shadow_far - shadow_near) * p);
        const float log_split = shadow_near * std::pow(shadow_far / shadow_near, p);
        float split = glm::mix(uniform_split, log_split, split_lambda);
        split = std::clamp(split, shadow_near + 0.001f, shadow_far);
        cascades.splits[static_cast<std::size_t>(cascade)] = split;
    }

    auto build_slice_corners = [&](float slice_near,
                                   float slice_far,
                                   std::array<glm::vec3, 8>& out_corners) {
        const float near_h = tan_half_fov * slice_near;
        const float near_w = near_h * aspect;
        const float far_h = tan_half_fov * slice_far;
        const float far_w = far_h * aspect;
        const glm::vec3 near_center = camera.position + (cam_forward * slice_near);
        const glm::vec3 far_center = camera.position + (cam_forward * slice_far);
        out_corners = {
            near_center + (cam_up * near_h) - (cam_right * near_w),
            near_center + (cam_up * near_h) + (cam_right * near_w),
            near_center - (cam_up * near_h) - (cam_right * near_w),
            near_center - (cam_up * near_h) + (cam_right * near_w),
            far_center + (cam_up * far_h) - (cam_right * far_w),
            far_center + (cam_up * far_h) + (cam_right * far_w),
            far_center - (cam_up * far_h) - (cam_right * far_w),
            far_center - (cam_up * far_h) + (cam_right * far_w),
        };
        return true;
    };

    const float safe_shadow_map_extent = std::max(static_cast<float>(cascades.map_size), 1.0f);
    float slice_prev_split = shadow_near;
    bool any_cascade_ready = false;
    for (int cascade_idx = 0; cascade_idx < kDirectionalShadowCascadeCount; ++cascade_idx) {
        const float split_near = slice_prev_split;
        const float split_far = cascades.splits[static_cast<std::size_t>(cascade_idx)];
        slice_prev_split = split_far;
        if (!std::isfinite(split_near) || !std::isfinite(split_far) ||
            split_far <= split_near + 1e-4f) {
            continue;
        }

        std::array<glm::vec3, 8> frustum_corners{};
        if (!build_slice_corners(split_near, split_far, frustum_corners)) {
            continue;
        }

        glm::vec3 frustum_center{0.0f, 0.0f, 0.0f};
        for (const glm::vec3& corner : frustum_corners) {
            frustum_center += corner;
        }
        frustum_center /= static_cast<float>(frustum_corners.size());

        float radius_ws = 0.0f;
        for (const glm::vec3& corner : frustum_corners) {
            radius_ws = std::max(radius_ws, glm::length(corner - frustum_center));
        }
        radius_ws = std::ceil(radius_ws * 16.0f) / 16.0f;
        radius_ws = std::max(radius_ws + 2.0f, 1.0f);

        float center_x = glm::dot(frustum_center, cascades.axis_right);
        float center_y = glm::dot(frustum_center, cascades.axis_up);
        const float units_per_texel = (2.0f * radius_ws) / safe_shadow_map_extent;
        if (units_per_texel > 1e-6f) {
            center_x = std::floor((center_x / units_per_texel) + 0.5f) * units_per_texel;
            center_y = std::floor((center_y / units_per_texel) + 0.5f) * units_per_texel;
        }

        float light_min_z = std::numeric_limits<float>::max();
        float light_max_z = std::numeric_limits<float>::lowest();
        for (const glm::vec3& corner : frustum_corners) {
            const float corner_lz = glm::dot(corner, cascades.axis_forward);
            light_min_z = std::min(light_min_z, corner_lz);
            light_max_z = std::max(light_max_z, corner_lz);
        }
        if (!std::isfinite(light_min_z) || !std::isfinite(light_max_z)) {
            continue;
        }

        const float depth_span = std::max(light_max_z - light_min_z, 1.0f);
        const float depth_padding = std::max(5.0f, depth_span * 0.2f);
        light_min_z -= depth_padding;
        light_max_z += depth_padding;
        const float depth_range = std::max(light_max_z - light_min_z, 0.01f);

        DirectionalShadowCascade cascade{};
        cascade.ready = true;
        cascade.extent = radius_ws;
        cascade.center_x = center_x;
        cascade.center_y = center_y;
        cascade.min_depth = light_min_z;
        cascade.depth_range = depth_range;
        cascade.world_texel = (2.0f * radius_ws) / safe_shadow_map_extent;
        if (kDirectionalShadowCascadeCount > 1) {
            const int col = cascade_idx % 2;
            const int row = cascade_idx / 2;
            cascade.atlas_offset = glm::vec2(static_cast<float>(col) * 0.5f,
                                             static_cast<float>(row) * 0.5f);
            cascade.atlas_scale = 0.5f;
        } else {
            cascade.atlas_offset = glm::vec2(0.0f, 0.0f);
            cascade.atlas_scale = 1.0f;
        }
        cascade.shadow_uv_proj = BuildDirectionalShadowCascadeUvProjection(
            cascades.axis_right,
            cascades.axis_up,
            cascades.axis_forward,
            cascade);
        cascades.cascades[static_cast<std::size_t>(cascade_idx)] = cascade;
        any_cascade_ready = true;
    }

    if (!any_cascade_ready) {
        return BuildDirectionalShadowCascadeSetFromSingleMap(fallback_projection);
    }

    DirectionalShadowCascade* previous_ready = nullptr;
    for (DirectionalShadowCascade& cascade : cascades.cascades) {
        if (cascade.ready) {
            previous_ready = &cascade;
            continue;
        }
        if (previous_ready) {
            cascade = *previous_ready;
        }
    }

    cascades.ready = true;
    return cascades;
}

inline bool IntersectsDirectionalShadowCascade(const DirectionalShadowCascadeSet& cascades,
                                               int cascade_index,
                                               const glm::vec3& center,
                                               float radius) {
    if (!cascades.ready) {
        return true;
    }
    if (cascade_index < 0 ||
        cascade_index >= cascades.cascade_count ||
        cascade_index >= kDirectionalShadowCascadeCount) {
        return false;
    }
    const DirectionalShadowCascade& cascade =
        cascades.cascades[static_cast<std::size_t>(cascade_index)];
    if (!cascade.ready) {
        return false;
    }
    const float lx = glm::dot(center, cascades.axis_right);
    const float ly = glm::dot(center, cascades.axis_up);
    const float lz = glm::dot(center, cascades.axis_forward);
    const float padded_extent = cascade.extent + std::max(radius, 0.0f);
    if (std::fabs(lx - cascade.center_x) > padded_extent ||
        std::fabs(ly - cascade.center_y) > padded_extent) {
        return false;
    }
    if (cascade.depth_range > 1e-6f) {
        const float min_depth = cascade.min_depth - std::max(radius, 0.0f);
        const float max_depth = cascade.min_depth + cascade.depth_range + std::max(radius, 0.0f);
        if (lz < min_depth || lz > max_depth) {
            return false;
        }
    }
    return true;
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

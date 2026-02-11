#pragma once

#include "environment_lighting_internal.hpp"
#include "material_semantics_internal.hpp"

#include "karma/renderer/types.hpp"

#include <algorithm>
#include <cmath>

namespace karma::renderer_backend::detail {

struct ResolvedMaterialLighting {
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 light_color{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 ambient_color{0.0f, 0.0f, 0.0f, 1.0f};
};

inline float ClampLightingFinite(float value, float fallback, float min_value, float max_value) {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, min_value, max_value);
}

inline ResolvedMaterialLighting ResolveMaterialLighting(const ResolvedMaterialSemantics& material,
                                                        const renderer::DirectionalLightData& light,
                                                        const ResolvedEnvironmentLightingSemantics& environment,
                                                        float shadow_factor) {
    const float metallic = ClampLightingFinite(material.metallic, 0.0f, 0.0f, 1.0f);
    const float roughness = ClampLightingFinite(material.roughness, 1.0f, 0.04f, 1.0f);
    const float normal_variation = ClampLightingFinite(material.normal_variation, 0.0f, 0.0f, 1.0f);
    const float occlusion = ClampLightingFinite(material.occlusion, 1.0f, 0.0f, 1.0f);
    const float occlusion_edge = ClampLightingFinite(material.occlusion_edge, 0.0f, 0.0f, 1.0f);
    const float one_minus_metallic = 1.0f - metallic;
    const float smoothness = 1.0f - roughness;
    const float microfacet = smoothness * smoothness;
    const float fresnel_f0 = 0.04f + (0.96f * metallic);

    ResolvedMaterialLighting out{};
    out.color = glm::vec4(
        glm::clamp(glm::vec3(material.base_color) + material.emissive, glm::vec3(0.0f), glm::vec3(4.0f)),
        ClampLightingFinite(material.base_color.a, 1.0f, 0.0f, 1.0f));

    const float diffuse_direct = one_minus_metallic * (0.60f + (0.40f * smoothness));
    const float specular_direct = fresnel_f0 * (0.20f + (0.80f * microfacet));
    const float normal_response = std::clamp(std::sqrt(normal_variation), 0.0f, 1.0f);
    const float normal_detail_gain = std::clamp(
        1.0f + (normal_response * ((0.05f + (0.17f * smoothness)) - (0.05f * roughness))),
        0.92f,
        1.25f);
    const float direct_scale = std::clamp(
        (diffuse_direct + specular_direct) *
            ComputeEnvironmentSpecularBoost(environment, roughness) *
            normal_detail_gain,
        0.05f,
        1.8f);

    const float occlusion_edge_lift =
        0.10f * occlusion_edge * (1.0f - occlusion) * (0.30f + (0.70f * smoothness));
    const float integrated_occlusion = std::clamp(occlusion + occlusion_edge_lift, 0.0f, 1.0f);
    const float diffuse_ambient = one_minus_metallic * (0.40f + (0.60f * roughness));
    const float specular_ambient = fresnel_f0 * (0.05f + (0.35f * microfacet)) * environment.specular_strength;
    const float ambient_scale = std::clamp(
        (diffuse_ambient + specular_ambient) *
            (0.35f + (0.65f * integrated_occlusion)),
        0.30f,
        2.25f);

    const float shadow = ClampLightingFinite(shadow_factor, 1.0f, 0.0f, 1.0f);
    out.light_color = glm::vec4(
        glm::clamp(glm::vec3(light.color) * (direct_scale * shadow), glm::vec3(0.0f), glm::vec3(8.0f)),
        ClampLightingFinite(light.color.a, 1.0f, 0.0f, 1.0f));

    const glm::vec3 environment_ambient = ComputeEnvironmentAmbientColor(environment, light.direction);
    out.ambient_color = glm::vec4(
        glm::clamp((glm::vec3(light.ambient) * ambient_scale) + environment_ambient, glm::vec3(0.0f), glm::vec3(8.0f)),
        ClampLightingFinite(light.ambient.a, 1.0f, 0.0f, 1.0f));
    return out;
}

inline bool ValidateResolvedMaterialLighting(const ResolvedMaterialLighting& lighting) {
    const auto finite_vec4 = [](const glm::vec4& v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) && std::isfinite(v.w);
    };
    if (!finite_vec4(lighting.color) ||
        !finite_vec4(lighting.light_color) ||
        !finite_vec4(lighting.ambient_color)) {
        return false;
    }
    if (lighting.color.a < 0.0f || lighting.color.a > 1.0f) {
        return false;
    }
    if (lighting.light_color.w < 0.0f || lighting.light_color.w > 1.0f) {
        return false;
    }
    if (lighting.ambient_color.w < 0.0f || lighting.ambient_color.w > 1.0f) {
        return false;
    }
    return true;
}

} // namespace karma::renderer_backend::detail

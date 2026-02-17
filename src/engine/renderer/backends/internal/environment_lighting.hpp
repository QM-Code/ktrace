#pragma once

#include "karma/renderer/types.hpp"

#include <algorithm>
#include <cmath>

namespace karma::renderer_backend::detail {

struct ResolvedEnvironmentLightingSemantics {
    bool enabled = true;
    glm::vec3 sky_color{0.56f, 0.66f, 0.88f};
    glm::vec3 ground_color{0.14f, 0.14f, 0.16f};
    float diffuse_strength = 0.75f;
    float specular_strength = 0.20f;
    float skybox_exposure = 1.0f;
};

inline float ClampEnvironmentFinite(float value, float fallback, float min_value, float max_value) {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, min_value, max_value);
}

inline glm::vec3 ClampColor(const glm::vec4& color, const glm::vec3& fallback) {
    const auto clamp_component = [](float value, float fb) {
        if (!std::isfinite(value)) {
            return fb;
        }
        return std::clamp(value, 0.0f, 8.0f);
    };
    return glm::vec3{
        clamp_component(color.r, fallback.r),
        clamp_component(color.g, fallback.g),
        clamp_component(color.b, fallback.b),
    };
}

inline ResolvedEnvironmentLightingSemantics ResolveEnvironmentLightingSemantics(
    const renderer::EnvironmentLightingData& environment) {
    ResolvedEnvironmentLightingSemantics semantics{};
    semantics.enabled = environment.enabled;
    semantics.sky_color = ClampColor(environment.sky_color, semantics.sky_color);
    semantics.ground_color = ClampColor(environment.ground_color, semantics.ground_color);
    semantics.diffuse_strength = ClampEnvironmentFinite(environment.diffuse_strength, 0.75f, 0.0f, 4.0f);
    semantics.specular_strength = ClampEnvironmentFinite(environment.specular_strength, 0.20f, 0.0f, 2.0f);
    semantics.skybox_exposure = ClampEnvironmentFinite(environment.skybox_exposure, 1.0f, 0.0f, 4.0f);
    return semantics;
}

inline bool ValidateResolvedEnvironmentLightingSemantics(const ResolvedEnvironmentLightingSemantics& semantics) {
    const auto finite_vec3 = [](const glm::vec3& v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    };
    if (!finite_vec3(semantics.sky_color) || !finite_vec3(semantics.ground_color)) {
        return false;
    }
    if (!std::isfinite(semantics.diffuse_strength) ||
        !std::isfinite(semantics.specular_strength) ||
        !std::isfinite(semantics.skybox_exposure)) {
        return false;
    }
    if (semantics.diffuse_strength < 0.0f || semantics.diffuse_strength > 4.0f) {
        return false;
    }
    if (semantics.specular_strength < 0.0f || semantics.specular_strength > 2.0f) {
        return false;
    }
    if (semantics.skybox_exposure < 0.0f || semantics.skybox_exposure > 4.0f) {
        return false;
    }
    return true;
}

inline glm::vec3 NormalizeOrFallback(const glm::vec3& value, const glm::vec3& fallback) {
    if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z)) {
        return glm::normalize(fallback);
    }
    const float len = glm::length(value);
    if (len <= 1e-6f) {
        return glm::normalize(fallback);
    }
    return value / len;
}

inline glm::vec3 ComputeEnvironmentAmbientColor(const ResolvedEnvironmentLightingSemantics& environment,
                                                const glm::vec3& light_direction) {
    if (!environment.enabled) {
        return glm::vec3(0.0f);
    }
    const glm::vec3 direction = NormalizeOrFallback(light_direction, glm::vec3(0.3f, -1.0f, -0.2f));
    const float sky_mix = std::clamp(0.5f - (0.5f * direction.y), 0.0f, 1.0f);
    const glm::vec3 hemispherical = glm::mix(environment.ground_color, environment.sky_color, sky_mix);
    return hemispherical * environment.diffuse_strength;
}

inline float ComputeEnvironmentSpecularBoost(const ResolvedEnvironmentLightingSemantics& environment,
                                             float roughness) {
    if (!environment.enabled) {
        return 1.0f;
    }
    const float smoothness = std::clamp(1.0f - roughness, 0.0f, 1.0f);
    return 1.0f + (0.20f * environment.specular_strength * smoothness);
}

inline glm::vec4 ComputeEnvironmentClearColor(const ResolvedEnvironmentLightingSemantics& environment) {
    if (!environment.enabled) {
        return glm::vec4(0.18f, 0.18f, 0.18f, 1.0f);
    }
    const glm::vec3 clear = glm::clamp(environment.sky_color * environment.skybox_exposure,
                                       glm::vec3(0.0f),
                                       glm::vec3(4.0f));
    return glm::vec4(clear, 1.0f);
}

} // namespace karma::renderer_backend::detail

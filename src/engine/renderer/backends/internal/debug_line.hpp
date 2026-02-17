#pragma once

#include "karma/renderer/types.hpp"

#include <algorithm>
#include <cmath>

namespace karma::renderer_backend::detail {

struct ResolvedDebugLineSemantics {
    glm::vec3 start{0.0f, 0.0f, 0.0f};
    glm::vec3 end{0.0f, 0.0f, 0.0f};
    glm::vec4 color{1.0f, 0.0f, 0.0f, 1.0f};
    renderer::LayerId layer = 0;
    bool draw = true;
};

inline bool IsFiniteDebugLineVec3(const glm::vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

inline bool IsFiniteDebugLineVec4(const glm::vec4& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) && std::isfinite(value.w);
}

inline float ClampColor(float value, float fallback) {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, 0.0f, 1.0f);
}

inline ResolvedDebugLineSemantics ResolveDebugLineSemantics(const renderer::DebugLineItem& line) {
    ResolvedDebugLineSemantics semantics{};
    semantics.layer = line.layer;
    semantics.start = line.start;
    semantics.end = line.end;
    semantics.color = glm::vec4(
        ClampColor(line.color.r, 1.0f),
        ClampColor(line.color.g, 0.0f),
        ClampColor(line.color.b, 0.0f),
        ClampColor(line.color.a, 1.0f));

    if (!IsFiniteDebugLineVec3(semantics.start) || !IsFiniteDebugLineVec3(semantics.end)) {
        semantics.draw = false;
        return semantics;
    }

    const glm::vec3 delta = semantics.end - semantics.start;
    const float len2 = glm::dot(delta, delta);
    semantics.draw = std::isfinite(len2) && (len2 > 1e-10f);
    return semantics;
}

inline bool ValidateResolvedDebugLineSemantics(const ResolvedDebugLineSemantics& semantics) {
    if (!IsFiniteDebugLineVec3(semantics.start) || !IsFiniteDebugLineVec3(semantics.end) || !IsFiniteDebugLineVec4(semantics.color)) {
        return false;
    }
    if (semantics.color.r < 0.0f || semantics.color.r > 1.0f ||
        semantics.color.g < 0.0f || semantics.color.g > 1.0f ||
        semantics.color.b < 0.0f || semantics.color.b > 1.0f ||
        semantics.color.a < 0.0f || semantics.color.a > 1.0f) {
        return false;
    }
    return true;
}

} // namespace karma::renderer_backend::detail

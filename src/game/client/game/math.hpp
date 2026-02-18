#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace bz3::client::game::detail {

inline constexpr float kEpsilon = 1e-6f;
inline constexpr float kTwoPi = glm::pi<float>() * 2.0f;

inline float Clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

inline float SmoothAlpha(float dt, float smooth_rate) {
    if (dt <= 0.0f || smooth_rate <= 0.0f) {
        return 1.0f;
    }
    const float alpha = 1.0f - std::exp(-smooth_rate * dt);
    return Clamp01(alpha);
}

inline float NormalizeAngle(float radians) {
    if (!std::isfinite(radians)) {
        return 0.0f;
    }
    const float wrapped = std::fmod(radians, kTwoPi);
    if (wrapped < 0.0f) {
        return wrapped + kTwoPi;
    }
    return wrapped;
}

inline float LerpAngle(float from, float to, float alpha) {
    const float clamped_alpha = Clamp01(alpha);
    const float delta = std::remainder(to - from, kTwoPi);
    return NormalizeAngle(from + (delta * clamped_alpha));
}

inline glm::vec3 LerpVec3(const glm::vec3& from, const glm::vec3& to, float alpha) {
    const float t = Clamp01(alpha);
    return from + ((to - from) * t);
}

} // namespace bz3::client::game::detail

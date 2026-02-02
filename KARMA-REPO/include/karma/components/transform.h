#pragma once

#include "karma/ecs/component.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace karma::components {

struct TransformComponent : ecs::ComponentTag {
  glm::vec3 position{0.0f, 0.0f, 0.0f};
  glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
  glm::vec3 scale{1.0f, 1.0f, 1.0f};
};

}  // namespace karma::components

#pragma once

#include "karma/components/transform.h"
#include "karma/ecs/component.h"
#include "karma/physics/types.h"
#include <string>
#include <glm/glm.hpp>

namespace karma::components {

struct ColliderComponent : ecs::ComponentTag {
  enum class Shape {
    Box,
    Sphere,
    Capsule,
    Mesh
  };

  Shape shape = Shape::Box;
  glm::vec3 center{};
  glm::vec3 half_extents{0.5f, 0.5f, 0.5f};
  float radius = 0.5f;
  float height = 1.0f;
  bool is_trigger = false;
  std::string mesh_key;
  karma::physics::PhysicsMaterial material{};
};

}  // namespace karma::components

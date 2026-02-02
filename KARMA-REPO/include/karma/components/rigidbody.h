#pragma once

#include "karma/components/transform.h"
#include "karma/ecs/component.h"
#include <glm/glm.hpp>

namespace karma::components {

class RigidbodyComponent : public ecs::ComponentTag {
 public:
  float mass = 1.0f;
  glm::vec3 velocity{};
  glm::vec3 angular_velocity{};
  bool is_kinematic = false;
  bool use_gravity = true;

  void setPosition(const glm::vec3& position) {
    position_ = position;
    teleport_position_ = position;
    teleport_ = true;
  }

  glm::vec3 getPosition() const { return position_; }

  bool consumeTeleport(glm::vec3& out_position) {
    if (!teleport_) {
      return false;
    }
    out_position = teleport_position_;
    teleport_ = false;
    return true;
  }

  void syncPosition(const glm::vec3& position) { position_ = position; }

 private:
  bool teleport_ = false;
  glm::vec3 teleport_position_{};
  glm::vec3 position_{};
};

}  // namespace karma::components

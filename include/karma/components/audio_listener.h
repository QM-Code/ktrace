#pragma once

#include "karma/ecs/component.h"

namespace karma::components {

struct AudioListenerComponent : ecs::ComponentTag {
  bool active = true;
};

}  // namespace karma::components

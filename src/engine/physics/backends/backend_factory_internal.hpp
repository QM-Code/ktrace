#pragma once

#include "karma/physics/backend.hpp"

namespace karma::physics_backend {

std::unique_ptr<Backend> CreateJoltBackend();
std::unique_ptr<Backend> CreatePhysXBackend();

} // namespace karma::physics_backend


#pragma once

#include "karma/renderer/backend.hpp"

namespace karma::renderer_backend {

std::unique_ptr<Backend> CreateBgfxBackend(karma::platform::Window& window);
std::unique_ptr<Backend> CreateDiligentBackend(karma::platform::Window& window);

} // namespace karma::renderer_backend

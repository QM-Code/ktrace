#pragma once

#include "game.hpp"
#include "karma/app/engine_app.hpp"
#include "karma/cli/client_app_options.hpp"
#include "karma/cli/client_runtime_options.hpp"

#include <glm/glm.hpp>

#include <string>

namespace bz3::client::runtime_detail {

glm::vec3 ReadRequiredVec3(const char* path);
glm::vec4 ReadRequiredColor(const char* path);

bz3::GameStartupOptions ResolveGameStartupOptions(const karma::cli::ClientAppOptions& options);
karma::app::EngineConfig BuildEngineConfig(const karma::cli::ClientAppOptions& options);

} // namespace bz3::client::runtime_detail

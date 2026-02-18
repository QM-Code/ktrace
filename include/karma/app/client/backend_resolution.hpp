#pragma once

#include "karma/renderer/backend.hpp"
#include "karma/ui/backend.hpp"

#include <optional>
#include <string>

namespace karma::app::client {

renderer::backend::BackendKind ResolveRenderBackendFromOption(
    const std::string& option_value,
    bool option_explicit);
std::string CompiledWindowBackendName();
void ValidateWindowBackendFromOption(
    const std::string& option_value,
    bool option_explicit);
std::string ReadPreferredVideoDriverFromConfig();

std::optional<ui::backend::BackendKind> ResolveUiBackendOverrideFromOption(
    const std::string& option_value,
    bool option_explicit);

} // namespace karma::app::client

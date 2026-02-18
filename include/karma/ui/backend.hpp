#pragma once

#include <optional>
#include <string_view>
#include <vector>

namespace karma::ui::backend {

enum class BackendKind {
    Auto,
    ImGui,
    RmlUi,
    Software
};

const char* BackendKindName(BackendKind kind);
std::optional<BackendKind> ParseBackendKind(std::string_view name);
std::vector<BackendKind> CompiledBackends();

} // namespace karma::ui::backend

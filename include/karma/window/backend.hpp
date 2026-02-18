#pragma once

#include "karma/window/events.hpp"

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace karma::window {
class Window;
}

namespace karma::window::backend {

enum class BackendKind {
    Auto,
    Sdl3
};

const char* BackendKindName(BackendKind kind);
std::optional<BackendKind> ParseBackendKind(std::string_view name);
std::vector<BackendKind> CompiledBackends();

std::unique_ptr<Window> CreateBackend(const WindowConfig& config,
                                      BackendKind preferred = BackendKind::Auto,
                                      BackendKind* out_selected = nullptr);

} // namespace karma::window::backend

#include "karma/platform/window.hpp"

#include <memory>

#include "platform/backends/window_sdl3.hpp"

namespace karma::platform {

std::unique_ptr<Window> CreateWindow(const WindowConfig& config) {
    return std::make_unique<WindowSdl3>(config);
}

} // namespace karma::platform

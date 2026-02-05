#include "karma/platform/window.hpp"

#include <memory>

#if defined(KARMA_WINDOW_BACKEND_SDL3)
#include "platform/backends/window_sdl3.hpp"
#elif defined(KARMA_WINDOW_BACKEND_SDL2)
#include "platform/backends/window_sdl2_stub.hpp"
#elif defined(KARMA_WINDOW_BACKEND_GLFW)
#include "platform/backends/window_glfw_stub.hpp"
#endif

namespace karma::platform {

std::unique_ptr<Window> CreateWindow(const WindowConfig& config) {
#if defined(KARMA_WINDOW_BACKEND_SDL3)
    return std::make_unique<WindowSdl3>(config);
#elif defined(KARMA_WINDOW_BACKEND_SDL2)
    return std::make_unique<WindowSdl2Stub>(config);
#elif defined(KARMA_WINDOW_BACKEND_GLFW)
    return std::make_unique<WindowGlfwStub>(config);
#else
    (void)config;
    return nullptr;
#endif
}

} // namespace karma::platform

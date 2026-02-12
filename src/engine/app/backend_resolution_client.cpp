#include "karma/app/backend_resolution.hpp"

#include "karma/common/config_helpers.hpp"
#include "karma/common/config_store.hpp"

#include <algorithm>
#include <stdexcept>

namespace karma::app {

renderer_backend::BackendKind ResolveRenderBackendFromOption(const std::string& option_value,
                                                             bool option_explicit) {
    const std::string configured = option_explicit
        ? option_value
        : config::ReadStringConfig("render.backend", "auto");
    const auto parsed = renderer_backend::ParseBackendKind(configured);
    if (!parsed) {
        const char* source = option_explicit ? "--backend-render" : "config 'render.backend'";
        throw std::runtime_error(std::string("Invalid value for ") + source + ": '" + configured
                                 + "' (expected: auto|bgfx|diligent)");
    }
    if (*parsed != renderer_backend::BackendKind::Auto) {
        const auto compiled = renderer_backend::CompiledBackends();
        const bool supported = std::any_of(compiled.begin(),
                                           compiled.end(),
                                           [parsed](renderer_backend::BackendKind kind) {
                                               return kind == *parsed;
                                           });
        if (!supported) {
            throw std::runtime_error(
                std::string("Configured render backend '") + configured + "' is not compiled into this binary.");
        }
    }
    return *parsed;
}

std::string CompiledPlatformBackendName() {
#if defined(KARMA_WINDOW_BACKEND_SDL3)
    return "sdl3";
#elif defined(KARMA_WINDOW_BACKEND_SDL2)
    return "sdl2";
#elif defined(KARMA_WINDOW_BACKEND_GLFW)
    return "glfw";
#else
    return "unknown";
#endif
}

void ValidatePlatformBackendFromOption(const std::string& option_value, bool option_explicit) {
    if (!option_explicit) {
        return;
    }
    const std::string compiled = CompiledPlatformBackendName();
    if (option_value != compiled) {
        throw std::runtime_error("Requested CLI platform backend '" + option_value
                                 + "' but this build only supports '" + compiled + "'.");
    }
}

std::string ReadPreferredVideoDriverFromConfig() {
    if (const auto* value = config::ConfigStore::Get("platform.VideoDriver")) {
        if (value->is_string()) {
            return value->get<std::string>();
        }
        throw std::runtime_error("Missing required string config: platform.VideoDriver");
    }
    return config::ReadRequiredStringConfig("platform.SdlVideoDriver");
}

std::optional<ui::Backend> ResolveUiBackendOverrideFromOption(const std::string& option_value,
                                                              bool option_explicit) {
    if (!option_explicit) {
        return std::nullopt;
    }
    if (option_value == "imgui") {
        return ui::Backend::ImGui;
    }
    if (option_value == "rmlui") {
        return ui::Backend::RmlUi;
    }
    throw std::runtime_error(std::string("Invalid CLI value for --backend-ui: '")
                             + option_value + "' (expected: imgui|rmlui)");
}

} // namespace karma::app

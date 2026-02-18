#include "karma/window/backend.hpp"
#include "karma/window/window.hpp"

#include "window/backends/factory_internal.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace karma::window::backend {
namespace {

std::string Lower(std::string_view input) {
    std::string text(input);
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

std::unique_ptr<Window> CreateBackendForKind(const WindowConfig& config, BackendKind kind) {
    switch (kind) {
        case BackendKind::Sdl3:
#if defined(KARMA_WINDOW_BACKEND_SDL3)
            return CreateSdl3WindowBackend(config);
#else
            return nullptr;
#endif
        case BackendKind::Auto:
        default:
            return nullptr;
    }
}

} // namespace

const char* BackendKindName(BackendKind kind) {
    switch (kind) {
        case BackendKind::Auto: return "auto";
        case BackendKind::Sdl3: return "sdl3";
        default: return "unknown";
    }
}

std::optional<BackendKind> ParseBackendKind(std::string_view name) {
    const std::string value = Lower(name);
    if (value.empty() || value == "auto") {
        return BackendKind::Auto;
    }
    if (value == "sdl3") {
        return BackendKind::Sdl3;
    }
    return std::nullopt;
}

std::vector<BackendKind> CompiledBackends() {
    std::vector<BackendKind> backends;
#if defined(KARMA_WINDOW_BACKEND_SDL3)
    backends.push_back(BackendKind::Sdl3);
#endif
    return backends;
}

std::unique_ptr<Window> CreateBackend(const WindowConfig& config,
                                      BackendKind preferred,
                                      BackendKind* out_selected) {
    if (out_selected) {
        *out_selected = BackendKind::Auto;
    }

    std::vector<BackendKind> candidates;
    if (preferred == BackendKind::Auto) {
        candidates = CompiledBackends();
    } else {
        candidates.push_back(preferred);
    }

    for (const BackendKind kind : candidates) {
        auto backend = CreateBackendForKind(config, kind);
        if (!backend) {
            continue;
        }
        if (out_selected) {
            *out_selected = kind;
        }
        return backend;
    }
    return nullptr;
}

} // namespace karma::window::backend

namespace karma::window {

std::unique_ptr<Window> CreateWindow(const WindowConfig& config) {
    return backend::CreateBackend(config);
}

} // namespace karma::window

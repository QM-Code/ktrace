#include "karma/renderer/backend.hpp"

#include "backends/backend_factory_internal.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace karma::renderer_backend {
namespace {

std::string Lower(std::string_view input) {
    std::string text(input);
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return text;
}

std::unique_ptr<Backend> CreateBackendForKind(karma::platform::Window& window, BackendKind kind) {
    switch (kind) {
        case BackendKind::Bgfx:
#if defined(KARMA_HAS_RENDER_BGFX)
            return CreateBgfxBackend(window);
#else
            return nullptr;
#endif
        case BackendKind::Diligent:
#if defined(KARMA_HAS_RENDER_DILIGENT)
            return CreateDiligentBackend(window);
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
        case BackendKind::Bgfx: return "bgfx";
        case BackendKind::Diligent: return "diligent";
        default: return "unknown";
    }
}

std::optional<BackendKind> ParseBackendKind(std::string_view name) {
    const std::string value = Lower(name);
    if (value.empty() || value == "auto") {
        return BackendKind::Auto;
    }
    if (value == "bgfx") {
        return BackendKind::Bgfx;
    }
    if (value == "diligent") {
        return BackendKind::Diligent;
    }
    return std::nullopt;
}

std::vector<BackendKind> CompiledBackends() {
    std::vector<BackendKind> backends;
#if defined(KARMA_HAS_RENDER_BGFX)
    backends.push_back(BackendKind::Bgfx);
#endif
#if defined(KARMA_HAS_RENDER_DILIGENT)
    backends.push_back(BackendKind::Diligent);
#endif
    return backends;
}

std::unique_ptr<Backend> CreateBackend(karma::platform::Window& window,
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

    for (BackendKind kind : candidates) {
        auto backend = CreateBackendForKind(window, kind);
        if (!backend) {
            continue;
        }
        if (backend->isValid()) {
            if (out_selected) {
                *out_selected = kind;
            }
            return backend;
        }
    }
    return nullptr;
}

} // namespace karma::renderer_backend

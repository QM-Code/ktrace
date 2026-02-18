#include "ui/backends/driver.hpp"

#include "ui/backends/factory_internal.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace karma::ui::backend {
namespace {

std::string Lower(std::string_view input) {
    std::string text(input);
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

std::unique_ptr<BackendDriver> CreateBackendForKind(BackendKind kind) {
    switch (kind) {
        case BackendKind::ImGui:
#if defined(KARMA_HAS_IMGUI)
            return CreateImGuiBackend();
#else
            return nullptr;
#endif
        case BackendKind::RmlUi:
#if defined(KARMA_HAS_RMLUI)
            return CreateRmlUiBackend();
#else
            return nullptr;
#endif
        case BackendKind::Software:
            return CreateSoftwareBackend();
        case BackendKind::Auto:
        default:
            return nullptr;
    }
}

} // namespace

const char* BackendKindName(BackendKind kind) {
    switch (kind) {
        case BackendKind::Auto: return "auto";
        case BackendKind::ImGui: return "imgui";
        case BackendKind::RmlUi: return "rmlui";
        case BackendKind::Software: return "software";
        default: return "unknown";
    }
}

std::optional<BackendKind> ParseBackendKind(std::string_view name) {
    const std::string value = Lower(name);
    if (value.empty() || value == "auto") {
        return BackendKind::Auto;
    }
    if (value == "imgui") {
        return BackendKind::ImGui;
    }
    if (value == "rmlui") {
        return BackendKind::RmlUi;
    }
    if (value == "software" || value == "software-overlay") {
        return BackendKind::Software;
    }
    return std::nullopt;
}

std::vector<BackendKind> CompiledBackends() {
    std::vector<BackendKind> backends;
#if defined(KARMA_HAS_IMGUI)
    backends.push_back(BackendKind::ImGui);
#endif
#if defined(KARMA_HAS_RMLUI)
    backends.push_back(BackendKind::RmlUi);
#endif
    backends.push_back(BackendKind::Software);
    return backends;
}

std::unique_ptr<BackendDriver> CreateBackend(BackendKind preferred, BackendKind* out_selected) {
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
        auto backend = CreateBackendForKind(kind);
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

} // namespace karma::ui::backend

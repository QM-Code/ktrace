#include "karma/physics/backend.hpp"

#include "physics/backends/backend_factory_internal.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace karma::physics_backend {
namespace {

std::string Lower(std::string_view input) {
    std::string text(input);
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

std::unique_ptr<Backend> CreateBackendForKind(BackendKind kind) {
    switch (kind) {
        case BackendKind::Jolt:
#if defined(KARMA_HAS_PHYSICS_JOLT)
            return CreateJoltBackend();
#else
            return nullptr;
#endif
        case BackendKind::PhysX:
#if defined(KARMA_HAS_PHYSICS_PHYSX)
            return CreatePhysXBackend();
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
        case BackendKind::Jolt: return "jolt";
        case BackendKind::PhysX: return "physx";
        default: return "unknown";
    }
}

std::optional<BackendKind> ParseBackendKind(std::string_view name) {
    const std::string value = Lower(name);
    if (value.empty() || value == "auto") {
        return BackendKind::Auto;
    }
    if (value == "jolt") {
        return BackendKind::Jolt;
    }
    if (value == "physx") {
        return BackendKind::PhysX;
    }
    return std::nullopt;
}

std::vector<BackendKind> CompiledBackends() {
    std::vector<BackendKind> backends;
#if defined(KARMA_HAS_PHYSICS_JOLT)
    backends.push_back(BackendKind::Jolt);
#endif
#if defined(KARMA_HAS_PHYSICS_PHYSX)
    backends.push_back(BackendKind::PhysX);
#endif
    return backends;
}

std::unique_ptr<Backend> CreateBackend(BackendKind preferred, BackendKind* out_selected) {
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

} // namespace karma::physics_backend

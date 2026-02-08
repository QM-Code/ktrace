#include "karma/audio/backend.hpp"

#include "audio/backends/backend_factory_internal.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace karma::audio_backend {
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
        case BackendKind::Sdl3Audio:
#if defined(KARMA_HAS_AUDIO_SDL3AUDIO)
            return CreateSdl3AudioBackend();
#else
            return nullptr;
#endif
        case BackendKind::Miniaudio:
#if defined(KARMA_HAS_AUDIO_MINIAUDIO)
            return CreateMiniaudioBackend();
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
        case BackendKind::Sdl3Audio: return "sdl3audio";
        case BackendKind::Miniaudio: return "miniaudio";
        default: return "unknown";
    }
}

std::optional<BackendKind> ParseBackendKind(std::string_view name) {
    const std::string value = Lower(name);
    if (value.empty() || value == "auto") {
        return BackendKind::Auto;
    }
    if (value == "sdl3audio") {
        return BackendKind::Sdl3Audio;
    }
    if (value == "miniaudio") {
        return BackendKind::Miniaudio;
    }
    return std::nullopt;
}

std::vector<BackendKind> CompiledBackends() {
    std::vector<BackendKind> backends;
#if defined(KARMA_HAS_AUDIO_SDL3AUDIO)
    backends.push_back(BackendKind::Sdl3Audio);
#endif
#if defined(KARMA_HAS_AUDIO_MINIAUDIO)
    backends.push_back(BackendKind::Miniaudio);
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

} // namespace karma::audio_backend


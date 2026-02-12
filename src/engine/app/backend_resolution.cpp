#include "karma/app/backend_resolution.hpp"

#include "karma/common/config_helpers.hpp"

#include <algorithm>
#include <stdexcept>

namespace karma::app {

physics_backend::BackendKind ResolvePhysicsBackendFromOption(const std::string& option_value,
                                                             bool option_explicit) {
    const std::string configured = option_explicit
        ? option_value
        : config::ReadStringConfig("physics.backend", "auto");
    const auto parsed = physics_backend::ParseBackendKind(configured);
    if (!parsed) {
        const char* source = option_explicit ? "--backend-physics" : "config 'physics.backend'";
        throw std::runtime_error(std::string("Invalid value for ") + source + ": '" + configured
                                 + "' (expected: auto|jolt|physx)");
    }
    if (*parsed != physics_backend::BackendKind::Auto) {
        const auto compiled = physics_backend::CompiledBackends();
        const bool supported = std::any_of(compiled.begin(),
                                           compiled.end(),
                                           [parsed](physics_backend::BackendKind kind) {
                                               return kind == *parsed;
                                           });
        if (!supported) {
            throw std::runtime_error(
                std::string("Configured physics backend '") + configured + "' is not compiled into this binary.");
        }
    }
    return *parsed;
}

audio_backend::BackendKind ResolveAudioBackendFromOption(const std::string& option_value,
                                                         bool option_explicit) {
    const std::string configured = option_explicit
        ? option_value
        : config::ReadStringConfig("audio.backend", "auto");
    const auto parsed = audio_backend::ParseBackendKind(configured);
    if (!parsed) {
        const char* source = option_explicit ? "--backend-audio" : "config 'audio.backend'";
        throw std::runtime_error(std::string("Invalid value for ") + source + ": '" + configured
                                 + "' (expected: auto|sdl3audio|miniaudio)");
    }
    if (*parsed != audio_backend::BackendKind::Auto) {
        const auto compiled = audio_backend::CompiledBackends();
        const bool supported = std::any_of(compiled.begin(),
                                           compiled.end(),
                                           [parsed](audio_backend::BackendKind kind) {
                                               return kind == *parsed;
                                           });
        if (!supported) {
            throw std::runtime_error(
                std::string("Configured audio backend '") + configured + "' is not compiled into this binary.");
        }
    }
    return *parsed;
}

} // namespace karma::app

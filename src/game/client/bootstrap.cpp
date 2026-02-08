#include "client/bootstrap.hpp"

#include "karma/common/config_store.hpp"
#include "karma/common/data_dir_override.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "karma/common/logging.hpp"
#include "karma/common/config_validation.hpp"

#include <spdlog/spdlog.h>

#include <stdexcept>
#include <vector>

namespace bz3::client {
namespace {

const char* CompiledPlatformBackendName() {
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

}

void ConfigureLogging(const CLIOptions& options) {
    karma::logging::ConfigureLogPatterns(options.timestamp_logging);
    spdlog::set_level(options.verbose ? spdlog::level::debug : spdlog::level::info);
    if (options.trace_explicit) {
        karma::logging::EnableTraceChannels(options.trace_channels);
    }
}

void ConfigureDataAndConfig(int argc, char** argv) {
    karma::data::DataPathSpec spec;
    spec.appName = "bz3";
    spec.dataDirEnvVar = "BZ3_DATA_DIR";
    spec.requiredDataMarker = "common/config.json";
    karma::data::SetDataPathSpec(spec);

    const auto dataDirResult = karma::data::ApplyDataDirOverrideFromArgs(argc, argv);
    const std::vector<karma::config::ConfigFileSpec> configSpecs = {
        {"common/config.json", "data/common/config.json", spdlog::level::err, true, true},
        {"client/config.json", "data/client/config.json", spdlog::level::debug, false, true}
    };
    karma::config::ConfigStore::Initialize(configSpecs, dataDirResult.userConfigPath);
}

void ApplyRuntimeOptionOverrides(const CLIOptions& options) {
    if (options.language_explicit) {
        if (!karma::config::ConfigStore::Set("language", options.language)) {
            throw std::runtime_error("Failed to apply CLI language override.");
        }
        KARMA_TRACE("config", "Applied CLI language override: {}", options.language);
    }
    if (options.backend_platform_explicit) {
        const std::string compiled = CompiledPlatformBackendName();
        if (options.backend_platform != compiled) {
            throw std::runtime_error("Requested CLI platform backend '" + options.backend_platform +
                                     "' but this build only supports '" + compiled + "'.");
        }
        KARMA_TRACE("engine.app", "CLI option --backend-platform set: '{}'", options.backend_platform);
    }
    if (options.backend_render_explicit) {
        KARMA_TRACE("engine.app", "CLI option --backend-render set: '{}'", options.backend_render);
    }
    if (options.backend_ui_explicit) {
        KARMA_TRACE("engine.app", "CLI option --backend-ui set: '{}'", options.backend_ui);
    }
    if (options.backend_physics_explicit) {
        KARMA_TRACE("engine.app", "CLI option --backend-physics set: '{}'", options.backend_physics);
    }
    if (options.backend_audio_explicit) {
        KARMA_TRACE("engine.app", "CLI option --backend-audio set: '{}'", options.backend_audio);
    }

    const auto issues = karma::config::ValidateRequiredKeys(karma::config::ClientRequiredKeys());
    if (!issues.empty()) {
        for (const auto& issue : issues) {
            if (options.strict_config) {
                spdlog::error("config validation: {}: {}", issue.path, issue.message);
            } else {
                spdlog::warn("config validation: {}: {}", issue.path, issue.message);
            }
        }
        if (options.strict_config) {
            throw std::runtime_error("Client required config validation failed.");
        }
    }

    if (options.name_explicit) {
        KARMA_TRACE("engine.app", "CLI option --name set: '{}'", options.player_name);
    }
    if (options.addr_explicit) {
        KARMA_TRACE("engine.app", "CLI option --addr set: '{}'", options.connect_addr);
    }
    if (options.port_explicit) {
        KARMA_TRACE("engine.app", "CLI option --port set: {}", options.connect_port);
    }
    if (options.dev_quick_start) {
        KARMA_TRACE("engine.app", "CLI option --dev-quick-start parsed (not wired yet)");
    }
}

} // namespace bz3::client

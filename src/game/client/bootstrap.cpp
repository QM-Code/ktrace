#include "client/bootstrap.hpp"

#include "karma/app/backend_resolution.hpp"
#include "karma/app/bootstrap_scaffold.hpp"
#include "karma/common/config_validation.hpp"
#include "karma/common/logging.hpp"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <stdexcept>
#include <vector>

namespace bz3::client {
namespace {
}

void ConfigureLogging(const CLIOptions& options) {
    karma::app::ConfigureLoggingFromOptions(options.timestamp_logging,
                                            options.trace_explicit,
                                            options.trace_channels);
}

void ConfigureDataAndConfig(int argc, char** argv) {
    karma::app::BootstrapConfigSpec spec{};
    spec.app_name = "bz3";
    spec.data_dir_env_var = "BZ3_DATA_DIR";
    spec.required_data_marker = "client/config.json";
    spec.default_user_config_relative = std::filesystem::path("config.json");
    spec.config_specs = {
        {"client/config.json", "data/client/config.json", spdlog::level::debug, false, true}
    };
    karma::app::ConfigureDataAndConfigFromSpec(spec, argc, argv);
}

void ApplyRuntimeOptionOverrides(const CLIOptions& options) {
    if (options.backend_platform_explicit) {
        karma::app::ValidatePlatformBackendFromOption(options.backend_platform,
                                                      options.backend_platform_explicit);
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
    if (!karma::app::ReportRequiredConfigIssues(issues, options.strict_config)) {
        throw std::runtime_error("Client required config validation failed.");
    }

    if (options.name_explicit) {
        KARMA_TRACE("engine.app", "CLI option --name set: '{}'", options.player_name);
    }
    if (options.addr_explicit) {
        KARMA_TRACE("engine.app", "CLI option --addr set: '{}'", options.connect_addr);
    }
    if (options.dev_quick_start) {
        KARMA_TRACE("engine.app", "CLI option --dev-quick-start parsed (not wired yet)");
    }
    if (options.community_list_active_explicit) {
        KARMA_TRACE("engine.app", "CLI option --community-list-active set: '{}'", options.community_list_active);
    }
}

} // namespace bz3::client

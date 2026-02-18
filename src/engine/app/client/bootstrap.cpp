#include "karma/app/client/bootstrap.hpp"

#include "karma/app/client/backend_resolution.hpp"
#include "karma/app/shared/bootstrap.hpp"
#include "karma/common/config/validation.hpp"
#include "karma/common/logging/logging.hpp"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <string>
#include <stdexcept>

namespace karma::app::client {

void RunBootstrap(const karma::cli::client::AppOptions& options,
                  int argc,
                  char** argv,
                  std::string_view app_name) {
    shared::ConfigureLoggingFromOptions(options.timestamp_logging,
                                        options.trace_explicit,
                                        options.trace_channels);

    shared::BootstrapConfigSpec spec{};
    spec.app_name = app_name.empty() ? std::string("app") : std::string(app_name);
    spec.data_dir_env_var = "BZ3_DATA_DIR";
    spec.required_data_marker = "client/config.json";
    spec.default_user_config_relative = std::filesystem::path("config.json");
    spec.config_specs = {
        {"client/config.json", "data/client/config.json", spdlog::level::debug, false, true}
    };
    shared::ConfigureDataAndConfigFromSpec(spec, argc, argv);

    if (options.backend_window_explicit) {
        ValidateWindowBackendFromOption(options.backend_window, options.backend_window_explicit);
        KARMA_TRACE("engine.app", "CLI option --backend-window set: '{}'", options.backend_window);
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

    const auto issues = common::config::ValidateRequiredKeys(common::config::ClientRequiredKeys());
    if (!shared::ReportRequiredConfigIssues(issues, options.strict_config)) {
        throw std::runtime_error("Client required config validation failed.");
    }

    if (options.username_explicit) {
        KARMA_TRACE("engine.app", "CLI option --username set: '{}'", options.username);
    }
    if (options.password_explicit) {
        KARMA_TRACE("engine.app", "CLI option --password set: [redacted]");
    }
    if (options.server_explicit) {
        KARMA_TRACE("engine.app", "CLI option --server set: '{}'", options.server);
    }
}

} // namespace karma::app::client

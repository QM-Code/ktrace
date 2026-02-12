#include "server/bootstrap.hpp"

#include "karma/app/bootstrap_scaffold.hpp"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <vector>

namespace bz3::server {

void ConfigureLogging(const CLIOptions& options) {
    karma::app::ConfigureLoggingFromOptions(options.timestamp_logging,
                                            options.verbose,
                                            options.trace_explicit,
                                            options.trace_channels);
}

void ConfigureDataAndConfig(int argc, char** argv) {
    karma::app::BootstrapConfigSpec spec{};
    spec.app_name = "bz3";
    spec.data_dir_env_var = "BZ3_DATA_DIR";
    spec.required_data_marker = "common/config.json";
    spec.default_user_config_relative = std::filesystem::path("server/config.json");
    spec.config_specs = {
        {"common/config.json", "data/common/config.json", spdlog::level::err, true, true},
        {"server/config.json", "data/server/config.json", spdlog::level::err, true, true}
    };
    karma::app::ConfigureDataAndConfigFromSpec(spec, argc, argv);
}

} // namespace bz3::server

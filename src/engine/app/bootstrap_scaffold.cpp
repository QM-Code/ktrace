#include "karma/app/bootstrap_scaffold.hpp"

#include "karma/common/data_dir_override.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "karma/common/logging.hpp"

#include <spdlog/spdlog.h>

namespace karma::app {

void ConfigureLoggingFromOptions(bool timestamp_logging,
                                 bool verbose,
                                 bool trace_explicit,
                                 const std::string& trace_channels) {
    logging::ConfigureLogPatterns(timestamp_logging);
    spdlog::set_level(verbose ? spdlog::level::debug : spdlog::level::info);
    if (trace_explicit) {
        logging::EnableTraceChannels(trace_channels);
    }
}

void ConfigureDataAndConfigFromSpec(const BootstrapConfigSpec& spec, int argc, char** argv) {
    data::DataPathSpec data_spec{};
    data_spec.appName = spec.app_name;
    data_spec.dataDirEnvVar = spec.data_dir_env_var;
    data_spec.requiredDataMarker = spec.required_data_marker;
    data::SetDataPathSpec(data_spec);

    const auto data_dir_result =
        data::ApplyDataDirOverrideFromArgs(argc, argv, spec.default_user_config_relative);
    config::ConfigStore::Initialize(spec.config_specs, data_dir_result.userConfigPath);
}

bool ReportRequiredConfigIssues(const std::vector<config::ValidationIssue>& issues, bool strict_config) {
    if (issues.empty()) {
        return true;
    }

    for (const auto& issue : issues) {
        if (strict_config) {
            spdlog::error("config validation: {}: {}", issue.path, issue.message);
        } else {
            spdlog::warn("config validation: {}: {}", issue.path, issue.message);
        }
    }

    return !strict_config;
}

} // namespace karma::app

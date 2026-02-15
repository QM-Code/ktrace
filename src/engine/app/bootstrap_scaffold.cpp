#include "karma/app/bootstrap_scaffold.hpp"

#include "karma/app/cli_parse_scaffold.hpp"
#include "karma/common/data_dir_override.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "karma/common/logging.hpp"

#include <spdlog/spdlog.h>

#include <optional>
#include <stdexcept>

namespace karma::app {

namespace {

CliCommonState ParseCommonCliState(int argc, char** argv) {
    CliCommonState state{};
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto result = ConsumeCommonCliOption(arg, i, argc, argv, state);
        if (result.consumed && !result.error.empty()) {
            throw std::runtime_error(result.error);
        }
    }
    return state;
}

void ApplyCommonCliConfigOverrides(const CliCommonState& state) {
    if (!state.language_explicit) {
        return;
    }
    karma::json::Value overlay = karma::json::Object();
    overlay["language"] = state.language;
    if (!config::ConfigStore::AddRuntimeLayer("cli overrides", overlay, {})) {
        throw std::runtime_error("Failed to apply CLI language override.");
    }
    KARMA_TRACE("config", "Applied CLI language override: {}", state.language);
}

} // namespace

void ConfigureLoggingFromOptions(bool timestamp_logging,
                                 bool trace_explicit,
                                 const std::string& trace_channels) {
    logging::ConfigureLogPatterns(timestamp_logging);
    // Default to debug-level logs globally; trace remains opt-in by channel.
    spdlog::set_level(spdlog::level::debug);
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
        data::ApplyDataDirOverrideFromArgs(argc,
                                           argv,
                                           spec.default_user_config_relative,
                                           spec.enable_user_config,
                                           spec.allow_user_config_data_dir_when_user_config_disabled);
    const auto user_config_path =
        spec.enable_user_config ? std::optional<std::filesystem::path>(data_dir_result.userConfigPath)
                                : std::nullopt;
    config::ConfigStore::Initialize(spec.config_specs, user_config_path);
    ApplyCommonCliConfigOverrides(ParseCommonCliState(argc, argv));
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

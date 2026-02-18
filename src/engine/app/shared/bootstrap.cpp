#include "karma/app/shared/bootstrap.hpp"

#include "karma/cli/shared/parse.hpp"
#include "karma/common/config/helpers.hpp"
#include "karma/common/data/directory_override.hpp"
#include "karma/common/data/path_resolver.hpp"
#include "karma/common/logging/logging.hpp"

#include <spdlog/spdlog.h>

#include <optional>
#include <stdexcept>

namespace karma::app::shared {

namespace {

karma::cli::shared::CommonState ParseCommonCliState(int argc, char** argv) {
    karma::cli::shared::CommonState state{};
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto result = karma::cli::shared::ConsumeCommonOption(arg, i, argc, argv, state);
        if (result.consumed && !result.error.empty()) {
            throw std::runtime_error(result.error);
        }
    }
    return state;
}

void ApplyCommonCliConfigOverrides(const karma::cli::shared::CommonState& state) {
    if (!state.language_explicit) {
        return;
    }
    karma::common::serialization::Value overlay = karma::common::serialization::Object();
    overlay["language"] = state.language;
    if (!common::config::ConfigStore::AddRuntimeLayer("cli overrides", overlay, {})) {
        throw std::runtime_error("Failed to apply CLI language override.");
    }
    KARMA_TRACE("config", "Applied CLI language override: {}", state.language);
}

} // namespace

void ConfigureLoggingFromOptions(bool timestamp_logging,
                                 bool trace_explicit,
                                 const std::string& trace_channels) {
    common::logging::ConfigureLogPatterns(timestamp_logging);
    // Default to debug-level logs globally; trace remains opt-in by channel.
    spdlog::set_level(spdlog::level::debug);
    if (trace_explicit) {
        common::logging::EnableTraceChannels(trace_channels);
    }
}

void ConfigureDataAndConfigFromSpec(const BootstrapConfigSpec& spec, int argc, char** argv) {
    common::data::DataPathSpec data_spec{};
    data_spec.appName = spec.app_name;
    data_spec.dataDirEnvVar = spec.data_dir_env_var;
    data_spec.requiredDataMarker = spec.required_data_marker;
    common::data::SetDataPathSpec(data_spec);

    const auto data_directory_result =
        common::data::ApplyDataDirectoryOverrideFromArgs(
            argc,
            argv,
            spec.default_user_config_relative,
            spec.enable_user_config,
            spec.allow_user_config_data_dir_when_user_config_disabled);
    const auto user_config_path =
        spec.enable_user_config ? std::optional<std::filesystem::path>(data_directory_result.userConfigPath)
                                : std::nullopt;
    common::config::ConfigStore::Initialize(spec.config_specs, user_config_path);
    ApplyCommonCliConfigOverrides(ParseCommonCliState(argc, argv));
}

bool ReportRequiredConfigIssues(const std::vector<common::config::ValidationIssue>& issues, bool strict_config) {
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

std::string ResolveConfiguredAppName(const std::string& fallback_name) {
    const std::string fallback = fallback_name.empty() ? std::string("app") : fallback_name;
    const std::string configured = common::config::ReadStringConfig("app.name", fallback);
    return configured.empty() ? fallback : configured;
}

} // namespace karma::app::shared

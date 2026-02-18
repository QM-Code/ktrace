#pragma once

#include "karma/common/config/store.hpp"
#include "karma/common/config/validation.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace karma::app::shared {

struct BootstrapConfigSpec {
    std::string app_name;
    std::string data_dir_env_var;
    std::string required_data_marker;
    bool enable_user_config = true;
    bool allow_user_config_data_dir_when_user_config_disabled = false;
    std::filesystem::path default_user_config_relative = std::filesystem::path("config.json");
    std::vector<common::config::ConfigFileSpec> config_specs{};
};

void ConfigureLoggingFromOptions(bool timestamp_logging,
                                 bool trace_explicit,
                                 const std::string& trace_channels);

void ConfigureDataAndConfigFromSpec(const BootstrapConfigSpec& spec, int argc, char** argv);

bool ReportRequiredConfigIssues(const std::vector<common::config::ValidationIssue>& issues, bool strict_config);
std::string ResolveConfiguredAppName(const std::string& fallback_name);

} // namespace karma::app::shared

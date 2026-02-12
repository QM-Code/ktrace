#pragma once

#include "karma/common/config_store.hpp"
#include "karma/common/config_validation.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace karma::app {

struct BootstrapConfigSpec {
    std::string app_name;
    std::string data_dir_env_var;
    std::string required_data_marker;
    std::filesystem::path default_user_config_relative = std::filesystem::path("config.json");
    std::vector<config::ConfigFileSpec> config_specs{};
};

void ConfigureLoggingFromOptions(bool timestamp_logging,
                                 bool verbose,
                                 bool trace_explicit,
                                 const std::string& trace_channels);

void ConfigureDataAndConfigFromSpec(const BootstrapConfigSpec& spec, int argc, char** argv);

bool ReportRequiredConfigIssues(const std::vector<config::ValidationIssue>& issues, bool strict_config);

} // namespace karma::app

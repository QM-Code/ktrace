#include "client/bootstrap.hpp"

#include "karma/common/config_store.hpp"
#include "karma/common/data_dir_override.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "karma/common/logging.hpp"

#include <spdlog/spdlog.h>

#include <vector>

namespace bz3::client {

void ConfigureLogging(const CLIOptions& options) {
    karma::logging::ConfigureLogPatterns(false);
    spdlog::set_level(options.verbose ? spdlog::level::debug : spdlog::level::info);
    if (options.trace_explicit) {
        karma::logging::EnableTraceChannels(options.trace_channels);
    }
}

void ConfigureDataAndConfig(int argc, char** argv) {
    karma::data::DataPathSpec spec;
    spec.appName = "bz3";
    spec.dataDirEnvVar = "KARMA_DATA_DIR";
    spec.requiredDataMarker = "common/config.json";
    karma::data::SetDataPathSpec(spec);

    const auto dataDirResult = karma::data::ApplyDataDirOverrideFromArgs(argc, argv);
    const std::vector<karma::config::ConfigFileSpec> configSpecs = {
        {"common/config.json", "data/common/config.json", spdlog::level::err, true, true},
        {"client/config.json", "data/client/config.json", spdlog::level::debug, false, true}
    };
    karma::config::ConfigStore::Initialize(configSpecs, dataDirResult.userConfigPath);
}

} // namespace bz3::client

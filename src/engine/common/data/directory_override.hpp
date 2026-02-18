#pragma once

#include <filesystem>
#include <optional>

namespace karma::common::data {

struct DataDirectoryOverrideResult {
    std::filesystem::path userConfigPath;
    std::optional<std::filesystem::path> dataDir;
};

DataDirectoryOverrideResult ApplyDataDirectoryOverrideFromArgs(
    int argc,
    char* argv[],
    const std::filesystem::path& defaultConfigRelative = std::filesystem::path("config.json"),
    bool enableUserConfig = true,
    bool allowDataDirFromUserConfigWhenUserConfigDisabled = false);

} // namespace karma::common::data

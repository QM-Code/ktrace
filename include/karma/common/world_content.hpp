#pragma once

#include "karma/common/content/catalog.hpp"

namespace karma::data {
struct ConfigLayerSpec;
}

namespace world {

using ArchiveBytes = karma::content::ArchiveBytes;
using AssetCatalog = karma::content::AssetCatalog;
using WorldContent = karma::content::WorldContent;

WorldContent LoadWorldContent(const std::vector<karma::data::ConfigLayerSpec>& baseSpecs,
                              const std::optional<karma::json::Value>& worldConfig,
                              const std::filesystem::path& worldDir,
                              const std::string& fallbackName,
                              const std::string& logContext);

} // namespace world

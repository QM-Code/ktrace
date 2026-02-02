#pragma once

#include "karma/common/json.hpp"

#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace karma::data {
struct ConfigLayerSpec;
}

namespace world {

using ArchiveBytes = std::vector<std::byte>;

struct AssetCatalog {
    std::map<std::string, std::filesystem::path> entries;

    void mergeFromJson(const karma::json::Value& assetsJson, const std::filesystem::path& baseDir);
    std::filesystem::path resolvePath(const std::string& key, const char* logContext) const;
    std::optional<std::filesystem::path> findPath(const std::string& key) const;
};

struct WorldContent {
    std::string name;
    std::filesystem::path rootDir;
    karma::json::Value config;
    AssetCatalog assets;

    void mergeLayer(const karma::json::Value& layerJson, const std::filesystem::path& baseDir);
    std::filesystem::path resolveAssetPath(const std::string& key, const char* logContext) const;
};

WorldContent LoadWorldContent(const std::vector<karma::data::ConfigLayerSpec>& baseSpecs,
                              const std::optional<karma::json::Value>& worldConfig,
                              const std::filesystem::path& worldDir,
                              const std::string& fallbackName,
                              const std::string& logContext);

} // namespace world

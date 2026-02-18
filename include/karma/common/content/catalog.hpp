#pragma once

#include "karma/common/content/types.hpp"
#include "karma/common/serialization/json.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace karma::common::data {
struct ConfigLayerSpec;
}

namespace karma::common::content {

struct AssetCatalog {
    std::map<std::string, std::filesystem::path> entries;

    void mergeFromJson(const karma::common::serialization::Value& assets_json, const std::filesystem::path& base_dir);
    std::filesystem::path resolvePath(const std::string& key, const char* log_context) const;
    std::optional<std::filesystem::path> findPath(const std::string& key) const;
};

struct WorldContent {
    std::string name;
    std::filesystem::path rootDir;
    karma::common::serialization::Value config;
    AssetCatalog assets;

    void mergeLayer(const karma::common::serialization::Value& layer_json, const std::filesystem::path& base_dir);
    std::filesystem::path resolveAssetPath(const std::string& key, const char* log_context) const;
};

WorldContent LoadWorldContent(const std::vector<karma::common::data::ConfigLayerSpec>& base_specs,
                              const std::optional<karma::common::serialization::Value>& world_config,
                              const std::filesystem::path& world_dir,
                              const std::string& fallback_name,
                              const std::string& log_context);

} // namespace karma::common::content

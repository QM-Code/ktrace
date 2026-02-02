#include "karma/common/world_content.hpp"

#include "karma/common/config_helpers.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "spdlog/spdlog.h"

namespace {
std::string LeafKey(const std::string& key) {
    const auto separator = key.find_last_of('.');
    return separator == std::string::npos ? key : key.substr(separator + 1);
}
}

namespace world {

void AssetCatalog::mergeFromJson(const karma::json::Value& assetsJson, const std::filesystem::path& baseDir) {
    if (!assetsJson.is_object()) {
        return;
    }

    std::map<std::string, std::filesystem::path> collected;
    karma::data::CollectAssetEntries(assetsJson, baseDir, collected);

    for (const auto& [key, path] : collected) {
        entries[key] = path;
        entries[LeafKey(key)] = path;
    }
}

std::optional<std::filesystem::path> AssetCatalog::findPath(const std::string& key) const {
    auto it = entries.find(key);
    if (it == entries.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::filesystem::path AssetCatalog::resolvePath(const std::string& key, const char* logContext) const {
    auto resolved = findPath(key);
    if (resolved.has_value()) {
        return resolved.value();
    }
    spdlog::error("{}: Asset '{}' not found", logContext, key);
    return {};
}

void WorldContent::mergeLayer(const karma::json::Value& layerJson, const std::filesystem::path& baseDir) {
    if (!layerJson.is_object()) {
        return;
    }

    const auto assetsIt = layerJson.find("assets");
    if (assetsIt != layerJson.end()) {
        if (!assetsIt->is_object()) {
            spdlog::warn("WorldContent: 'assets' in layer is not an object; skipping");
        } else {
            assets.mergeFromJson(*assetsIt, baseDir);
        }
    }
}

std::filesystem::path WorldContent::resolveAssetPath(const std::string& key, const char* logContext) const {
    return assets.resolvePath(key, logContext);
}

WorldContent LoadWorldContent(const std::vector<karma::data::ConfigLayerSpec>& baseSpecs,
                              const std::optional<karma::json::Value>& worldConfig,
                              const std::filesystem::path& worldDir,
                              const std::string& fallbackName,
                              const std::string& logContext) {
    WorldContent content;
    content.rootDir = worldDir;
    content.name = fallbackName;

    std::vector<karma::data::ConfigLayer> layers = karma::data::LoadConfigLayers(baseSpecs);
    if (worldConfig.has_value()) {
        if (worldConfig->is_object()) {
            layers.push_back({*worldConfig, worldDir});
        } else {
            spdlog::warn("{}: World config for {} is not an object", logContext, worldDir.string());
        }
    }

    karma::json::Value mergedConfig = karma::json::Object();
    for (const auto& layer : layers) {
        karma::data::MergeJsonObjects(mergedConfig, layer.json);
        content.mergeLayer(layer.json, layer.baseDir);
    }

    content.config = std::move(mergedConfig);
    if (content.name.empty()) {
        content.name = std::filesystem::path(content.rootDir).filename().string();
    }
    spdlog::info("{}: Loaded world '{}'", logContext, content.name);
    return content;
}

} // namespace world

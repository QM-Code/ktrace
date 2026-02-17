#include "karma/common/content/catalog.hpp"

#include "karma/common/config_helpers.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "karma/common/logging.hpp"
#include "spdlog/spdlog.h"

namespace {
std::string LeafKey(const std::string& key) {
    const auto separator = key.find_last_of('.');
    return separator == std::string::npos ? key : key.substr(separator + 1);
}
} // namespace

namespace karma::content {

void AssetCatalog::mergeFromJson(const karma::json::Value& assets_json,
                                 const std::filesystem::path& base_dir) {
    if (!assets_json.is_object()) {
        return;
    }

    std::map<std::string, std::filesystem::path> collected;
    karma::data::CollectAssetEntries(assets_json, base_dir, collected);

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

std::filesystem::path AssetCatalog::resolvePath(const std::string& key, const char* log_context) const {
    auto resolved = findPath(key);
    if (resolved.has_value()) {
        return resolved.value();
    }
    spdlog::error("{}: Asset '{}' not found", log_context, key);
    return {};
}

void WorldContent::mergeLayer(const karma::json::Value& layer_json,
                              const std::filesystem::path& base_dir) {
    if (!layer_json.is_object()) {
        return;
    }

    const auto assets_it = layer_json.find("assets");
    if (assets_it != layer_json.end()) {
        if (!assets_it->is_object()) {
            spdlog::warn("WorldContent: 'assets' in layer is not an object; skipping");
        } else {
            assets.mergeFromJson(*assets_it, base_dir);
        }
    }
}

std::filesystem::path WorldContent::resolveAssetPath(const std::string& key, const char* log_context) const {
    return assets.resolvePath(key, log_context);
}

WorldContent LoadWorldContent(const std::vector<karma::data::ConfigLayerSpec>& base_specs,
                              const std::optional<karma::json::Value>& world_config,
                              const std::filesystem::path& world_dir,
                              const std::string& fallback_name,
                              const std::string& log_context) {
    WorldContent content;
    content.rootDir = world_dir;
    content.name = fallback_name;

    std::vector<karma::data::ConfigLayer> layers = karma::data::LoadConfigLayers(base_specs);
    if (world_config.has_value()) {
        if (world_config->is_object()) {
            layers.push_back({*world_config, world_dir});
        } else {
            spdlog::warn("{}: World config for {} is not an object", log_context, world_dir.string());
        }
    }

    karma::json::Value merged_config = karma::json::Object();
    for (const auto& layer : layers) {
        karma::data::MergeJsonObjects(merged_config, layer.json);
        content.mergeLayer(layer.json, layer.baseDir);
    }

    content.config = std::move(merged_config);
    if (content.name.empty()) {
        content.name = std::filesystem::path(content.rootDir).filename().string();
    }
    KARMA_TRACE("world", "{}: Loaded world '{}'", log_context, content.name);
    return content;
}

} // namespace karma::content

#include "common/config_store.hpp"

#include "common/data_path_resolver.hpp"
#include "karma/common/logging.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <fstream>
#include <mutex>
#include <optional>
#include <unordered_set>
#include <spdlog/spdlog.h>

namespace {

struct ConfigStoreState {
    std::mutex mutex;
    bool initialized = false;
    uint64_t revision = 0;
    std::vector<karma::config::ConfigLayer> defaultLayers;
    std::optional<karma::config::ConfigLayer> userLayer;
    std::vector<karma::config::ConfigLayer> runtimeLayers;
    karma::json::Value defaults = karma::json::Object();
    karma::json::Value user = karma::json::Object();
    karma::json::Value merged = karma::json::Object();
    std::unordered_map<std::string, std::filesystem::path> assetLookup;
    std::unordered_map<std::string, std::pair<int, std::size_t>> labelIndex;
    std::filesystem::path userConfigPath;
    double saveIntervalSeconds = 0.0;
    double mergeIntervalSeconds = 0.0;
    std::chrono::steady_clock::time_point lastSaveTime = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point lastMergeTime = std::chrono::steady_clock::now();
    uint64_t lastSavedRevision = 0;
    bool pendingSave = false;
    bool mergedDirty = false;
};

ConfigStoreState g_state;

std::filesystem::path TryCanonical(const std::filesystem::path &path) {
    std::error_code ec;
    auto result = std::filesystem::weakly_canonical(path, ec);
    if (!ec) {
        return result;
    }
    result = std::filesystem::absolute(path, ec);
    if (!ec) {
        return result;
    }
    return path;
}

std::filesystem::path ResolveWithBase(const std::filesystem::path &baseDir, const std::string &value) {
    std::filesystem::path candidate(value);
    if (!candidate.is_absolute()) {
        candidate = baseDir / candidate;
    }
    return TryCanonical(candidate);
}

const karma::json::Value *resolvePath(const karma::json::Value &root, std::string_view path) {
    if (path.empty()) {
        return &root;
    }

    const karma::json::Value *current = &root;
    std::size_t position = 0;

    while (position < path.size()) {
        const std::size_t dot = path.find('.', position);
        const bool lastSegment = (dot == std::string_view::npos);
        const std::string segment(path.substr(position, lastSegment ? std::string_view::npos : dot - position));
        if (segment.empty()) {
            return nullptr;
        }

        std::string key = segment;
        std::optional<std::size_t> arrayIndex;
        const auto bracketPos = segment.find('[');
        if (bracketPos != std::string::npos) {
            key = segment.substr(0, bracketPos);
            const auto closingPos = segment.find(']', bracketPos);
            if (closingPos == std::string::npos || closingPos != segment.size() - 1) {
                return nullptr;
            }
            const std::string indexText = segment.substr(bracketPos + 1, closingPos - bracketPos - 1);
            if (indexText.empty()) {
                return nullptr;
            }
            try {
                arrayIndex = static_cast<std::size_t>(std::stoul(indexText));
            } catch (...) {
                return nullptr;
            }
        }

        if (!key.empty()) {
            if (!current->is_object()) {
                return nullptr;
            }
            const auto it = current->find(key);
            if (it == current->end()) {
                return nullptr;
            }
            current = &(*it);
        }

        if (arrayIndex.has_value()) {
            if (!current->is_array() || *arrayIndex >= current->size()) {
                return nullptr;
            }
            current = &((*current)[*arrayIndex]);
        }

        if (lastSegment) {
            break;
        }

        position = dot + 1;
    }

    return current;
}

bool parsePathSegments(std::string_view path,
                       std::vector<std::pair<std::string, std::optional<std::size_t>>> &out) {
    out.clear();
    if (path.empty()) {
        return false;
    }
    std::size_t position = 0;
    while (position < path.size()) {
        const std::size_t dot = path.find('.', position);
        const bool lastSegment = (dot == std::string_view::npos);
        const std::string segment(path.substr(position, lastSegment ? std::string_view::npos : dot - position));
        if (segment.empty()) {
            return false;
        }
        std::string key = segment;
        std::optional<std::size_t> index;
        const auto bracketPos = segment.find('[');
        if (bracketPos != std::string::npos) {
            key = segment.substr(0, bracketPos);
            const auto closingPos = segment.find(']', bracketPos);
            if (closingPos == std::string::npos || closingPos != segment.size() - 1) {
                return false;
            }
            const std::string indexText = segment.substr(bracketPos + 1, closingPos - bracketPos - 1);
            if (indexText.empty()) {
                return false;
            }
            try {
                index = static_cast<std::size_t>(std::stoul(indexText));
            } catch (...) {
                return false;
            }
        }
        out.emplace_back(std::move(key), index);
        if (lastSegment) {
            break;
        }
        position = dot + 1;
    }
    return !out.empty();
}

void mergeJsonObjects(karma::json::Value &destination, const karma::json::Value &source) {
    if (!destination.is_object() || !source.is_object()) {
        destination = source;
        return;
    }
    for (auto it = source.begin(); it != source.end(); ++it) {
        const auto &key = it.key();
        const auto &value = it.value();
        if (value.is_object() && destination.contains(key) && destination[key].is_object()) {
            mergeJsonObjects(destination[key], value);
        } else {
            destination[key] = value;
        }
    }
}

void roundFloatValues(karma::json::Value &node) {
    if (node.is_object()) {
        for (auto it = node.begin(); it != node.end(); ++it) {
            roundFloatValues(it.value());
        }
        return;
    }
    if (node.is_array()) {
        for (auto &entry : node) {
            roundFloatValues(entry);
        }
        return;
    }
    if (node.is_number_float()) {
        const double value = node.get<double>();
        const double rounded = std::round(value * 100.0) / 100.0;
        node = rounded;
    }
}

double readIntervalSeconds(const karma::json::Value &root, std::string_view path, double fallback) {
    const auto *value = resolvePath(root, path);
    if (!value || !value->is_number()) {
        spdlog::error("config_store: Missing numeric config '{}'", path);
        return fallback;
    }
    const double seconds = value->get<double>();
    if (seconds < 0.0) {
        spdlog::warn("config_store: Clamping '{}' to 0.0 (was {})", path, seconds);
        return 0.0;
    }
    return seconds;
}

void collectAssetEntries(const karma::json::Value &node,
                         const std::filesystem::path &baseDir,
                         std::unordered_map<std::string, std::filesystem::path> &assetMap,
                         const std::string &prefix = "") {
    if (!node.is_object()) {
        return;
    }
    for (const auto &[key, value] : node.items()) {
        const std::string fullKey = prefix.empty() ? key : prefix + "." + key;
        if (value.is_string()) {
            assetMap[fullKey] = ResolveWithBase(baseDir, value.get<std::string>());
        } else if (value.is_object()) {
            collectAssetEntries(value, baseDir, assetMap, fullKey);
        }
    }
}

std::unordered_map<std::string, std::filesystem::path> buildAssetLookup(
    const std::vector<karma::config::ConfigLayer> &layers) {
    std::unordered_map<std::string, std::filesystem::path> lookup;
    for (const auto &layer : layers) {
        if (!layer.json.is_object()) {
            continue;
        }
        const auto assetsIt = layer.json.find("assets");
        if (assetsIt != layer.json.end() && assetsIt->is_object()) {
            collectAssetEntries(*assetsIt, layer.baseDir, lookup);
        }
        const auto fontsIt = layer.json.find("fonts");
        if (fontsIt != layer.json.end() && fontsIt->is_object()) {
            collectAssetEntries(*fontsIt, layer.baseDir, lookup, "fonts");
        }
    }

    std::unordered_map<std::string, std::filesystem::path> expanded = lookup;
    for (const auto &[key, resolvedPath] : lookup) {
        const auto separator = key.find_last_of('.');
        if (separator != std::string::npos) {
            expanded[key.substr(separator + 1)] = resolvedPath;
        }
    }
    return expanded;
}

std::vector<karma::config::ConfigLayer> loadLayers(const std::vector<karma::config::ConfigFileSpec> &specs) {
    std::vector<karma::config::ConfigLayer> layers;
    layers.reserve(specs.size());
    for (const auto &spec : specs) {
        std::filesystem::path path = spec.path;
        if (spec.resolveRelativeToDataRoot && path.is_relative()) {
            path = karma::data::Resolve(path);
        }
        path = TryCanonical(path);
        const std::string label = spec.label.empty() ? path.string() : spec.label;
        KARMA_TRACE("config", "config_store: loading config file '{}' (label: {})", path.string(), label);
        auto jsonOpt = karma::data::LoadJsonFile(path, label, spec.missingLevel);
        if (!jsonOpt) {
            if (spec.required) {
                spdlog::error("config_store: Required config missing: {}", path.string());
            }
            continue;
        }
        if (!jsonOpt->is_object()) {
            spdlog::warn("config_store: Config {} is not a JSON object, skipping", path.string());
            continue;
        }
        layers.push_back({std::move(*jsonOpt), path.parent_path(), label});
    }
    return layers;
}

}

namespace karma::config {

void ConfigStore::Initialize(const std::vector<ConfigFileSpec> &defaultSpecs,
                             const std::filesystem::path &userConfigPath,
                             const std::vector<ConfigFileSpec> &runtimeSpecs) {
    std::vector<ConfigFileSpec> combinedDefaults = defaultSpecs;

    std::vector<ConfigLayer> defaults = loadLayers(combinedDefaults);
    std::vector<ConfigLayer> runtime = loadLayers(runtimeSpecs);

    const std::filesystem::path resolvedUserPath = userConfigPath.empty()
        ? karma::data::EnsureUserConfigFile("config.json")
        : TryCanonical(userConfigPath);

    karma::json::Value userJson = karma::json::Object();
    KARMA_TRACE("config", "config_store: loading user config '{}'", resolvedUserPath.string());
    if (auto userOpt = karma::data::LoadJsonFile(resolvedUserPath, "user config", spdlog::level::debug)) {
        if (userOpt->is_object()) {
            userJson = std::move(*userOpt);
        } else {
            spdlog::warn("config_store: User config {} is not a JSON object", resolvedUserPath.string());
        }
    }

    karma::json::Value defaultsMerged = karma::json::Object();
    for (const auto &layer : defaults) {
        mergeJsonObjects(defaultsMerged, layer.json);
    }

    std::optional<ConfigLayer> userLayer;
    if (userJson.is_object()) {
        userLayer = ConfigLayer{userJson, resolvedUserPath.parent_path(), "user config"};
    }

    std::lock_guard<std::mutex> lock(g_state.mutex);
    g_state.defaultLayers = std::move(defaults);
    g_state.runtimeLayers = std::move(runtime);
    g_state.userLayer = std::move(userLayer);
    g_state.defaults = std::move(defaultsMerged);
    g_state.user = userJson;
    g_state.userConfigPath = resolvedUserPath;
    g_state.saveIntervalSeconds = readIntervalSeconds(g_state.defaults, "config.SaveIntervalSeconds", 0.0);
    g_state.mergeIntervalSeconds = readIntervalSeconds(g_state.defaults, "config.MergeIntervalSeconds", 0.0);
    g_state.lastSaveTime = std::chrono::steady_clock::now();
    g_state.lastMergeTime = g_state.lastSaveTime;
    g_state.lastSavedRevision = 0;
    g_state.pendingSave = false;
    g_state.mergedDirty = false;
    rebuildMergedLocked();
    g_state.revision++;
    g_state.lastSavedRevision = g_state.revision;
    g_state.initialized = true;
}

bool ConfigStore::Initialized() {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    return g_state.initialized;
}

uint64_t ConfigStore::Revision() {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    return g_state.revision;
}

const karma::json::Value &ConfigStore::Defaults() {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    return g_state.defaults;
}

const karma::json::Value &ConfigStore::User() {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    return g_state.user;
}

const karma::json::Value &ConfigStore::Merged() {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    return g_state.merged;
}

const karma::json::Value *ConfigStore::Get(std::string_view path) {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    if (!g_state.initialized) {
        return nullptr;
    }
    if (g_state.mergedDirty) {
        const auto now = std::chrono::steady_clock::now();
        if (g_state.mergeIntervalSeconds <= 0.0 ||
            std::chrono::duration<double>(now - g_state.lastMergeTime).count() >= g_state.mergeIntervalSeconds) {
            rebuildMergedLocked();
        }
    }
    if (g_state.pendingSave) {
        const auto now = std::chrono::steady_clock::now();
        if (g_state.saveIntervalSeconds <= 0.0 ||
            std::chrono::duration<double>(now - g_state.lastSaveTime).count() >= g_state.saveIntervalSeconds) {
            saveUserUnlocked(nullptr, true);
        }
    }
    KARMA_TRACE("config", "config_store: request for key '{}'", path);
    return resolvePath(g_state.merged, path);
}

std::optional<karma::json::Value> ConfigStore::GetCopy(std::string_view path) {
    if (const auto *value = Get(path)) {
        return std::optional<karma::json::Value>(std::in_place, *value);
    }
    return std::nullopt;
}

bool ConfigStore::Set(std::string_view path, karma::json::Value value) {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    if (!g_state.initialized) {
        return false;
    }
    KARMA_TRACE("config", "config_store: writing key '{}'", path);
    if (!setValueAtPath(g_state.user, path, std::move(value))) {
        return false;
    }
    g_state.userLayer = ConfigLayer{g_state.user, g_state.userConfigPath.parent_path(), "user config"};
    g_state.revision++;
    g_state.mergedDirty = true;
    if (g_state.mergeIntervalSeconds <= 0.0) {
        rebuildMergedLocked();
    }
    g_state.pendingSave = true;
    if (g_state.saveIntervalSeconds <= 0.0) {
        return saveUserUnlocked(nullptr, true);
    }
    return true;
}

bool ConfigStore::Erase(std::string_view path) {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    if (!g_state.initialized) {
        return false;
    }
    KARMA_TRACE("config", "config_store: erasing key '{}'", path);
    if (!eraseValueAtPath(g_state.user, path)) {
        return false;
    }
    g_state.userLayer = ConfigLayer{g_state.user, g_state.userConfigPath.parent_path(), "user config"};
    g_state.revision++;
    g_state.mergedDirty = true;
    if (g_state.mergeIntervalSeconds <= 0.0) {
        rebuildMergedLocked();
    }
    g_state.pendingSave = true;
    if (g_state.saveIntervalSeconds <= 0.0) {
        return saveUserUnlocked(nullptr, true);
    }
    return true;
}

bool ConfigStore::ReplaceUserConfig(karma::json::Value userConfig, std::string *error) {
    if (!userConfig.is_object()) {
        userConfig = karma::json::Object();
    }
    std::lock_guard<std::mutex> lock(g_state.mutex);
    if (!g_state.initialized) {
        return false;
    }
    KARMA_TRACE("config", "config_store: replacing entire user config");
    g_state.user = std::move(userConfig);
    g_state.userLayer = ConfigLayer{g_state.user, g_state.userConfigPath.parent_path(), "user config"};
    g_state.revision++;
    g_state.mergedDirty = true;
    if (g_state.mergeIntervalSeconds <= 0.0) {
        rebuildMergedLocked();
    }
    g_state.pendingSave = true;
    if (g_state.saveIntervalSeconds <= 0.0) {
        return saveUserUnlocked(error, true);
    }
    return true;
}

bool ConfigStore::SaveUser(std::string *error) {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    if (!g_state.initialized) {
        if (error) {
            *error = "Config store not initialized.";
        }
        return false;
    }
    return saveUserUnlocked(error, true);
}

void ConfigStore::Tick() {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    if (!g_state.initialized) {
        return;
    }
    if (!g_state.pendingSave) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (g_state.saveIntervalSeconds > 0.0 &&
        std::chrono::duration<double>(now - g_state.lastSaveTime).count() < g_state.saveIntervalSeconds) {
        return;
    }
    saveUserUnlocked(nullptr, true);
}

bool ConfigStore::saveUserUnlocked(std::string *error, bool ignoreInterval) {
    const auto now = std::chrono::steady_clock::now();
    if (g_state.revision <= g_state.lastSavedRevision) {
        g_state.pendingSave = false;
        return true;
    }
    if (!ignoreInterval && g_state.saveIntervalSeconds > 0.0 &&
        std::chrono::duration<double>(now - g_state.lastSaveTime).count() < g_state.saveIntervalSeconds) {
        g_state.pendingSave = true;
        return true;
    }

    const std::filesystem::path path = g_state.userConfigPath.empty()
        ? karma::data::EnsureUserConfigFile("config.json")
        : g_state.userConfigPath;

    std::error_code ec;
    const auto parentDir = path.parent_path();
    if (!parentDir.empty()) {
        std::filesystem::create_directories(parentDir, ec);
        if (ec) {
            if (error) {
                *error = "Failed to create config directory.";
            }
            return false;
        }
    }

    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        if (error) {
            *error = "Failed to open user config for writing.";
        }
        return false;
    }

    try {
        karma::json::Value rounded = g_state.user;
        roundFloatValues(rounded);
        KARMA_TRACE("config", "config_store: writing user config '{}'", path.string());
        file << rounded.dump(4) << '\n';
    } catch (const std::exception &ex) {
        if (error) {
            *error = ex.what();
        }
        return false;
    }
    g_state.lastSaveTime = now;
    g_state.lastSavedRevision = g_state.revision;
    g_state.pendingSave = false;
    return true;
}

const std::filesystem::path &ConfigStore::UserConfigPath() {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    return g_state.userConfigPath;
}

bool ConfigStore::AddRuntimeLayer(const std::string &label,
                                  const karma::json::Value &layerJson,
                                  const std::filesystem::path &baseDir) {
    if (!layerJson.is_object()) {
        spdlog::warn("config_store: Runtime layer '{}' ignored because it is not a JSON object", label);
        return false;
    }
    std::lock_guard<std::mutex> lock(g_state.mutex);
    if (!g_state.initialized) {
        return false;
    }
    const std::string resolvedLabel = label.empty() ? baseDir.string() : label;
    for (auto &layer : g_state.runtimeLayers) {
        if (layer.label == resolvedLabel) {
            layer.json = layerJson;
            layer.baseDir = baseDir;
            g_state.revision++;
            rebuildMergedLocked();
            return true;
        }
    }
    g_state.runtimeLayers.push_back({layerJson, baseDir, resolvedLabel});
    g_state.revision++;
    rebuildMergedLocked();
    return true;
}

bool ConfigStore::RemoveRuntimeLayer(const std::string &label) {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    if (!g_state.initialized) {
        return false;
    }
    const auto before = g_state.runtimeLayers.size();
    g_state.runtimeLayers.erase(
        std::remove_if(g_state.runtimeLayers.begin(), g_state.runtimeLayers.end(),
                       [&](const ConfigLayer &layer) { return layer.label == label; }),
        g_state.runtimeLayers.end());
    if (g_state.runtimeLayers.size() == before) {
        return false;
    }
    g_state.revision++;
    rebuildMergedLocked();
    return true;
}

const karma::json::Value *ConfigStore::LayerByLabel(const std::string &label) {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    auto it = g_state.labelIndex.find(label);
    if (it == g_state.labelIndex.end()) {
        return nullptr;
    }
    const auto [kind, index] = it->second;
    if (kind == 0) {
        return index < g_state.defaultLayers.size() ? &g_state.defaultLayers[index].json : nullptr;
    }
    if (kind == 1) {
        return g_state.userLayer ? &g_state.userLayer->json : nullptr;
    }
    return index < g_state.runtimeLayers.size() ? &g_state.runtimeLayers[index].json : nullptr;
}

std::filesystem::path ConfigStore::ResolveAssetPath(const std::string &assetKey,
                                                    const std::filesystem::path &defaultPath) {
    std::lock_guard<std::mutex> lock(g_state.mutex);
    const auto it = g_state.assetLookup.find(assetKey);
    if (it != g_state.assetLookup.end()) {
        return it->second;
    }
    return defaultPath;
}

void ConfigStore::rebuildMergedLocked() {
    g_state.mergedDirty = false;
    g_state.lastMergeTime = std::chrono::steady_clock::now();
    g_state.merged = g_state.defaults;
    if (g_state.userLayer) {
        mergeJsonObjects(g_state.merged, g_state.userLayer->json);
    }
    for (const auto &layer : g_state.runtimeLayers) {
        mergeJsonObjects(g_state.merged, layer.json);
    }

    std::vector<ConfigLayer> allLayers;
    allLayers.reserve(g_state.defaultLayers.size() + g_state.runtimeLayers.size() + 1);
    for (const auto &layer : g_state.defaultLayers) {
        allLayers.push_back(layer);
    }
    if (g_state.userLayer) {
        allLayers.push_back(*g_state.userLayer);
    }
    for (const auto &layer : g_state.runtimeLayers) {
        allLayers.push_back(layer);
    }
    g_state.assetLookup = buildAssetLookup(allLayers);

    g_state.labelIndex.clear();
    for (std::size_t i = 0; i < g_state.defaultLayers.size(); ++i) {
        g_state.labelIndex[g_state.defaultLayers[i].label] = {0, i};
    }
    if (g_state.userLayer) {
        g_state.labelIndex[g_state.userLayer->label] = {1, 0};
    }
    for (std::size_t i = 0; i < g_state.runtimeLayers.size(); ++i) {
        g_state.labelIndex[g_state.runtimeLayers[i].label] = {2, i};
    }
}

bool ConfigStore::setValueAtPath(karma::json::Value &root, std::string_view path, karma::json::Value value) {
    std::vector<std::pair<std::string, std::optional<std::size_t>>> segments;
    if (!parsePathSegments(path, segments)) {
        return false;
    }
    if (!root.is_object()) {
        root = karma::json::Object();
    }
    karma::json::Value *current = &root;
    for (std::size_t i = 0; i < segments.size(); ++i) {
        const auto &[key, index] = segments[i];
        const bool last = (i == segments.size() - 1);
        if (!key.empty()) {
            if (!current->is_object()) {
                *current = karma::json::Object();
            }
            if (last && !index.has_value()) {
                (*current)[key] = std::move(value);
                return true;
            }
            if (!current->contains(key)) {
                (*current)[key] = index.has_value() ? karma::json::Array() : karma::json::Object();
            }
            current = &(*current)[key];
        }

        if (index.has_value()) {
            if (!current->is_array()) {
                *current = karma::json::Array();
            }
            while (current->size() <= *index) {
                current->push_back(nullptr);
            }
            if (last) {
                (*current)[*index] = std::move(value);
                return true;
            }
            current = &(*current)[*index];
        }
    }
    return false;
}

bool ConfigStore::eraseValueAtPath(karma::json::Value &root, std::string_view path) {
    std::vector<std::pair<std::string, std::optional<std::size_t>>> segments;
    if (!parsePathSegments(path, segments)) {
        return false;
    }
    karma::json::Value *current = &root;
    for (std::size_t i = 0; i < segments.size(); ++i) {
        const auto &[key, index] = segments[i];
        const bool last = (i == segments.size() - 1);
        if (!key.empty()) {
            if (!current->is_object()) {
                return false;
            }
            auto it = current->find(key);
            if (it == current->end()) {
                return false;
            }
            if (last && !index.has_value()) {
                current->erase(key);
                return true;
            }
            current = &(*it);
        }
        if (index.has_value()) {
            if (!current->is_array() || *index >= current->size()) {
                return false;
            }
            if (last) {
                (*current)[*index] = nullptr;
                return true;
            }
            current = &(*current)[*index];
        }
    }
    return false;
}

} // namespace karma::config

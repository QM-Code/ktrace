#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "common/serialization/json.hpp"
#include <spdlog/spdlog.h>

namespace karma::common::config {

struct ConfigFileSpec {
    std::filesystem::path path;
    std::string label;
    spdlog::level::level_enum missingLevel = spdlog::level::warn;
    bool required = false;
    bool resolveRelativeToDataRoot = true;
};

struct ConfigLayer {
    karma::common::serialization::Value json;
    std::filesystem::path baseDir;
    std::string label;
};

class ConfigStore {
public:
    static void Initialize(const std::vector<ConfigFileSpec> &defaultSpecs,
                           const std::optional<std::filesystem::path> &userConfigPath = std::nullopt,
                           const std::vector<ConfigFileSpec> &runtimeSpecs = {});
    static bool Initialized();
    static uint64_t Revision();

    static const karma::common::serialization::Value &Defaults();
    static const karma::common::serialization::Value &User();
    static const karma::common::serialization::Value &Merged();

    static const karma::common::serialization::Value *Get(std::string_view path);
    static std::optional<karma::common::serialization::Value> GetCopy(std::string_view path);

    static bool Set(std::string_view path, karma::common::serialization::Value value);
    static bool Erase(std::string_view path);
    static bool ReplaceUserConfig(karma::common::serialization::Value userConfig, std::string *error = nullptr);

    static bool SaveUser(std::string *error = nullptr);
    static void Tick();
    static const std::filesystem::path &UserConfigPath();

    static bool AddRuntimeLayer(const std::string &label,
                                const karma::common::serialization::Value &layerJson,
                                const std::filesystem::path &baseDir);
    static bool RemoveRuntimeLayer(const std::string &label);
    static const karma::common::serialization::Value *LayerByLabel(const std::string &label);

    static std::filesystem::path ResolveAssetPath(const std::string &assetKey,
                                                  const std::filesystem::path &defaultPath = {});

private:
    static void rebuildMergedLocked();
    static bool setValueAtPath(karma::common::serialization::Value &root, std::string_view path, karma::common::serialization::Value value);
    static bool eraseValueAtPath(karma::common::serialization::Value &root, std::string_view path);
    static bool saveUserUnlocked(std::string *error, bool ignoreInterval);
};

} // namespace karma::common::config

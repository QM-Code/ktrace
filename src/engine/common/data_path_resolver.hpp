#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "common/json.hpp"
#include <spdlog/spdlog.h>

namespace karma::data {

// Resolve paths located under the runtime data directory.
std::filesystem::path Resolve(const std::filesystem::path &relativePath);

// Overrides the detected data directory. Must be called before the first Resolve/DataRoot invocation.
void SetDataRootOverride(const std::filesystem::path &path);

std::optional<karma::json::Value> LoadJsonFile(const std::filesystem::path &path,
										   const std::string &label,
										   spdlog::level::level_enum missingLevel);

std::filesystem::path UserConfigDirectory();
std::filesystem::path EnsureUserConfigFile(const std::string &fileName);
std::filesystem::path EnsureUserWorldsDirectory();
std::filesystem::path EnsureUserWorldDirectoryForServer(const std::string &host, uint16_t port);

struct ConfigLayerSpec {
	std::filesystem::path relativePath;
	std::string label;
	spdlog::level::level_enum missingLevel = spdlog::level::warn;
	bool required = false;
};

struct ConfigLayer {
	karma::json::Value json;
	std::filesystem::path baseDir;
	std::string label;
};

std::vector<ConfigLayer> LoadConfigLayers(const std::vector<ConfigLayerSpec> &specs);

void MergeJsonObjects(karma::json::Value &destination, const karma::json::Value &source);

void CollectAssetEntries(const karma::json::Value &node,
						 const std::filesystem::path &baseDir,
						 std::map<std::string, std::filesystem::path> &assetMap,
						 const std::string &prefix = "");

struct DataPathSpec {
    std::string appName = "app";
    std::string dataDirEnvVar = "DATA_DIR";
    std::filesystem::path requiredDataMarker;
    std::vector<ConfigLayerSpec> fallbackAssetLayers;
};

enum class ContentMountType {
    FileSystem,
    Package
};

struct ContentMount {
    std::string id;
    ContentMountType type = ContentMountType::FileSystem;
    std::filesystem::path source;
    std::filesystem::path mountPoint;
};

void SetDataPathSpec(DataPathSpec spec);
DataPathSpec GetDataPathSpec();

// Registers package metadata for future package-backed content loading.
// Current Resolve() behavior remains filesystem-root based.
void RegisterPackageMount(const std::string &id,
                          const std::filesystem::path &packagePath,
                          const std::filesystem::path &mountPoint = {});
void ClearPackageMounts();
std::vector<ContentMount> GetContentMounts();


// Returns the directory containing the running executable.
std::filesystem::path ExecutableDirectory();

// Resolve an asset path declared in configuration layers, falling back to a default relative path.
std::filesystem::path ResolveConfiguredAsset(const std::string &assetKey,
											 const std::filesystem::path &defaultRelativePath = {});
// Returns the detected runtime data directory.
const std::filesystem::path &DataRoot();

} // namespace karma::data

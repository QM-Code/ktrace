#include "common/data_path_resolver.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <map>
#include <utility>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <sstream>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

#include "common/config_store.hpp"
#include "common/json.hpp"
#include <spdlog/spdlog.h>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

namespace {

using karma::data::ContentMount;
using karma::data::ContentMountType;
using karma::data::DataPathSpec;

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

std::string SanitizePathComponent(std::string_view value) {
    std::string sanitized;
    sanitized.reserve(value.size());

    for (char ch : value) {
        if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '.' || ch == '-' || ch == '_') {
            sanitized.push_back(ch);
        } else {
            sanitized.push_back('_');
        }
    }

    if (sanitized.empty()) {
        sanitized = "server";
    }

    return sanitized;
}

std::filesystem::path ExecutableDirectory() {
#if defined(_WIN32)
    std::array<char, MAX_PATH> buffer{};
    const DWORD length = GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length == buffer.size()) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(buffer.data(), buffer.data() + length).parent_path();
#elif defined(__APPLE__)
    std::array<char, PATH_MAX> buffer{};
    uint32_t size = static_cast<uint32_t>(buffer.size());
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        buffer.fill('\0');
        if (size > buffer.size()) {
            return std::filesystem::current_path();
        }
        if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
            return std::filesystem::current_path();
        }
    }
    return TryCanonical(std::filesystem::path(buffer.data())).parent_path();
#else
    std::array<char, PATH_MAX> buffer{};
    const ssize_t length = ::readlink("/proc/self/exe", buffer.data(), buffer.size());
    if (length <= 0 || static_cast<size_t>(length) >= buffer.size()) {
        return std::filesystem::current_path();
    }
    return TryCanonical(std::filesystem::path(buffer.data(), buffer.data() + length)).parent_path();
#endif
}

std::mutex g_dataRootMutex;
std::optional<std::filesystem::path> g_dataRootOverride;
bool g_dataRootInitialized = false;
std::mutex g_dataSpecMutex;
DataPathSpec g_dataSpec;

std::filesystem::path NormalizeMountPoint(const std::filesystem::path &mountPoint) {
    if (mountPoint.empty()) {
        return {};
    }

    std::filesystem::path normalized = mountPoint.lexically_normal();
    if (normalized == ".") {
        return {};
    }
    if (normalized.is_absolute()) {
        normalized = normalized.relative_path();
    }
    return normalized;
}

class ContentMountManager {
  public:
    void setFilesystemRoot(const std::filesystem::path &root) {
        std::lock_guard<std::mutex> lock(mutex_);
        filesystem_mount_ = ContentMount{
            .id = "data-root",
            .type = ContentMountType::FileSystem,
            .source = root,
            .mountPoint = {}
        };
    }

    void registerPackageMount(const std::string &id,
                              const std::filesystem::path &packagePath,
                              const std::filesystem::path &mountPoint) {
        if (id.empty()) {
            throw std::runtime_error("data_path_resolver: package mount id must not be empty");
        }
        if (packagePath.empty()) {
            throw std::runtime_error("data_path_resolver: package mount path must not be empty");
        }

        ContentMount mount{
            .id = id,
            .type = ContentMountType::Package,
            .source = packagePath,
            .mountPoint = NormalizeMountPoint(mountPoint)
        };

        std::lock_guard<std::mutex> lock(mutex_);
        auto existing = std::find_if(package_mounts_.begin(), package_mounts_.end(), [&id](const ContentMount &entry) {
            return entry.id == id;
        });
        if (existing != package_mounts_.end()) {
            *existing = std::move(mount);
            return;
        }
        package_mounts_.push_back(std::move(mount));
    }

    void clearPackageMounts() {
        std::lock_guard<std::mutex> lock(mutex_);
        package_mounts_.clear();
    }

    std::vector<ContentMount> snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<ContentMount> mounts;
        mounts.reserve((filesystem_mount_.has_value() ? 1U : 0U) + package_mounts_.size());
        if (filesystem_mount_.has_value()) {
            mounts.push_back(*filesystem_mount_);
        }
        mounts.insert(mounts.end(), package_mounts_.begin(), package_mounts_.end());
        return mounts;
    }

    std::filesystem::path resolve(const std::filesystem::path &relativePath) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!filesystem_mount_.has_value()) {
            throw std::runtime_error("data_path_resolver: content mounts are not initialized");
        }
        return TryCanonical(filesystem_mount_->source / relativePath);
    }

  private:
    mutable std::mutex mutex_;
    std::optional<ContentMount> filesystem_mount_;
    std::vector<ContentMount> package_mounts_;
};

ContentMountManager g_mountManager;

std::filesystem::path ValidateDataRootCandidate(const std::filesystem::path &path) {
    const auto canonical = TryCanonical(path);
    std::error_code ec;
    if (!std::filesystem::exists(canonical, ec) || !std::filesystem::is_directory(canonical, ec)) {
        throw std::runtime_error("data_path_resolver: Data directory is invalid: " + canonical.string());
    }

    DataPathSpec spec;
    {
        std::lock_guard<std::mutex> lock(g_dataSpecMutex);
        spec = g_dataSpec;
    }

    if (!spec.requiredDataMarker.empty()) {
        const auto markerPath = canonical / spec.requiredDataMarker;
        if (!std::filesystem::exists(markerPath, ec) || !std::filesystem::is_regular_file(markerPath, ec)) {
            throw std::runtime_error("Invalid data directory: " + canonical.string() + "\n" + markerPath.string() + " does not exist.");
        }
    }
    return canonical;
}

std::filesystem::path DetectDataRoot(const std::optional<std::filesystem::path> &overridePath) {
    if (overridePath) {
        return ValidateDataRootCandidate(*overridePath);
    }

    DataPathSpec spec;
    {
        std::lock_guard<std::mutex> lock(g_dataSpecMutex);
        spec = g_dataSpec;
    }

    const char *envDataDir = std::getenv(spec.dataDirEnvVar.c_str());
    if (!envDataDir || *envDataDir == '\0') {
        throw std::runtime_error(spec.dataDirEnvVar + " environment variable must be set to the data directory");
    }

    return ValidateDataRootCandidate(envDataDir);
}

} // namespace

namespace karma::data {

void SetDataPathSpec(DataPathSpec spec) {
    std::lock_guard<std::mutex> lock(g_dataSpecMutex);
    g_dataSpec = std::move(spec);
}

DataPathSpec GetDataPathSpec() {
    std::lock_guard<std::mutex> lock(g_dataSpecMutex);
    return g_dataSpec;
}

void RegisterPackageMount(const std::string &id,
                          const std::filesystem::path &packagePath,
                          const std::filesystem::path &mountPoint) {
    std::filesystem::path resolvedPackagePath = packagePath;
    if (resolvedPackagePath.is_absolute()) {
        resolvedPackagePath = TryCanonical(resolvedPackagePath);
    } else {
        resolvedPackagePath = Resolve(resolvedPackagePath);
    }

    g_mountManager.registerPackageMount(id, resolvedPackagePath, mountPoint);
}

void ClearPackageMounts() {
    g_mountManager.clearPackageMounts();
}

std::vector<ContentMount> GetContentMounts() {
    // Ensure filesystem mount is initialized before snapshotting.
    (void)DataRoot();
    return g_mountManager.snapshot();
}

std::filesystem::path ExecutableDirectory() {
    return ::ExecutableDirectory();
}

const std::filesystem::path &DataRoot() {
    static std::once_flag initFlag;
    static std::filesystem::path root;

    std::call_once(initFlag, [] {
        std::optional<std::filesystem::path> overrideCopy;
        {
            std::lock_guard<std::mutex> lock(g_dataRootMutex);
            overrideCopy = g_dataRootOverride;
        }

        root = DetectDataRoot(overrideCopy);
        g_mountManager.setFilesystemRoot(root);

        std::lock_guard<std::mutex> lock(g_dataRootMutex);
        g_dataRootInitialized = true;
    });

    return root;
}

void SetDataRootOverride(const std::filesystem::path &path) {
    std::lock_guard<std::mutex> lock(g_dataRootMutex);
    if (g_dataRootInitialized) {
        throw std::runtime_error("data_path_resolver: Data root already initialized; override must be set earlier");
    }

    g_dataRootOverride = ValidateDataRootCandidate(path);
}

std::filesystem::path Resolve(const std::filesystem::path &relativePath) {
    if (relativePath.is_absolute()) {
        return TryCanonical(relativePath);
    }

    // DataRoot() initializes the default filesystem mount on first use.
    (void)DataRoot();
    return g_mountManager.resolve(relativePath);
}

std::filesystem::path ResolveWithBase(const std::filesystem::path &baseDir, const std::string &value) {
    std::filesystem::path candidate(value);
    if (!candidate.is_absolute()) {
        candidate = baseDir / candidate;
    }
    return TryCanonical(candidate);
}

std::filesystem::path UserConfigDirectory() {
    static const std::filesystem::path dir = [] {
        DataPathSpec spec;
        {
            std::lock_guard<std::mutex> lock(g_dataSpecMutex);
            spec = g_dataSpec;
        }
        const std::string appName = spec.appName.empty() ? std::string("app") : spec.appName;
        std::filesystem::path base;

#if defined(_WIN32)
        if (const char *appData = std::getenv("APPDATA"); appData && *appData) {
            base = appData;
        } else if (const char *userProfile = std::getenv("USERPROFILE"); userProfile && *userProfile) {
            base = std::filesystem::path(userProfile) / "AppData" / "Roaming";
        }
#elif defined(__APPLE__)
        if (const char *home = std::getenv("HOME"); home && *home) {
            base = std::filesystem::path(home) / "Library" / "Application Support";
        }
#else
        if (const char *xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) {
            base = xdg;
        } else if (const char *home = std::getenv("HOME"); home && *home) {
            base = std::filesystem::path(home) / ".config";
        }
#endif

        if (base.empty()) {
            throw std::runtime_error("Unable to determine user configuration directory: no home path detected");
        }

        return TryCanonical(base / appName);
    }();

    return dir;
}

std::filesystem::path EnsureUserConfigFile(const std::string &fileName) {
    const auto configDir = UserConfigDirectory();

    std::error_code ec;
    std::filesystem::create_directories(configDir, ec);
    if (ec) {
        throw std::runtime_error("Failed to create user config directory " + configDir.string() + ": " + ec.message());
    }

    const auto filePath = configDir / fileName;
    if (!std::filesystem::exists(filePath)) {
        std::ofstream stream(filePath);
        if (!stream) {
            throw std::runtime_error("Failed to create user config file " + filePath.string());
        }

        stream << "{}\n";
        if (!stream) {
            throw std::runtime_error("Failed to initialize user config file " + filePath.string());
        }
    } else if (std::filesystem::is_regular_file(filePath)) {
        std::error_code sizeEc;
        const auto fileSize = std::filesystem::file_size(filePath, sizeEc);
        if (!sizeEc && fileSize == 0) {
            std::ofstream stream(filePath, std::ios::trunc);
            if (!stream) {
                throw std::runtime_error("Failed to truncate empty user config file " + filePath.string());
            }

            stream << "{}\n";
            if (!stream) {
                throw std::runtime_error("Failed to initialize truncated user config file " + filePath.string());
            }
        }
    }

    return TryCanonical(filePath);
}

std::filesystem::path EnsureUserWorldsDirectory() {
    const auto baseDir = UserConfigDirectory();
    const auto worldsDir = baseDir / "worlds";

    std::error_code ec;
    std::filesystem::create_directories(worldsDir, ec);
    if (ec) {
        throw std::runtime_error("Failed to create user worlds directory " + worldsDir.string() + ": " + ec.message());
    }

    return TryCanonical(worldsDir);
}

std::filesystem::path EnsureUserWorldDirectoryForServer(const std::string &host, uint16_t port) {
    const auto baseDir = EnsureUserWorldsDirectory();
    const auto sanitizedHost = SanitizePathComponent(host);

    std::ostringstream name;
    name << sanitizedHost << '.' << port;

    const auto serverDir = baseDir / name.str();

    std::error_code ec;
    std::filesystem::create_directories(serverDir, ec);
    if (ec) {
        throw std::runtime_error("Failed to create server world directory " + serverDir.string() + ": " + ec.message());
    }

    return TryCanonical(serverDir);
}

std::optional<karma::json::Value> LoadJsonFile(const std::filesystem::path &path,
                                           const std::string &label,
                                           spdlog::level::level_enum missingLevel) {
    if (!std::filesystem::exists(path)) {
        (void)missingLevel;
        spdlog::error("data_path_resolver: {} not found: {}", label, path.string());
        return std::nullopt;
    }

    std::ifstream stream(path);
    if (!stream) {
        spdlog::error("data_path_resolver: Failed to open {}: {}", label, path.string());
        return std::nullopt;
    }

    try {
        karma::json::Value json;
        stream >> json;
        return json;
    } catch (const std::exception &e) {
        spdlog::error("data_path_resolver: Failed to parse {}: {}", label, e.what());
        return std::nullopt;
    }
}

std::vector<ConfigLayer> LoadConfigLayers(const std::vector<ConfigLayerSpec> &specs) {
    std::vector<ConfigLayer> layers;
    layers.reserve(specs.size());

    for (const auto &spec : specs) {
        const auto absolutePath = Resolve(spec.relativePath);
        const std::string label = spec.label.empty() ? spec.relativePath.string() : spec.label;
        auto jsonOpt = LoadJsonFile(absolutePath, label, spec.missingLevel);
        if (!jsonOpt) {
            spdlog::error("data_path_resolver: Config missing: {}", absolutePath.string());
            continue;
        }

        if (!jsonOpt->is_object()) {
            spdlog::warn("data_path_resolver: Config {} is not a JSON object, skipping", absolutePath.string());
            continue;
        }

        layers.push_back({std::move(*jsonOpt), absolutePath.parent_path(), label});
    }

    return layers;
}

void MergeJsonObjects(karma::json::Value &destination, const karma::json::Value &source) {
    if (!destination.is_object() || !source.is_object()) {
        destination = source;
        return;
    }

    for (auto it = source.begin(); it != source.end(); ++it) {
        const auto &key = it.key();
        const auto &value = it.value();

        if (value.is_object() && destination.contains(key) && destination[key].is_object()) {
            MergeJsonObjects(destination[key], value);
        } else {
            destination[key] = value;
        }
    }
}

void CollectAssetEntries(const karma::json::Value &node,
                         const std::filesystem::path &baseDir,
                         std::map<std::string, std::filesystem::path> &assetMap,
                         const std::string &prefix) {
    if (!node.is_object()) {
        return;
    }

    for (const auto &[key, value] : node.items()) {
        const std::string fullKey = prefix.empty() ? key : prefix + "." + key;
        if (value.is_string()) {
            assetMap[fullKey] = ResolveWithBase(baseDir, value.get<std::string>());
        } else if (value.is_object()) {
            CollectAssetEntries(value, baseDir, assetMap, fullKey);
        }
    }
}

namespace {

std::unordered_map<std::string, std::filesystem::path> BuildAssetLookupFromLayers(const std::vector<ConfigLayer> &layers) {
    std::map<std::string, std::filesystem::path> flattened;

    for (const auto &layer : layers) {
        if (!layer.json.is_object()) {
            continue;
        }

        const auto assetsIt = layer.json.find("assets");
        if (assetsIt != layer.json.end()) {
            if (!assetsIt->is_object()) {
                spdlog::warn("data_path_resolver: 'assets' in {} is not an object; skipping", layer.baseDir.string());
            } else {
                CollectAssetEntries(*assetsIt, layer.baseDir, flattened, "assets");
            }
        }

        const auto fontsIt = layer.json.find("fonts");
        if (fontsIt != layer.json.end()) {
            if (!fontsIt->is_object()) {
                spdlog::warn("data_path_resolver: 'fonts' in {} is not an object; skipping", layer.baseDir.string());
            } else {
                CollectAssetEntries(*fontsIt, layer.baseDir, flattened, "fonts");
            }
        }
    }

    std::unordered_map<std::string, std::filesystem::path> lookup;
    lookup.reserve(flattened.size());

    for (const auto &[key, resolvedPath] : flattened) {
        lookup[key] = resolvedPath;
    }

    return lookup;
}

} // namespace

std::filesystem::path ResolveConfiguredAsset(const std::string &assetKey,
                                             const std::filesystem::path &defaultRelativePath) {
    const auto defaultPath = defaultRelativePath.empty() ? std::filesystem::path{} : Resolve(defaultRelativePath);

    if (karma::config::ConfigStore::Initialized()) {
        const auto resolved = karma::config::ConfigStore::ResolveAssetPath(assetKey, defaultPath);
        if (resolved.empty() && defaultPath.empty()) {
            spdlog::warn("data_path_resolver: Asset '{}' not found in configuration layers", assetKey);
        }
        return resolved;
    }

    DataPathSpec spec;
    {
        std::lock_guard<std::mutex> lock(g_dataSpecMutex);
        spec = g_dataSpec;
    }
    if (spec.fallbackAssetLayers.empty()) {
        spdlog::warn("data_path_resolver: Asset '{}' not found in configuration layers, using default.", assetKey);
        return defaultPath;
    }

    static std::once_flag fallbackLoadFlag;
    static std::unordered_map<std::string, std::filesystem::path> fallbackLookup;
    std::call_once(fallbackLoadFlag, [spec] {
        const auto layers = LoadConfigLayers(spec.fallbackAssetLayers);
        fallbackLookup = BuildAssetLookupFromLayers(layers);
    });

    if (const auto it = fallbackLookup.find(assetKey); it != fallbackLookup.end()) {
        return it->second;
    }

    spdlog::warn("data_path_resolver: Asset '{}' not found in configuration layers, using default.", assetKey);
    return defaultPath;
}

} // namespace karma::data

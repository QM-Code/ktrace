#pragma once

#include "karma/common/content/manifest.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace karma::common::content {

inline constexpr size_t kMaxCachePathComponentLen = 96;
inline constexpr uint16_t kDefaultMaxRevisionsPerWorld = 4;
inline constexpr uint16_t kDefaultMaxPackagesPerRevision = 2;

struct CachedContentIdentity {
    std::string world_hash{};
    std::string world_content_hash{};
    std::string world_id{};
    std::string world_revision{};
};

enum class CachePruneWarningKind {
    PrunePackage,
    RemoveEmptyRevision,
    PruneRevision,
    RemoveEmptyWorldDir,
};

struct CachePruneWarning {
    CachePruneWarningKind kind = CachePruneWarningKind::PrunePackage;
    std::filesystem::path path{};
    std::string message{};
};

struct CachePruneResult {
    size_t scanned_world_dirs = 0;
    size_t scanned_revision_dirs = 0;
    size_t scanned_package_dirs = 0;
    size_t pruned_revision_dirs = 0;
    size_t pruned_package_dirs = 0;
    std::vector<std::filesystem::path> pruned_revision_paths{};
    std::vector<std::filesystem::path> pruned_package_paths{};
    std::vector<CachePruneWarning> warnings{};
};

std::string SanitizeCachePathComponent(
    std::string_view input,
    std::string_view fallback_prefix,
    size_t max_component_len = kMaxCachePathComponentLen);
std::string ResolveWorldPackageCacheKey(std::string_view world_content_hash, std::string_view world_hash);
std::string WorldCacheDirName(std::string_view world_id,
                              size_t max_component_len = kMaxCachePathComponentLen);
std::string RevisionCacheDirName(std::string_view world_revision,
                                 size_t max_component_len = kMaxCachePathComponentLen);
std::filesystem::path PackageRootForIdentity(
    const std::filesystem::path& world_packages_by_world_root,
    std::string_view world_id,
    std::string_view world_revision,
    std::string_view world_package_cache_key,
    size_t max_component_len = kMaxCachePathComponentLen);

void TouchPathIfPresent(const std::filesystem::path& path);
std::filesystem::file_time_type LastWriteTimeOrMin(const std::filesystem::path& path);

bool HasPackageIdentity(const CachedContentIdentity& identity);
bool HasRequiredIdentityFields(const CachedContentIdentity& identity);
std::optional<CachedContentIdentity> ReadCachedIdentityFile(const std::filesystem::path& identity_file);
bool PersistCachedIdentityFile(const std::filesystem::path& identity_file,
                               std::string_view world_hash,
                               std::string_view world_content_hash,
                               std::string_view world_id,
                               std::string_view world_revision);

std::vector<ManifestEntry> ReadCachedManifestFile(const std::filesystem::path& manifest_file,
                                                  bool* malformed = nullptr);
bool PersistCachedManifestFile(const std::filesystem::path& manifest_file,
                               const std::vector<ManifestEntry>& manifest);

CachePruneResult PruneWorldPackageCache(
    const std::filesystem::path& world_packages_by_world_root,
    std::string_view active_world_id,
    std::string_view active_world_revision,
    std::string_view active_world_package_key,
    uint16_t max_revisions_per_world = kDefaultMaxRevisionsPerWorld,
    uint16_t max_packages_per_revision = kDefaultMaxPackagesPerRevision,
    size_t max_component_len = kMaxCachePathComponentLen);

} // namespace karma::common::content

#include "karma/common/content/cache_store.hpp"

#include "karma/common/content/primitives.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <string>
#include <vector>

namespace karma::common::content {

namespace {

struct CacheDirectoryEntry {
    std::filesystem::path path{};
    std::string name{};
    std::filesystem::file_time_type modified{};
};

std::vector<CacheDirectoryEntry> CollectDirectoryEntriesByMtimeDesc(const std::filesystem::path& root) {
    std::vector<CacheDirectoryEntry> entries{};
    std::error_code ec;
    if (!std::filesystem::exists(root, ec) || ec || !std::filesystem::is_directory(root, ec)) {
        return entries;
    }

    for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_directory()) {
            continue;
        }
        const auto path = entry.path();
        entries.push_back(CacheDirectoryEntry{
            .path = path,
            .name = path.filename().string(),
            .modified = LastWriteTimeOrMin(path)});
    }

    std::sort(entries.begin(),
              entries.end(),
              [](const CacheDirectoryEntry& lhs, const CacheDirectoryEntry& rhs) {
                  return lhs.modified > rhs.modified;
              });
    return entries;
}

} // namespace

std::string SanitizeCachePathComponent(std::string_view input,
                                       std::string_view fallback_prefix,
                                       size_t max_component_len) {
    std::string sanitized{};
    sanitized.reserve(input.size());
    for (const char ch : input) {
        const auto uch = static_cast<unsigned char>(ch);
        if (std::isalnum(uch) || ch == '-' || ch == '_' || ch == '.') {
            sanitized.push_back(ch);
        } else {
            sanitized.push_back('_');
        }
        if (sanitized.size() >= max_component_len) {
            break;
        }
    }

    while (!sanitized.empty() && sanitized.front() == '.') {
        sanitized.erase(sanitized.begin());
    }

    if (sanitized.empty()) {
        sanitized = std::string(fallback_prefix);
        sanitized.push_back('-');
        sanitized += Hash64Hex(HashStringFNV1a(input));
    }
    return sanitized;
}

std::string ResolveWorldPackageCacheKey(std::string_view world_content_hash,
                                        std::string_view world_hash) {
    if (!world_content_hash.empty()) {
        return std::string(world_content_hash);
    }
    if (!world_hash.empty()) {
        return std::string(world_hash);
    }
    return "adhoc";
}

std::string WorldCacheDirName(std::string_view world_id, size_t max_component_len) {
    return SanitizeCachePathComponent(world_id, "world", max_component_len);
}

std::string RevisionCacheDirName(std::string_view world_revision, size_t max_component_len) {
    return SanitizeCachePathComponent(world_revision, "rev", max_component_len);
}

std::filesystem::path PackageRootForIdentity(const std::filesystem::path& world_packages_by_world_root,
                                             std::string_view world_id,
                                             std::string_view world_revision,
                                             std::string_view world_package_cache_key,
                                             size_t max_component_len) {
    const std::string world_dir = WorldCacheDirName(world_id, max_component_len);
    const std::string revision_dir = RevisionCacheDirName(world_revision, max_component_len);
    const std::string package_dir =
        world_package_cache_key.empty()
            ? std::string("adhoc")
            : SanitizeCachePathComponent(world_package_cache_key, "hash", max_component_len);
    return world_packages_by_world_root / world_dir / revision_dir / package_dir;
}

void TouchPathIfPresent(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        return;
    }
    ec.clear();
    std::filesystem::last_write_time(path, std::filesystem::file_time_type::clock::now(), ec);
}

std::filesystem::file_time_type LastWriteTimeOrMin(const std::filesystem::path& path) {
    std::error_code ec;
    const auto timestamp = std::filesystem::last_write_time(path, ec);
    if (ec) {
        return std::filesystem::file_time_type::min();
    }
    return timestamp;
}

bool HasPackageIdentity(const CachedContentIdentity& identity) {
    return !identity.world_hash.empty() || !identity.world_content_hash.empty();
}

bool HasRequiredIdentityFields(const CachedContentIdentity& identity) {
    return !identity.world_id.empty() && !identity.world_revision.empty();
}

std::optional<CachedContentIdentity> ReadCachedIdentityFile(const std::filesystem::path& identity_file) {
    if (!std::filesystem::exists(identity_file) || !std::filesystem::is_regular_file(identity_file)) {
        return std::nullopt;
    }

    std::ifstream input(identity_file);
    if (!input) {
        return std::nullopt;
    }

    CachedContentIdentity identity{};
    std::string line{};
    while (std::getline(input, line)) {
        const size_t split = line.find('=');
        if (split == std::string::npos) {
            continue;
        }
        const std::string key = line.substr(0, split);
        const std::string value = line.substr(split + 1);
        if (key == "hash") {
            identity.world_hash = value;
        } else if (key == "content_hash") {
            identity.world_content_hash = value;
        } else if (key == "id") {
            identity.world_id = value;
        } else if (key == "revision") {
            identity.world_revision = value;
        }
    }

    if (!HasRequiredIdentityFields(identity)) {
        return std::nullopt;
    }

    return identity;
}

bool PersistCachedIdentityFile(const std::filesystem::path& identity_file,
                               std::string_view world_hash,
                               std::string_view world_content_hash,
                               std::string_view world_id,
                               std::string_view world_revision) {
    std::error_code ec;
    if (world_hash.empty() && world_content_hash.empty() && world_id.empty() && world_revision.empty()) {
        std::filesystem::remove(identity_file, ec);
        return !ec;
    }
    if (world_id.empty() || world_revision.empty() ||
        (world_hash.empty() && world_content_hash.empty())) {
        return false;
    }

    std::filesystem::create_directories(identity_file.parent_path(), ec);
    if (ec) {
        return false;
    }

    std::ofstream output(identity_file, std::ios::trunc);
    if (!output) {
        return false;
    }
    output << "hash=" << world_hash << '\n';
    output << "content_hash=" << world_content_hash << '\n';
    output << "id=" << world_id << '\n';
    output << "revision=" << world_revision << '\n';
    return static_cast<bool>(output);
}

std::vector<ManifestEntry> ReadCachedManifestFile(const std::filesystem::path& manifest_file,
                                                  bool* malformed) {
    if (malformed) {
        *malformed = false;
    }
    if (!std::filesystem::exists(manifest_file) || !std::filesystem::is_regular_file(manifest_file)) {
        return {};
    }

    std::ifstream input(manifest_file);
    if (!input) {
        return {};
    }

    std::vector<ManifestEntry> manifest{};
    std::string path{};
    uint64_t size = 0;
    std::string hash{};
    while (input >> std::quoted(path) >> size >> std::quoted(hash)) {
        manifest.push_back(ManifestEntry{
            .path = path,
            .size = size,
            .hash = hash});
    }

    if (!input.eof()) {
        if (malformed) {
            *malformed = true;
        }
        return {};
    }
    return manifest;
}

bool PersistCachedManifestFile(const std::filesystem::path& manifest_file,
                               const std::vector<ManifestEntry>& manifest) {
    std::error_code ec;
    if (manifest.empty()) {
        std::filesystem::remove(manifest_file, ec);
        return !ec;
    }

    std::filesystem::create_directories(manifest_file.parent_path(), ec);
    if (ec) {
        return false;
    }

    std::ofstream output(manifest_file, std::ios::trunc);
    if (!output) {
        return false;
    }
    for (const auto& entry : manifest) {
        output << std::quoted(entry.path) << ' '
               << entry.size << ' '
               << std::quoted(entry.hash) << '\n';
    }
    return static_cast<bool>(output);
}

CachePruneResult PruneWorldPackageCache(const std::filesystem::path& world_packages_by_world_root,
                                        std::string_view active_world_id,
                                        std::string_view active_world_revision,
                                        std::string_view active_world_package_key,
                                        uint16_t max_revisions_per_world,
                                        uint16_t max_packages_per_revision,
                                        size_t max_component_len) {
    CachePruneResult result{};
    const std::string active_world_dir = WorldCacheDirName(active_world_id, max_component_len);
    const std::string active_revision_dir =
        RevisionCacheDirName(active_world_revision, max_component_len);
    const std::string active_package_dir =
        active_world_package_key.empty()
            ? std::string("adhoc")
            : SanitizeCachePathComponent(active_world_package_key, "hash", max_component_len);

    for (const auto& world_entry : CollectDirectoryEntriesByMtimeDesc(world_packages_by_world_root)) {
        ++result.scanned_world_dirs;
        const auto revision_entries = CollectDirectoryEntriesByMtimeDesc(world_entry.path);
        size_t kept_non_active_revisions = 0;
        for (const auto& revision_entry : revision_entries) {
            ++result.scanned_revision_dirs;
            const bool is_active_revision =
                world_entry.name == active_world_dir && revision_entry.name == active_revision_dir;

            const auto package_entries = CollectDirectoryEntriesByMtimeDesc(revision_entry.path);
            size_t kept_non_active_packages = 0;
            for (const auto& package_entry : package_entries) {
                ++result.scanned_package_dirs;
                const bool is_active_package =
                    is_active_revision && package_entry.name == active_package_dir;
                if (is_active_package) {
                    continue;
                }
                if (kept_non_active_packages < static_cast<size_t>(max_packages_per_revision)) {
                    ++kept_non_active_packages;
                    continue;
                }

                std::error_code remove_ec;
                const auto removed_count = std::filesystem::remove_all(package_entry.path, remove_ec);
                if (remove_ec) {
                    result.warnings.push_back(CachePruneWarning{
                        .kind = CachePruneWarningKind::PrunePackage,
                        .path = package_entry.path,
                        .message = remove_ec.message()});
                } else if (removed_count > 0) {
                    ++result.pruned_package_dirs;
                    result.pruned_package_paths.push_back(package_entry.path);
                }
            }

            std::error_code ec;
            if (std::filesystem::is_empty(revision_entry.path, ec) && !ec) {
                std::filesystem::remove(revision_entry.path, ec);
                if (ec) {
                    result.warnings.push_back(CachePruneWarning{
                        .kind = CachePruneWarningKind::RemoveEmptyRevision,
                        .path = revision_entry.path,
                        .message = ec.message()});
                }
            }
            ec.clear();
            if (!std::filesystem::exists(revision_entry.path, ec) || ec) {
                continue;
            }

            if (is_active_revision) {
                continue;
            }
            if (kept_non_active_revisions < static_cast<size_t>(max_revisions_per_world)) {
                ++kept_non_active_revisions;
                continue;
            }

            std::error_code remove_ec;
            const auto removed_count = std::filesystem::remove_all(revision_entry.path, remove_ec);
            if (remove_ec) {
                result.warnings.push_back(CachePruneWarning{
                    .kind = CachePruneWarningKind::PruneRevision,
                    .path = revision_entry.path,
                    .message = remove_ec.message()});
            } else if (removed_count > 0) {
                ++result.pruned_revision_dirs;
                result.pruned_revision_paths.push_back(revision_entry.path);
            }
        }

        std::error_code ec;
        if (std::filesystem::is_empty(world_entry.path, ec) && !ec) {
            std::filesystem::remove(world_entry.path, ec);
            if (ec) {
                result.warnings.push_back(CachePruneWarning{
                    .kind = CachePruneWarningKind::RemoveEmptyWorldDir,
                    .path = world_entry.path,
                    .message = ec.message()});
            }
        }
    }

    return result;
}

} // namespace karma::common::content

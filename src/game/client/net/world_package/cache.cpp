#include "client/net/world_package/internal.hpp"

#include "karma/common/data_path_resolver.hpp"
#include "karma/common/logging.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace bz3::client::net {

bool HasCachedWorldPackageForServer(const std::string& host,
                                    uint16_t port,
                                    std::string_view world_id,
                                    std::string_view world_revision,
                                    std::string_view world_content_hash,
                                    std::string_view world_hash) {
    if (world_id.empty() || world_revision.empty()) {
        return false;
    }

    try {
        const std::filesystem::path server_cache_dir =
            karma::data::EnsureUserWorldDirectoryForServer(host, port);
        const std::string world_package_cache_key =
            ResolveWorldPackageCacheKey(world_content_hash, world_hash);
        const std::filesystem::path package_root = PackageRootForIdentity(server_cache_dir,
                                                                          world_id,
                                                                          world_revision,
                                                                          world_package_cache_key);
        return std::filesystem::exists(package_root) && std::filesystem::is_directory(package_root);
    } catch (const std::exception& ex) {
        spdlog::warn("ClientConnection: failed to query cached package for {}:{} id='{}' rev='{}': {}",
                     host,
                     port,
                     world_id,
                     world_revision,
                     ex.what());
        return false;
    }
}


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

void PruneWorldPackageCache(const std::filesystem::path& server_cache_dir,
                            std::string_view active_world_id,
                            std::string_view active_world_revision,
                            std::string_view active_world_package_key) {
    // Keep retention policy engine-owned here so server-delivered world config cannot influence
    // cache pruning behavior via runtime layers.
    const uint16_t max_revisions_per_world = kDefaultMaxRevisionsPerWorld;
    const uint16_t max_packages_per_revision = kDefaultMaxPackagesPerRevision;

    const std::filesystem::path by_world_root = WorldPackagesByWorldRoot(server_cache_dir);
    const std::string active_world_dir = WorldCacheDirName(active_world_id);
    const std::string active_revision_dir = RevisionCacheDirName(active_world_revision);
    const std::string active_package_dir =
        active_world_package_key.empty() ? std::string("adhoc")
                                         : SanitizeCachePathComponent(active_world_package_key, "hash");
    size_t scanned_world_dirs = 0;
    size_t scanned_revision_dirs = 0;
    size_t scanned_package_dirs = 0;
    size_t pruned_revision_dirs = 0;
    size_t pruned_package_dirs = 0;

    for (const auto& world_entry : CollectDirectoryEntriesByMtimeDesc(by_world_root)) {
        ++scanned_world_dirs;
        const auto revision_entries = CollectDirectoryEntriesByMtimeDesc(world_entry.path);
        size_t kept_non_active_revisions = 0;
        for (const auto& revision_entry : revision_entries) {
            ++scanned_revision_dirs;
            const bool is_active_revision =
                world_entry.name == active_world_dir && revision_entry.name == active_revision_dir;

            const auto package_entries = CollectDirectoryEntriesByMtimeDesc(revision_entry.path);
            size_t kept_non_active_packages = 0;
            for (const auto& package_entry : package_entries) {
                ++scanned_package_dirs;
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
                    spdlog::warn("ClientConnection: failed to prune cached world package '{}': {}",
                                 package_entry.path.string(),
                                 remove_ec.message());
                } else if (removed_count > 0) {
                    ++pruned_package_dirs;
                    KARMA_TRACE("net.client",
                                "ClientConnection: pruned cached world package '{}'",
                                package_entry.path.string());
                }
            }

            std::error_code ec;
            if (std::filesystem::is_empty(revision_entry.path, ec) && !ec) {
                std::filesystem::remove(revision_entry.path, ec);
                if (ec) {
                    spdlog::warn("ClientConnection: failed to remove empty cached revision '{}': {}",
                                 revision_entry.path.string(),
                                 ec.message());
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
                spdlog::warn("ClientConnection: failed to prune cached world revision '{}': {}",
                             revision_entry.path.string(),
                             remove_ec.message());
            } else if (removed_count > 0) {
                ++pruned_revision_dirs;
                KARMA_TRACE("net.client",
                            "ClientConnection: pruned cached world revision '{}'",
                            revision_entry.path.string());
            }
        }

        std::error_code ec;
        if (std::filesystem::is_empty(world_entry.path, ec) && !ec) {
            std::filesystem::remove(world_entry.path, ec);
            if (ec) {
                spdlog::warn("ClientConnection: failed to remove empty cached world dir '{}': {}",
                             world_entry.path.string(),
                             ec.message());
            }
        }
    }

    KARMA_TRACE("net.client",
                "ClientConnection: cache prune summary worlds={} revisions={} packages={} pruned_revisions={} pruned_packages={}",
                scanned_world_dirs,
                scanned_revision_dirs,
                scanned_package_dirs,
                pruned_revision_dirs,
                pruned_package_dirs);
}


bool HasPackageIdentity(const CachedWorldIdentity& identity) {
    return !identity.world_hash.empty() || !identity.world_content_hash.empty();
}

bool HasRequiredIdentityFields(const CachedWorldIdentity& identity) {
    return !identity.world_id.empty() && !identity.world_revision.empty();
}


std::optional<CachedWorldIdentity> ReadCachedWorldIdentityFile(const std::filesystem::path& identity_file) {
    if (!std::filesystem::exists(identity_file) || !std::filesystem::is_regular_file(identity_file)) {
        return std::nullopt;
    }

    std::ifstream input(identity_file);
    if (!input) {
        return std::nullopt;
    }

    CachedWorldIdentity identity{};
    std::string line;
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

std::optional<CachedWorldIdentity> ReadCachedWorldIdentityForServer(const std::string& host, uint16_t port) {
    try {
        const auto server_cache_dir = karma::data::EnsureUserWorldDirectoryForServer(host, port);
        const auto identity_file = ActiveWorldIdentityPath(server_cache_dir);
        const auto identity = ReadCachedWorldIdentityFile(identity_file);
        if (!identity.has_value()) {
            std::error_code ec;
            std::filesystem::remove(identity_file, ec);
            KARMA_TRACE("net.client",
                        "ClientConnection: cached world identity ignored for {}:{} (missing/incomplete identity metadata)",
                        host,
                        port);
            return std::nullopt;
        }
        return identity.value();
    } catch (const std::exception& ex) {
        spdlog::warn("ClientConnection: failed to read cached world identity for {}:{}: {}",
                     host,
                     port,
                     ex.what());
        return std::nullopt;
    }
}

bool PersistCachedWorldIdentity(const std::filesystem::path& server_cache_dir,
                                std::string_view world_hash,
                                std::string_view world_content_hash,
                                std::string_view world_id,
                                std::string_view world_revision) {
    const auto identity_file = ActiveWorldIdentityPath(server_cache_dir);
    std::error_code ec;
    if (world_hash.empty() && world_content_hash.empty() && world_id.empty() && world_revision.empty()) {
        std::filesystem::remove(identity_file, ec);
        return !ec;
    }
    if (world_id.empty() || world_revision.empty() ||
        (world_hash.empty() && world_content_hash.empty())) {
        return false;
    }

    std::filesystem::create_directories(server_cache_dir, ec);
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

std::optional<CachedWorldIdentity> ReadCachedWorldIdentity(const std::filesystem::path& server_cache_dir) {
    const auto identity_file = ActiveWorldIdentityPath(server_cache_dir);
    return ReadCachedWorldIdentityFile(identity_file);
}


bool ValidateCachedWorldIdentity(const std::filesystem::path& server_cache_dir,
                                 std::string_view world_name,
                                 std::string_view expected_world_hash,
                                 std::string_view expected_world_content_hash,
                                 std::string_view expected_world_id,
                                 std::string_view expected_world_revision,
                                 std::string_view expected_world_manifest_hash,
                                 uint32_t expected_world_manifest_file_count,
                                 bool require_exact_revision) {
    const auto identity = ReadCachedWorldIdentity(server_cache_dir);
    if (!identity.has_value()) {
        spdlog::error("ClientConnection: cache identity metadata is missing for world '{}'", world_name);
        return false;
    }

    const bool id_match = identity->world_id == expected_world_id;
    const bool revision_match = identity->world_revision == expected_world_revision;
    const bool hash_match = !expected_world_hash.empty() && identity->world_hash == expected_world_hash;
    const bool content_hash_match = !expected_world_content_hash.empty() &&
                                    identity->world_content_hash == expected_world_content_hash;
    bool manifest_match = false;
    uint32_t cached_manifest_file_count = 0;
    std::string cached_manifest_hash{};
    if (!expected_world_manifest_hash.empty()) {
        const auto cached_manifest = ReadCachedWorldManifest(server_cache_dir);
        cached_manifest_file_count = static_cast<uint32_t>(cached_manifest.size());
        cached_manifest_hash = ComputeManifestHash(cached_manifest);
        manifest_match = cached_manifest_hash == expected_world_manifest_hash &&
                         cached_manifest_file_count == expected_world_manifest_file_count;
    }

    const bool package_match = hash_match || content_hash_match || manifest_match;
    if (!id_match || (require_exact_revision && !revision_match) || !package_match) {
        spdlog::error("ClientConnection: cache identity mismatch for world '{}' (expected hash='{}' content_hash='{}' id='{}' rev='{}' manifest_hash='{}' manifest_files={} require_exact_revision={}, got hash='{}' content_hash='{}' id='{}' rev='{}' manifest_hash='{}' manifest_files={})",
                      world_name,
                      expected_world_hash,
                      expected_world_content_hash,
                      expected_world_id,
                      expected_world_revision,
                      expected_world_manifest_hash,
                      expected_world_manifest_file_count,
                      require_exact_revision ? 1 : 0,
                      identity->world_hash,
                      identity->world_content_hash,
                      identity->world_id,
                      identity->world_revision,
                      cached_manifest_hash,
                      cached_manifest_file_count);
        return false;
    }
    return true;
}

void ClearCachedWorldIdentity(const std::filesystem::path& server_cache_dir) {
    static_cast<void>(PersistCachedWorldIdentity(server_cache_dir, "", "", "", ""));
    static_cast<void>(PersistCachedWorldManifest(server_cache_dir, {}));
}


} // namespace bz3::client::net

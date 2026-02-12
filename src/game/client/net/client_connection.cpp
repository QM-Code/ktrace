#include "client/net/client_connection.hpp"

#include "karma/network/client_reconnect_policy.hpp"
#include "karma/network/client_transport.hpp"
#include "karma/network/transport_config_mapping.hpp"
#include "karma/common/config_helpers.hpp"
#include "karma/common/config_store.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "karma/common/logging.hpp"
#include "karma/common/world_archive.hpp"
#include "net/protocol_codec.hpp"
#include "net/protocol.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <exception>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace bz3::client::net {

namespace {

constexpr const char* kRuntimeLayerLabel = "world package config";
constexpr const char* kPackageMountId = "world.package";
constexpr const char* kWorldIdentityFile = "active_world_identity.txt";
constexpr const char* kWorldManifestFile = "active_world_manifest.txt";
constexpr const char* kWorldPackagesDir = "world-packages";
constexpr const char* kWorldPackagesByWorldDir = "by-world";
constexpr const char* kDeltaRemovedPathsFile = "__bz3_delta_removed_paths.txt";
constexpr const char* kDeltaMetaFile = "__bz3_delta_meta.txt";
constexpr size_t kMaxCachePathComponentLen = 96;
constexpr uint16_t kDefaultMaxRevisionsPerWorld = 4;
constexpr uint16_t kDefaultMaxPackagesPerRevision = 2;

struct CachedWorldIdentity {
    std::string world_hash{};
    std::string world_content_hash{};
    std::string world_id{};
    std::string world_revision{};
};

std::filesystem::path ActiveWorldIdentityPath(const std::filesystem::path& server_cache_dir) {
    return server_cache_dir / kWorldIdentityFile;
}

std::filesystem::path ActiveWorldManifestPath(const std::filesystem::path& server_cache_dir) {
    return server_cache_dir / kWorldManifestFile;
}

std::filesystem::path WorldPackagesByWorldRoot(const std::filesystem::path& server_cache_dir) {
    return server_cache_dir / kWorldPackagesDir / kWorldPackagesByWorldDir;
}

uint64_t HashStringFNV1a(std::string_view value) {
    uint64_t hash = 14695981039346656037ULL;
    for (const char ch : value) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(ch));
        hash *= 1099511628211ULL;
    }
    return hash;
}

void HashStringFNV1a(uint64_t& hash, std::string_view value) {
    for (const char ch : value) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(ch));
        hash *= 1099511628211ULL;
    }
}

void HashBytesFNV1a(uint64_t& hash, const std::byte* bytes, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        hash ^= static_cast<uint64_t>(std::to_integer<unsigned char>(bytes[i]));
        hash *= 1099511628211ULL;
    }
}

void HashBytesFNV1a(uint64_t& hash, std::string_view value) {
    const auto* bytes = reinterpret_cast<const std::byte*>(value.data());
    HashBytesFNV1a(hash, bytes, value.size());
}

void HashSeparatorFNV1a(uint64_t& hash) {
    hash ^= static_cast<uint64_t>(0);
    hash *= 1099511628211ULL;
}

std::string Hash64Hex(uint64_t hash) {
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

bool IsChunkInTransferBounds(uint64_t total_bytes,
                             uint32_t chunk_size,
                             uint32_t chunk_index,
                             size_t chunk_bytes) {
    if (chunk_size == 0) {
        return false;
    }
    const uint64_t chunk_offset = static_cast<uint64_t>(chunk_index) * chunk_size;
    if (chunk_offset > total_bytes) {
        return false;
    }
    const uint64_t remaining_bytes = total_bytes - chunk_offset;
    return static_cast<uint64_t>(chunk_bytes) <= remaining_bytes;
}

bool ChunkMatchesBufferedPayload(const std::vector<std::byte>& payload,
                                 size_t chunk_offset,
                                 const std::vector<std::byte>& chunk_data) {
    if (chunk_offset > payload.size()) {
        return false;
    }
    if (chunk_data.size() > (payload.size() - chunk_offset)) {
        return false;
    }
    return std::equal(chunk_data.begin(),
                      chunk_data.end(),
                      payload.begin() + static_cast<std::ptrdiff_t>(chunk_offset));
}

std::string SanitizeCachePathComponent(std::string_view input, std::string_view fallback_prefix) {
    std::string sanitized{};
    sanitized.reserve(input.size());
    for (const char ch : input) {
        const auto uch = static_cast<unsigned char>(ch);
        if (std::isalnum(uch) || ch == '-' || ch == '_' || ch == '.') {
            sanitized.push_back(ch);
        } else {
            sanitized.push_back('_');
        }
        if (sanitized.size() >= kMaxCachePathComponentLen) {
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

std::filesystem::path PackageRootForIdentity(const std::filesystem::path& server_cache_dir,
                                             std::string_view world_id,
                                             std::string_view world_revision,
                                             std::string_view world_package_cache_key) {
    const std::string world_dir = SanitizeCachePathComponent(world_id, "world");
    const std::string revision_dir = SanitizeCachePathComponent(world_revision, "rev");
    const std::string package_dir =
        world_package_cache_key.empty() ? std::string("adhoc")
                                        : SanitizeCachePathComponent(world_package_cache_key, "hash");
    return server_cache_dir / kWorldPackagesDir / kWorldPackagesByWorldDir / world_dir / revision_dir / package_dir;
}

std::string ResolveWorldPackageCacheKey(std::string_view world_content_hash, std::string_view world_hash) {
    if (!world_content_hash.empty()) {
        return std::string(world_content_hash);
    }
    if (!world_hash.empty()) {
        return std::string(world_hash);
    }
    return "adhoc";
}

struct WorldDirectoryManifestSummary {
    std::string content_hash{};
    std::string manifest_hash{};
    std::vector<bz3::net::WorldManifestEntry> entries{};
};

std::optional<WorldDirectoryManifestSummary> ComputeWorldDirectoryManifestSummary(
    const std::filesystem::path& package_root) {
    try {
        std::vector<std::filesystem::path> files{};
        for (const auto& entry : std::filesystem::recursive_directory_iterator(package_root)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            files.push_back(entry.path());
        }
        std::sort(files.begin(), files.end());

        uint64_t content_hash = 14695981039346656037ULL;
        uint64_t manifest_hash = 14695981039346656037ULL;
        std::array<char, 64 * 1024> buffer{};
        const std::byte separator = std::byte{0};
        std::vector<bz3::net::WorldManifestEntry> entries{};
        entries.reserve(files.size());

        for (const auto& file_path : files) {
            const std::filesystem::path rel_path =
                std::filesystem::relative(file_path, package_root);
            const std::string rel = rel_path.generic_string();
            HashStringFNV1a(content_hash, rel);
            HashBytesFNV1a(content_hash, &separator, 1);

            std::ifstream input(file_path, std::ios::binary);
            if (!input) {
                spdlog::error("ClientConnection: failed to open extracted file for verification '{}'",
                              file_path.string());
                return std::nullopt;
            }

            uint64_t file_hash = 14695981039346656037ULL;
            uint64_t file_size = 0;
            while (input.good()) {
                input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
                const std::streamsize read_count = input.gcount();
                if (read_count > 0) {
                    const auto* bytes = reinterpret_cast<const std::byte*>(buffer.data());
                    HashBytesFNV1a(content_hash, bytes, static_cast<size_t>(read_count));
                    HashBytesFNV1a(file_hash, bytes, static_cast<size_t>(read_count));
                    file_size += static_cast<uint64_t>(read_count);
                }
            }
            if (!input.eof()) {
                spdlog::error("ClientConnection: failed while hashing extracted file '{}'",
                              file_path.string());
                return std::nullopt;
            }
            HashBytesFNV1a(content_hash, &separator, 1);

            const std::string file_hash_hex = Hash64Hex(file_hash);
            const std::string file_size_text = std::to_string(file_size);
            HashStringFNV1a(manifest_hash, rel);
            HashBytesFNV1a(manifest_hash, &separator, 1);
            HashStringFNV1a(manifest_hash, file_size_text);
            HashBytesFNV1a(manifest_hash, &separator, 1);
            HashStringFNV1a(manifest_hash, file_hash_hex);
            HashBytesFNV1a(manifest_hash, &separator, 1);

            entries.push_back(bz3::net::WorldManifestEntry{
                .path = rel,
                .size = file_size,
                .hash = file_hash_hex});
        }

        return WorldDirectoryManifestSummary{
            .content_hash = Hash64Hex(content_hash),
            .manifest_hash = Hash64Hex(manifest_hash),
            .entries = std::move(entries)};
    } catch (const std::exception& ex) {
        spdlog::error("ClientConnection: exception while verifying extracted world package '{}': {}",
                      package_root.string(),
                      ex.what());
        return std::nullopt;
    }
}

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

std::string ComputeManifestHash(const std::vector<bz3::net::WorldManifestEntry>& manifest) {
    if (manifest.empty()) {
        return {};
    }

    std::vector<const bz3::net::WorldManifestEntry*> ordered_entries{};
    ordered_entries.reserve(manifest.size());
    for (const auto& entry : manifest) {
        ordered_entries.push_back(&entry);
    }
    std::sort(ordered_entries.begin(),
              ordered_entries.end(),
              [](const bz3::net::WorldManifestEntry* lhs,
                 const bz3::net::WorldManifestEntry* rhs) {
                  if (lhs->path != rhs->path) {
                      return lhs->path < rhs->path;
                  }
                  if (lhs->size != rhs->size) {
                      return lhs->size < rhs->size;
                  }
                  return lhs->hash < rhs->hash;
              });

    uint64_t hash = 14695981039346656037ULL;
    for (const auto* entry : ordered_entries) {
        HashBytesFNV1a(hash, entry->path);
        HashSeparatorFNV1a(hash);
        HashBytesFNV1a(hash, std::to_string(entry->size));
        HashSeparatorFNV1a(hash);
        HashBytesFNV1a(hash, entry->hash);
        HashSeparatorFNV1a(hash);
    }

    return Hash64Hex(hash);
}

std::string WorldCacheDirName(std::string_view world_id) {
    return SanitizeCachePathComponent(world_id, "world");
}

std::string RevisionCacheDirName(std::string_view world_revision) {
    return SanitizeCachePathComponent(world_revision, "rev");
}

std::filesystem::path BuildPackageStagingRoot(const std::filesystem::path& package_root) {
    const auto nonce = static_cast<unsigned long long>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    return package_root.parent_path() /
           (package_root.filename().string() + ".staging-" + std::to_string(nonce));
}

std::filesystem::path BuildPackageBackupRoot(const std::filesystem::path& package_root) {
    const auto nonce = static_cast<unsigned long long>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    return package_root.parent_path() /
           (package_root.filename().string() + ".previous-" + std::to_string(nonce));
}

void CleanupStaleTemporaryDirectories(const std::filesystem::path& package_root) {
    const std::filesystem::path parent = package_root.parent_path();
    if (parent.empty() || !std::filesystem::exists(parent) || !std::filesystem::is_directory(parent)) {
        return;
    }

    const std::string staging_prefix = package_root.filename().string() + ".staging-";
    const std::string previous_prefix = package_root.filename().string() + ".previous-";
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(parent, ec)) {
        if (ec) {
            return;
        }
        if (!entry.is_directory()) {
            continue;
        }
        const auto name = entry.path().filename().string();
        const bool stale_staging = name.rfind(staging_prefix, 0) == 0;
        const bool stale_previous = name.rfind(previous_prefix, 0) == 0;
        if (!stale_staging && !stale_previous) {
            continue;
        }

        std::error_code remove_ec;
        std::filesystem::remove_all(entry.path(), remove_ec);
        if (remove_ec) {
            spdlog::warn("ClientConnection: failed to remove stale package temp dir '{}': {}",
                         entry.path().string(),
                         remove_ec.message());
        }
    }
}

bool ActivateStagedPackageRootAtomically(const std::filesystem::path& package_root,
                                         const std::filesystem::path& staging_root) {
    const std::filesystem::path backup_root = BuildPackageBackupRoot(package_root);
    std::error_code ec;
    bool moved_existing_root = false;
    const bool package_root_exists = std::filesystem::exists(package_root, ec);
    if (ec) {
        spdlog::error("ClientConnection: failed to query package directory '{}': {}",
                      package_root.string(),
                      ec.message());
        return false;
    }

    if (package_root_exists) {
        std::filesystem::remove_all(backup_root, ec);
        ec.clear();
        std::filesystem::rename(package_root, backup_root, ec);
        if (ec) {
            spdlog::error("ClientConnection: failed to move package '{}' -> '{}': {}",
                          package_root.string(),
                          backup_root.string(),
                          ec.message());
            return false;
        }
        moved_existing_root = true;
    }

    ec.clear();
    std::filesystem::rename(staging_root, package_root, ec);
    if (ec) {
        std::error_code cleanup_ec;
        std::filesystem::remove_all(staging_root, cleanup_ec);
        if (moved_existing_root) {
            std::error_code rollback_ec;
            std::filesystem::rename(backup_root, package_root, rollback_ec);
            if (rollback_ec) {
                spdlog::error("ClientConnection: failed to rollback package '{}' -> '{}' after activation failure: {}",
                              backup_root.string(),
                              package_root.string(),
                              rollback_ec.message());
            }
        }
        spdlog::error("ClientConnection: failed to activate staged package '{}' -> '{}': {}",
                      staging_root.string(),
                      package_root.string(),
                      ec.message());
        return false;
    }

    if (moved_existing_root) {
        std::error_code remove_ec;
        std::filesystem::remove_all(backup_root, remove_ec);
        if (remove_ec) {
            spdlog::warn("ClientConnection: failed to remove previous package backup '{}': {}",
                         backup_root.string(),
                         remove_ec.message());
        }
    }

    CleanupStaleTemporaryDirectories(package_root);
    return true;
}

std::vector<bz3::net::WorldManifestEntry> SortedManifestEntries(
    const std::vector<bz3::net::WorldManifestEntry>& entries) {
    auto ordered = entries;
    std::sort(ordered.begin(),
              ordered.end(),
              [](const bz3::net::WorldManifestEntry& lhs,
                 const bz3::net::WorldManifestEntry& rhs) {
                  if (lhs.path != rhs.path) {
                      return lhs.path < rhs.path;
                  }
                  if (lhs.size != rhs.size) {
                      return lhs.size < rhs.size;
                  }
                  return lhs.hash < rhs.hash;
              });
    return ordered;
}

bool ManifestEntriesEqual(const std::vector<bz3::net::WorldManifestEntry>& lhs,
                          const std::vector<bz3::net::WorldManifestEntry>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (size_t i = 0; i < lhs.size(); ++i) {
        if (lhs[i].path != rhs[i].path || lhs[i].size != rhs[i].size || lhs[i].hash != rhs[i].hash) {
            return false;
        }
    }
    return true;
}

bool VerifyExtractedWorldPackage(const std::filesystem::path& package_root,
                                 std::string_view world_name,
                                 std::string_view expected_world_content_hash,
                                 std::string_view expected_world_manifest_hash,
                                 uint32_t expected_world_manifest_file_count,
                                 const std::vector<bz3::net::WorldManifestEntry>& expected_world_manifest,
                                 std::string_view stage_name) {
    const auto summary = ComputeWorldDirectoryManifestSummary(package_root);
    if (!summary.has_value()) {
        spdlog::error("ClientConnection: failed to verify {} world package '{}' at '{}'",
                      stage_name,
                      world_name,
                      package_root.string());
        return false;
    }

    if (!expected_world_content_hash.empty() &&
        summary->content_hash != expected_world_content_hash) {
        spdlog::error("ClientConnection: {} content hash mismatch for world '{}' (expected='{}' got='{}')",
                      stage_name,
                      world_name,
                      expected_world_content_hash,
                      summary->content_hash);
        return false;
    }

    if (!expected_world_manifest_hash.empty() &&
        summary->manifest_hash != expected_world_manifest_hash) {
        spdlog::error("ClientConnection: {} manifest hash mismatch for world '{}' (expected='{}' got='{}')",
                      stage_name,
                      world_name,
                      expected_world_manifest_hash,
                      summary->manifest_hash);
        return false;
    }

    if (expected_world_manifest_file_count > 0 &&
        summary->entries.size() != expected_world_manifest_file_count) {
        spdlog::error("ClientConnection: {} manifest file count mismatch for world '{}' (expected={} got={})",
                      stage_name,
                      world_name,
                      expected_world_manifest_file_count,
                      summary->entries.size());
        return false;
    }

    if (!expected_world_manifest.empty()) {
        const auto expected = SortedManifestEntries(expected_world_manifest);
        const auto actual = SortedManifestEntries(summary->entries);
        if (!ManifestEntriesEqual(expected, actual)) {
            spdlog::error("ClientConnection: {} manifest entries mismatch for world '{}' (expected_entries={} got_entries={})",
                          stage_name,
                          world_name,
                          expected.size(),
                          actual.size());
            return false;
        }
    }

    KARMA_TRACE("net.client",
                "ClientConnection: verified {} world package world='{}' content_hash='{}' manifest_hash='{}' files={}",
                stage_name,
                world_name,
                summary->content_hash,
                summary->manifest_hash,
                summary->entries.size());
    return true;
}

bool ExtractWorldArchiveAtomically(const std::vector<std::byte>& world_data,
                                   const std::filesystem::path& package_root,
                                   std::string_view world_name,
                                   std::string_view expected_world_content_hash,
                                   std::string_view expected_world_manifest_hash,
                                   uint32_t expected_world_manifest_file_count,
                                   const std::vector<bz3::net::WorldManifestEntry>& expected_world_manifest) {
    const std::filesystem::path staging_root = BuildPackageStagingRoot(package_root);
    std::error_code ec;
    std::filesystem::remove_all(staging_root, ec);
    ec.clear();
    std::filesystem::create_directories(staging_root, ec);
    if (ec) {
        spdlog::error("ClientConnection: failed to create staging directory '{}': {}",
                      staging_root.string(),
                      ec.message());
        return false;
    }

    if (!world::ExtractWorldArchive(world_data, staging_root)) {
        std::filesystem::remove_all(staging_root, ec);
        spdlog::error("ClientConnection: failed to extract world archive into staging '{}'",
                      staging_root.string());
        return false;
    }

    if (!VerifyExtractedWorldPackage(staging_root,
                                     world_name,
                                     expected_world_content_hash,
                                     expected_world_manifest_hash,
                                     expected_world_manifest_file_count,
                                     expected_world_manifest,
                                     "staged full")) {
        std::filesystem::remove_all(staging_root, ec);
        return false;
    }

    return ActivateStagedPackageRootAtomically(package_root, staging_root);
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

std::string ComputeWorldPackageHash(const std::vector<std::byte>& bytes) {
    uint64_t hash = 14695981039346656037ULL;
    for (const std::byte value : bytes) {
        hash ^= static_cast<uint64_t>(std::to_integer<unsigned char>(value));
        hash *= 1099511628211ULL;
    }

    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

bool HasPackageIdentity(const CachedWorldIdentity& identity) {
    return !identity.world_hash.empty() || !identity.world_content_hash.empty();
}

bool HasRequiredIdentityFields(const CachedWorldIdentity& identity) {
    return !identity.world_id.empty() && !identity.world_revision.empty();
}

std::vector<bz3::net::WorldManifestEntry> ReadCachedWorldManifest(
    const std::filesystem::path& server_cache_dir) {
    const auto manifest_file = ActiveWorldManifestPath(server_cache_dir);
    if (!std::filesystem::exists(manifest_file) || !std::filesystem::is_regular_file(manifest_file)) {
        return {};
    }

    std::ifstream input(manifest_file);
    if (!input) {
        return {};
    }

    std::vector<bz3::net::WorldManifestEntry> manifest{};
    std::string path{};
    uint64_t size = 0;
    std::string hash{};
    while (input >> std::quoted(path) >> size >> std::quoted(hash)) {
        manifest.push_back(bz3::net::WorldManifestEntry{
            .path = path,
            .size = size,
            .hash = hash});
    }

    if (!input.eof()) {
        spdlog::warn("ClientConnection: cached world manifest '{}' is malformed; ignoring",
                     manifest_file.string());
        return {};
    }
    return manifest;
}

bool PersistCachedWorldManifest(const std::filesystem::path& server_cache_dir,
                                const std::vector<bz3::net::WorldManifestEntry>& manifest) {
    const auto manifest_file = ActiveWorldManifestPath(server_cache_dir);
    std::error_code ec;
    if (manifest.empty()) {
        std::filesystem::remove(manifest_file, ec);
        return !ec;
    }

    std::filesystem::create_directories(server_cache_dir, ec);
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

void LogManifestDiffPlan(std::string_view world_name,
                         const std::vector<bz3::net::WorldManifestEntry>& cached_manifest,
                         const std::vector<bz3::net::WorldManifestEntry>& incoming_manifest) {
    if (incoming_manifest.empty()) {
        KARMA_TRACE("net.client",
                    "ClientConnection: manifest diff plan skipped world='{}' (incoming manifest unavailable, cached_entries={})",
                    world_name,
                    cached_manifest.size());
        return;
    }

    uint64_t incoming_bytes = 0;
    for (const auto& entry : incoming_manifest) {
        incoming_bytes += entry.size;
    }

    if (cached_manifest.empty()) {
        KARMA_TRACE("net.client",
                    "ClientConnection: manifest diff plan world='{}' cached_entries=0 incoming_entries={} unchanged=0 added={} modified=0 removed=0 potential_transfer_bytes={} reused_bytes=0",
                    world_name,
                    incoming_manifest.size(),
                    incoming_manifest.size(),
                    incoming_bytes);
        return;
    }

    std::unordered_map<std::string, const bz3::net::WorldManifestEntry*> cached_by_path{};
    cached_by_path.reserve(cached_manifest.size());
    for (const auto& entry : cached_manifest) {
        cached_by_path[entry.path] = &entry;
    }

    size_t unchanged = 0;
    size_t added = 0;
    size_t modified = 0;
    uint64_t potential_transfer_bytes = 0;
    uint64_t reused_bytes = 0;
    for (const auto& entry : incoming_manifest) {
        const auto it = cached_by_path.find(entry.path);
        if (it == cached_by_path.end()) {
            ++added;
            potential_transfer_bytes += entry.size;
            continue;
        }

        const auto& cached_entry = *it->second;
        if (cached_entry.size == entry.size && cached_entry.hash == entry.hash) {
            ++unchanged;
            reused_bytes += entry.size;
        } else {
            ++modified;
            potential_transfer_bytes += entry.size;
        }
        cached_by_path.erase(it);
    }

    const size_t removed = cached_by_path.size();
    KARMA_TRACE("net.client",
                "ClientConnection: manifest diff plan world='{}' cached_entries={} incoming_entries={} unchanged={} added={} modified={} removed={} potential_transfer_bytes={} reused_bytes={}",
                world_name,
                cached_manifest.size(),
                incoming_manifest.size(),
                unchanged,
                added,
                modified,
                removed,
                potential_transfer_bytes,
                reused_bytes);
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

bool NormalizeRelativePath(std::string_view raw_path, std::filesystem::path* out) {
    if (!out || raw_path.empty()) {
        return false;
    }
    std::filesystem::path normalized = std::filesystem::path(raw_path).lexically_normal();
    if (normalized.empty() || normalized == "." || normalized.is_absolute() ||
        normalized.has_root_path()) {
        return false;
    }
    for (const auto& part : normalized) {
        if (part == "..") {
            return false;
        }
    }
    *out = normalized;
    return true;
}

bool ApplyDeltaArchiveOverCachedBase(const std::filesystem::path& server_cache_dir,
                                     std::string_view world_name,
                                     std::string_view world_id,
                                     std::string_view world_revision,
                                     std::string_view world_hash,
                                     std::string_view world_content_hash,
                                     std::string_view world_manifest_hash,
                                     uint32_t world_manifest_file_count,
                                     const std::vector<bz3::net::WorldManifestEntry>& world_manifest,
                                     std::string_view base_world_id,
                                     std::string_view base_world_revision,
                                     std::string_view base_world_hash,
                                     std::string_view base_world_content_hash,
                                     const std::vector<std::byte>& delta_archive) {
    if (base_world_id.empty() || base_world_revision.empty()) {
        spdlog::error("ClientConnection: delta transfer missing base world identity metadata");
        return false;
    }
    if (base_world_id != world_id) {
        spdlog::error("ClientConnection: delta transfer base world id mismatch target='{}' base='{}'",
                      world_id,
                      base_world_id);
        return false;
    }

    const std::string target_package_cache_key = ResolveWorldPackageCacheKey(world_content_hash, world_hash);
    const std::filesystem::path target_root =
        PackageRootForIdentity(server_cache_dir, world_id, world_revision, target_package_cache_key);
    const std::filesystem::path staging_root = BuildPackageStagingRoot(target_root);
    const std::string base_package_cache_key =
        ResolveWorldPackageCacheKey(base_world_content_hash, base_world_hash);
    const std::filesystem::path base_root = PackageRootForIdentity(server_cache_dir,
                                                                   base_world_id,
                                                                   base_world_revision,
                                                                   base_package_cache_key);
    if (!std::filesystem::exists(base_root) || !std::filesystem::is_directory(base_root)) {
        spdlog::error("ClientConnection: delta base world package is missing '{}' for id='{}' rev='{}'",
                      base_root.string(),
                      base_world_id,
                      base_world_revision);
        return false;
    }

    std::error_code ec;
    std::filesystem::remove_all(staging_root, ec);
    ec.clear();
    std::filesystem::create_directories(staging_root.parent_path(), ec);
    if (ec) {
        spdlog::error("ClientConnection: failed to create target package parent '{}': {}",
                      staging_root.parent_path().string(),
                      ec.message());
        return false;
    }

    std::filesystem::copy(base_root, staging_root, std::filesystem::copy_options::recursive, ec);
    if (ec) {
        spdlog::error("ClientConnection: failed to clone delta base package '{}' -> '{}': {}",
                      base_root.string(),
                      staging_root.string(),
                      ec.message());
        return false;
    }

    if (!world::ExtractWorldArchive(delta_archive, staging_root)) {
        spdlog::error("ClientConnection: failed to extract world delta archive into '{}'",
                      staging_root.string());
        std::filesystem::remove_all(staging_root, ec);
        return false;
    }

    size_t removed_paths = 0;
    const std::filesystem::path removed_paths_file = staging_root / kDeltaRemovedPathsFile;
    if (std::filesystem::exists(removed_paths_file) && std::filesystem::is_regular_file(removed_paths_file)) {
        std::ifstream removals_in(removed_paths_file);
        if (!removals_in) {
            spdlog::error("ClientConnection: failed to read delta removals file '{}'",
                          removed_paths_file.string());
            std::filesystem::remove_all(staging_root, ec);
            return false;
        }
        std::string line{};
        while (std::getline(removals_in, line)) {
            if (line.empty()) {
                continue;
            }
            std::filesystem::path normalized_rel{};
            if (!NormalizeRelativePath(line, &normalized_rel)) {
                spdlog::error("ClientConnection: invalid delta removal path '{}'", line);
                std::filesystem::remove_all(staging_root, ec);
                return false;
            }
            std::error_code remove_ec;
            std::filesystem::remove_all(staging_root / normalized_rel, remove_ec);
            if (remove_ec) {
                spdlog::error("ClientConnection: failed to apply delta removal path '{}' in '{}': {}",
                              line,
                              staging_root.string(),
                              remove_ec.message());
                std::filesystem::remove_all(staging_root, ec);
                return false;
            }
            ++removed_paths;
        }
    }

    std::filesystem::remove(staging_root / kDeltaRemovedPathsFile, ec);
    ec.clear();
    std::filesystem::remove(staging_root / kDeltaMetaFile, ec);
    ec.clear();

    if (!VerifyExtractedWorldPackage(staging_root,
                                     world_name,
                                     world_content_hash,
                                     world_manifest_hash,
                                     world_manifest_file_count,
                                     world_manifest,
                                     "staged delta")) {
        std::filesystem::remove_all(staging_root, ec);
        return false;
    }

    if (!ActivateStagedPackageRootAtomically(target_root, staging_root)) {
        std::filesystem::remove_all(staging_root, ec);
        return false;
    }

    KARMA_TRACE("net.client",
                "ClientConnection: applied world delta archive target='{}' base='{}' removed_paths={}",
                target_root.string(),
                base_root.string(),
                removed_paths);
    return true;
}

bool ValidateCachedWorldIdentity(const std::filesystem::path& server_cache_dir,
                                 std::string_view world_name,
                                 std::string_view expected_world_hash,
                                 std::string_view expected_world_content_hash,
                                 std::string_view expected_world_id,
                                 std::string_view expected_world_revision,
                                 std::string_view expected_world_manifest_hash,
                                 uint32_t expected_world_manifest_file_count,
                                 bool require_exact_revision = true) {
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

bool ApplyWorldPackageForServer(const std::string& host,
                                uint16_t port,
                                std::string_view world_name,
                                std::string_view world_id,
                                std::string_view world_revision,
                                std::string_view world_hash,
                                std::string_view world_content_hash,
                                std::string_view world_manifest_hash,
                                uint32_t world_manifest_file_count,
                                uint64_t world_size,
                                const std::vector<bz3::net::WorldManifestEntry>& world_manifest,
                                const std::vector<std::byte>& world_data,
                                bool is_delta_transfer = false,
                                std::string_view delta_base_world_id = {},
                                std::string_view delta_base_world_revision = {},
                                std::string_view delta_base_world_hash = {},
                                std::string_view delta_base_world_content_hash = {}) {
    const std::filesystem::path server_cache_dir =
        karma::data::EnsureUserWorldDirectoryForServer(host, port);
    if (world_id.empty() || world_revision.empty()) {
        spdlog::error("ClientConnection: missing world identity metadata for world '{}' (id='{}' rev='{}')",
                      world_name,
                      world_id,
                      world_revision);
        return false;
    }

    if (world_data.empty() && world_hash.empty() && world_content_hash.empty()) {
        static_cast<void>(karma::config::ConfigStore::RemoveRuntimeLayer(kRuntimeLayerLabel));
        karma::data::ClearPackageMounts();
        ClearCachedWorldIdentity(server_cache_dir);
        KARMA_TRACE("net.client",
                    "ClientConnection: bundled world mode '{}' id='{}' rev='{}' (no world package transfer)",
                    world_name,
                    world_id,
                    world_revision);
        return true;
    }

    const auto cached_manifest = ReadCachedWorldManifest(server_cache_dir);
    std::vector<bz3::net::WorldManifestEntry> effective_world_manifest{};
    if (!world_manifest.empty()) {
        effective_world_manifest = world_manifest;
    } else {
        const bool can_reuse_cached_manifest = world_data.empty() &&
                                               !world_manifest_hash.empty() &&
                                               world_manifest_file_count > 0 &&
                                               !cached_manifest.empty() &&
                                               ComputeManifestHash(cached_manifest) == world_manifest_hash &&
                                               cached_manifest.size() == world_manifest_file_count;
        if (can_reuse_cached_manifest) {
            effective_world_manifest = cached_manifest;
            KARMA_TRACE("net.client",
                        "ClientConnection: init omitted manifest entries for world='{}'; reusing cached manifest entries={} hash='{}'",
                        world_name,
                        effective_world_manifest.size(),
                        world_manifest_hash);
        } else if (world_data.empty() && !world_manifest_hash.empty() && world_manifest_file_count > 0) {
            spdlog::warn("ClientConnection: init omitted manifest entries for world '{}' but cached manifest is unavailable/mismatched (manifest_hash='{}' manifest_files={})",
                         world_name,
                         world_manifest_hash,
                         world_manifest_file_count);
        }
    }
    LogManifestDiffPlan(world_name, cached_manifest, effective_world_manifest);

    const std::string world_package_cache_key = ResolveWorldPackageCacheKey(world_content_hash, world_hash);
    const std::filesystem::path package_root =
        PackageRootForIdentity(server_cache_dir, world_id, world_revision, world_package_cache_key);

    if (!world_data.empty()) {
        if (is_delta_transfer) {
            if (!ApplyDeltaArchiveOverCachedBase(server_cache_dir,
                                                 world_name,
                                                 world_id,
                                                 world_revision,
                                                 world_hash,
                                                 world_content_hash,
                                                 world_manifest_hash,
                                                 world_manifest_file_count,
                                                 effective_world_manifest,
                                                 delta_base_world_id,
                                                 delta_base_world_revision,
                                                 delta_base_world_hash,
                                                 delta_base_world_content_hash,
                                                 world_data)) {
                spdlog::error("ClientConnection: failed to apply world delta package for world '{}' (base id='{}' rev='{}')",
                              world_name,
                              delta_base_world_id,
                              delta_base_world_revision);
                return false;
            }
        } else {
            if (world_size > 0 && world_size != world_data.size()) {
                spdlog::error("ClientConnection: world package size mismatch for '{}' (expected={} got={})",
                              world_name,
                              world_size,
                              world_data.size());
                return false;
            }
            if (!world_hash.empty()) {
                const std::string computed_hash = ComputeWorldPackageHash(world_data);
                if (computed_hash != world_hash) {
                    spdlog::error("ClientConnection: world package hash mismatch for '{}' (expected={} got={})",
                                  world_name,
                                  world_hash,
                                  computed_hash);
                    return false;
                }
            }

            if (!ExtractWorldArchiveAtomically(world_data,
                                               package_root,
                                               world_name,
                                               world_content_hash,
                                               world_manifest_hash,
                                               world_manifest_file_count,
                                               effective_world_manifest)) {
                spdlog::error("ClientConnection: failed to extract world package for world '{}'", world_name);
                return false;
            }
        }
    } else {
        if (!std::filesystem::exists(package_root) || !std::filesystem::is_directory(package_root)) {
            spdlog::error("ClientConnection: server skipped world package transfer for '{}' hash='{}' content_hash='{}', but cache is missing",
                          world_name,
                          world_hash,
                          world_content_hash);
            ClearCachedWorldIdentity(server_cache_dir);
            return false;
        }
        if (!ValidateCachedWorldIdentity(server_cache_dir,
                                         world_name,
                                         world_hash,
                                         world_content_hash,
                                         world_id,
                                         world_revision,
                                         world_manifest_hash,
                                         world_manifest_file_count,
                                         true)) {
            ClearCachedWorldIdentity(server_cache_dir);
            return false;
        }
    }

    auto world_config = world::ReadWorldJsonFile(package_root / "config.json");
    if (world_config.has_value() && !world_config->is_object()) {
        spdlog::error("ClientConnection: world package config.json is not a JSON object for world '{}'",
                      world_name);
        return false;
    }

    static_cast<void>(karma::config::ConfigStore::RemoveRuntimeLayer(kRuntimeLayerLabel));
    karma::data::ClearPackageMounts();
    karma::data::RegisterPackageMount(kPackageMountId, package_root);

    if (world_config.has_value() &&
        !karma::config::ConfigStore::AddRuntimeLayer(kRuntimeLayerLabel, *world_config, package_root)) {
        karma::data::ClearPackageMounts();
        spdlog::error("ClientConnection: failed to add runtime layer for world '{}'", world_name);
        return false;
    }

    if (!PersistCachedWorldIdentity(server_cache_dir,
                                    world_hash,
                                    world_content_hash,
                                    world_id,
                                    world_revision)) {
        spdlog::warn("ClientConnection: failed to persist cached world identity hash='{}' content_hash='{}' id='{}' rev='{}' for {}:{}",
                     world_hash,
                     world_content_hash,
                     world_id,
                     world_revision,
                     host,
                     port);
    }
    if (!PersistCachedWorldManifest(server_cache_dir, effective_world_manifest)) {
        spdlog::warn("ClientConnection: failed to persist cached world manifest entries={} for {}:{}",
                     effective_world_manifest.size(),
                     host,
                     port);
    }

    TouchPathIfPresent(package_root);
    TouchPathIfPresent(package_root.parent_path());
    TouchPathIfPresent(package_root.parent_path().parent_path());
    PruneWorldPackageCache(server_cache_dir,
                           world_id,
                           world_revision,
                           world_package_cache_key);

    KARMA_TRACE("net.client",
                "ClientConnection: applied world package world='{}' id='{}' rev='{}' hash='{}' content_hash='{}' bytes={} manifest_entries={} cache='{}' cache_hit={} transfer_mode={}",
                world_name,
                world_id,
                world_revision,
                world_hash.empty() ? "-" : std::string(world_hash),
                world_content_hash.empty() ? "-" : std::string(world_content_hash),
                world_data.size(),
                effective_world_manifest.size(),
                package_root.string(),
                world_data.empty() ? 1 : 0,
                world_data.empty() ? "none" : (is_delta_transfer ? "delta" : "full"));
    return true;
}

} // namespace

ClientConnection::ClientConnection(std::string host,
                                   uint16_t port,
                                   std::string player_name,
                                   AudioEventCallback audio_event_callback)
    : host_(std::move(host)),
      port_(port),
      player_name_(std::move(player_name)),
      audio_event_callback_(std::move(audio_event_callback)) {}

ClientConnection::~ClientConnection() {
    shutdown();
}

bool ClientConnection::start() {
    if (started_) {
        return connected_;
    }
    started_ = true;

    if (host_.empty() || port_ == 0) {
        KARMA_TRACE("net.client",
                    "ClientConnection: startup skipped (missing host/port)");
        return false;
    }

    bool custom_backend = false;
    karma::network::ClientTransportConfig transport_config =
        karma::network::ResolveClientTransportConfigFromConfig(&custom_backend);
    if (custom_backend) {
        KARMA_TRACE("net.client",
                    "ClientConnection: using custom client transport backend='{}'",
                    transport_config.backend_name);
    }

    transport_ = karma::network::CreateClientTransport(transport_config);
    if (!transport_ || !transport_->isReady()) {
        const std::string configured_backend =
            karma::network::EffectiveClientTransportBackendName(transport_config);
        spdlog::error("ClientConnection: failed to create client transport backend={}",
                      configured_backend);
        closeTransport();
        return false;
    }

    KARMA_TRACE("net.client",
                "ClientConnection: connecting to {}:{}",
                host_,
                port_);

    const uint32_t timeout_ms = static_cast<uint32_t>(
        karma::config::ReadUInt16Config({"network.ConnectTimeoutMs"}, static_cast<uint16_t>(2000)));
    const karma::network::ClientReconnectPolicy reconnect_policy =
        karma::network::ReadClientReconnectPolicyFromConfig();
    karma::network::ClientTransportConnectOptions connect_options{
        .host = host_,
        .port = port_,
        .timeout_ms = timeout_ms};
    karma::network::ApplyReconnectPolicyToConnectOptions(reconnect_policy, &connect_options);

    if (!transport_->connect(connect_options)) {
        spdlog::error("ClientConnection: connection timed out to {}:{}", host_, port_);
        closeTransport();
        return false;
    }

    connected_ = true;
    assigned_client_id_ = 0;
    init_received_ = false;
    join_bootstrap_complete_logged_ = false;
    init_world_name_.clear();
    init_server_name_.clear();
    pending_world_package_ = {};
    active_world_transfer_ = {};
    KARMA_TRACE("net.client",
                "ClientConnection: connected to {}:{}",
                host_,
                port_);

    if (!sendJoinRequest()) {
        spdlog::error("ClientConnection: failed to send join request");
        shutdown();
        return false;
    }

    return true;
}

void ClientConnection::poll() {
    if (!connected_ || !transport_) {
        return;
    }

    std::vector<karma::network::ClientTransportEvent> transport_events{};
    transport_->poll(karma::network::ClientTransportPollOptions{}, &transport_events);
    auto request_disconnect = [this]() {
        if (transport_) {
            transport_->disconnect(0);
        }
    };
    for (const auto& transport_event : transport_events) {
        switch (transport_event.type) {
            case karma::network::ClientTransportEventType::Received: {
                if (!transport_event.payload.empty()) {
                    const auto message =
                        bz3::net::DecodeServerMessage(transport_event.payload.data(),
                                                      transport_event.payload.size());
                    if (message.has_value()) {
                        switch (message->type) {
                            case bz3::net::ServerMessageType::JoinResponse:
                                if (message->join_accepted) {
                                    KARMA_TRACE("net.client",
                                                "ClientConnection: join accepted by {}:{}",
                                                host_,
                                                port_);
                                } else {
                                    const std::string reason = message->reason.empty()
                                        ? std::string("Join rejected by server.")
                                        : message->reason;
                                    spdlog::error("ClientConnection: join rejected: {}", reason);
                                    KARMA_TRACE("net.client",
                                                "ClientConnection: join rejected by {}:{} reason='{}'",
                                                host_,
                                                port_,
                                                reason);
                                    should_exit_ = true;
                                    request_disconnect();
                                }
                                break;
                            case bz3::net::ServerMessageType::Init: {
                                assigned_client_id_ = message->client_id;
                                init_received_ = true;
                                init_world_name_ = message->world_name;
                                init_server_name_ = message->server_name;
                                KARMA_TRACE("net.client",
                                            "ClientConnection: init world='{}' id='{}' rev='{}' hash='{}' content_hash='{}' manifest_hash='{}' manifest_files={} manifest_entries={} server='{}' client_id={} protocol={}",
                                            message->world_name,
                                            message->world_id.empty() ? "-" : message->world_id,
                                            message->world_revision.empty() ? "-" : message->world_revision,
                                            message->world_hash.empty() ? "-" : message->world_hash,
                                            message->world_content_hash.empty() ? "-" : message->world_content_hash,
                                            message->world_manifest_hash.empty() ? "-" : message->world_manifest_hash,
                                            message->world_manifest_file_count,
                                            message->world_manifest.size(),
                                            message->server_name,
                                            message->client_id,
                                            message->protocol_version);
                                if (message->protocol_version != bz3::net::kProtocolVersion) {
                                    KARMA_TRACE("net.client",
                                                "ClientConnection: protocol mismatch server={} client={}",
                                                message->protocol_version,
                                                bz3::net::kProtocolVersion);
                                }
                                const bool expects_inline_payload = !message->world_data.empty();
                                const bool cached_package_available =
                                    HasCachedWorldPackageForServer(host_,
                                                                   port_,
                                                                   message->world_id,
                                                                   message->world_revision,
                                                                   message->world_content_hash,
                                                                   message->world_hash);
                                const bool expect_chunk_transfer = !expects_inline_payload &&
                                                                   message->world_size > 0 &&
                                                                   !cached_package_available;
                                if (expect_chunk_transfer) {
                                    pending_world_package_.active = true;
                                    pending_world_package_.world_name = message->world_name;
                                    pending_world_package_.world_id = message->world_id;
                                    pending_world_package_.world_revision = message->world_revision;
                                    pending_world_package_.world_hash = message->world_hash;
                                    pending_world_package_.world_content_hash = message->world_content_hash;
                                    pending_world_package_.world_manifest_hash = message->world_manifest_hash;
                                    pending_world_package_.world_manifest_file_count =
                                        message->world_manifest_file_count;
                                    pending_world_package_.world_size = message->world_size;
                                    pending_world_package_.world_manifest = message->world_manifest;
                                    active_world_transfer_ = {};
                                    KARMA_TRACE("net.client",
                                                "ClientConnection: init awaiting chunk transfer world='{}' id='{}' rev='{}' bytes={} hash='{}' content_hash='{}' manifest_entries={}",
                                                message->world_name,
                                                message->world_id.empty() ? "-" : message->world_id,
                                                message->world_revision.empty() ? "-" : message->world_revision,
                                                message->world_size,
                                                message->world_hash.empty() ? "-" : message->world_hash,
                                                message->world_content_hash.empty() ? "-" : message->world_content_hash,
                                                message->world_manifest.size());
                                    break;
                                }

                                if (!ApplyWorldPackageForServer(host_,
                                                                port_,
                                                                message->world_name,
                                                                message->world_id,
                                                                message->world_revision,
                                                                message->world_hash,
                                                                message->world_content_hash,
                                                                message->world_manifest_hash,
                                                                message->world_manifest_file_count,
                                                                message->world_size,
                                                                message->world_manifest,
                                                                message->world_data)) {
                                    spdlog::error("ClientConnection: failed to apply world package for '{}'",
                                                  message->world_name);
                                    should_exit_ = true;
                                    request_disconnect();
                                }
                                break;
                            }
                            case bz3::net::ServerMessageType::WorldTransferBegin: {
                                if (!pending_world_package_.active) {
                                    spdlog::error("ClientConnection: unexpected world transfer begin transfer_id='{}' (no pending world init)",
                                                  message->transfer_id);
                                    should_exit_ = true;
                                    request_disconnect();
                                    break;
                                }
                                if (message->transfer_id.empty()) {
                                    spdlog::error("ClientConnection: world transfer begin missing transfer_id");
                                    should_exit_ = true;
                                    request_disconnect();
                                    break;
                                }
                                if (message->transfer_world_id != pending_world_package_.world_id ||
                                    message->transfer_world_revision != pending_world_package_.world_revision) {
                                    spdlog::error("ClientConnection: world transfer begin identity mismatch transfer_id='{}' begin_id='{}' begin_rev='{}' init_id='{}' init_rev='{}'",
                                                  message->transfer_id,
                                                  message->transfer_world_id,
                                                  message->transfer_world_revision,
                                                  pending_world_package_.world_id,
                                                  pending_world_package_.world_revision);
                                    should_exit_ = true;
                                    request_disconnect();
                                    break;
                                }
                                if (message->transfer_is_delta &&
                                    (message->transfer_delta_base_world_id.empty() ||
                                     message->transfer_delta_base_world_revision.empty())) {
                                    spdlog::error("ClientConnection: world delta transfer begin missing base identity transfer_id='{}' base_id='{}' base_rev='{}'",
                                                  message->transfer_id,
                                                  message->transfer_delta_base_world_id,
                                                  message->transfer_delta_base_world_revision);
                                    should_exit_ = true;
                                    request_disconnect();
                                    break;
                                }
                                if (message->transfer_total_bytes > 0 && message->transfer_chunk_size == 0) {
                                    spdlog::error("ClientConnection: world transfer begin invalid chunk size transfer_id='{}' total_bytes={} chunk_size={}",
                                                  message->transfer_id,
                                                  message->transfer_total_bytes,
                                                  message->transfer_chunk_size);
                                    should_exit_ = true;
                                    request_disconnect();
                                    break;
                                }

                                bool resumed_transfer = false;
                                uint32_t resumed_chunks = 0;
                                size_t resumed_bytes = 0;
                                if (active_world_transfer_.active) {
                                    const bool transfer_compatible =
                                        active_world_transfer_.total_bytes_expected ==
                                            message->transfer_total_bytes &&
                                        active_world_transfer_.chunk_size ==
                                            message->transfer_chunk_size &&
                                        active_world_transfer_.is_delta ==
                                            message->transfer_is_delta &&
                                        active_world_transfer_.delta_base_world_id ==
                                            message->transfer_delta_base_world_id &&
                                        active_world_transfer_.delta_base_world_revision ==
                                            message->transfer_delta_base_world_revision &&
                                        active_world_transfer_.delta_base_world_hash ==
                                            message->transfer_delta_base_world_hash &&
                                        active_world_transfer_.delta_base_world_content_hash ==
                                            message->transfer_delta_base_world_content_hash &&
                                        active_world_transfer_.payload.size() <=
                                            message->transfer_total_bytes;
                                    if (!transfer_compatible) {
                                        KARMA_TRACE("net.client",
                                                    "ClientConnection: world transfer restart reset transfer_id='{}' previous_transfer_id='{}' previous_chunks={} previous_bytes={} total_bytes={} chunk_size={}",
                                                    message->transfer_id,
                                                    active_world_transfer_.transfer_id,
                                                    active_world_transfer_.next_chunk_index,
                                                    active_world_transfer_.payload.size(),
                                                    message->transfer_total_bytes,
                                                    message->transfer_chunk_size);
                                        active_world_transfer_ = {};
                                    } else {
                                        resumed_transfer = active_world_transfer_.next_chunk_index > 0;
                                        resumed_chunks = active_world_transfer_.next_chunk_index;
                                        resumed_bytes = active_world_transfer_.payload.size();
                                    }
                                }

                                if (!active_world_transfer_.active) {
                                    active_world_transfer_.active = true;
                                    active_world_transfer_.is_delta = message->transfer_is_delta;
                                    active_world_transfer_.delta_base_world_id =
                                        message->transfer_delta_base_world_id;
                                    active_world_transfer_.delta_base_world_revision =
                                        message->transfer_delta_base_world_revision;
                                    active_world_transfer_.delta_base_world_hash =
                                        message->transfer_delta_base_world_hash;
                                    active_world_transfer_.delta_base_world_content_hash =
                                        message->transfer_delta_base_world_content_hash;
                                    active_world_transfer_.total_bytes_expected =
                                        message->transfer_total_bytes;
                                    active_world_transfer_.chunk_size = message->transfer_chunk_size;
                                    active_world_transfer_.next_chunk_index = 0;
                                    active_world_transfer_.payload.clear();
                                    if (message->transfer_total_bytes > 0) {
                                        active_world_transfer_.payload.reserve(
                                            static_cast<size_t>(message->transfer_total_bytes));
                                    }
                                }
                                active_world_transfer_.transfer_id = message->transfer_id;
                                KARMA_TRACE("net.client",
                                            "ClientConnection: world transfer begin transfer_id='{}' mode={} world='{}' id='{}' rev='{}' total_bytes={} chunk_size={} base_id='{}' base_rev='{}' resume={} resumed_chunks={} resumed_bytes={}",
                                            active_world_transfer_.transfer_id,
                                            active_world_transfer_.is_delta ? "delta" : "full",
                                            pending_world_package_.world_name,
                                            pending_world_package_.world_id,
                                            pending_world_package_.world_revision,
                                            active_world_transfer_.total_bytes_expected,
                                            active_world_transfer_.chunk_size,
                                            active_world_transfer_.delta_base_world_id.empty()
                                                ? "-"
                                                : active_world_transfer_.delta_base_world_id,
                                            active_world_transfer_.delta_base_world_revision.empty()
                                                ? "-"
                                                : active_world_transfer_.delta_base_world_revision,
                                            resumed_transfer ? 1 : 0,
                                            resumed_chunks,
                                            resumed_bytes);
                                break;
                            }
                            case bz3::net::ServerMessageType::WorldTransferChunk: {
                                if (!active_world_transfer_.active) {
                                    spdlog::error("ClientConnection: unexpected world transfer chunk transfer_id='{}' (no active transfer)",
                                                  message->transfer_id);
                                    should_exit_ = true;
                                    request_disconnect();
                                    break;
                                }
                                if (message->transfer_id != active_world_transfer_.transfer_id) {
                                    spdlog::error("ClientConnection: world transfer chunk transfer_id mismatch expected='{}' got='{}'",
                                                  active_world_transfer_.transfer_id,
                                                  message->transfer_id);
                                    should_exit_ = true;
                                    request_disconnect();
                                    break;
                                }
                                if (!IsChunkInTransferBounds(active_world_transfer_.total_bytes_expected,
                                                             active_world_transfer_.chunk_size,
                                                             message->transfer_chunk_index,
                                                             message->transfer_chunk_data.size())) {
                                    spdlog::error("ClientConnection: world transfer chunk bounds mismatch transfer_id='{}' chunk_index={} chunk_bytes={} total_bytes={} chunk_size={}",
                                                  active_world_transfer_.transfer_id,
                                                  message->transfer_chunk_index,
                                                  message->transfer_chunk_data.size(),
                                                  active_world_transfer_.total_bytes_expected,
                                                  active_world_transfer_.chunk_size);
                                    should_exit_ = true;
                                    request_disconnect();
                                    break;
                                }
                                const uint64_t chunk_offset_u64 =
                                    static_cast<uint64_t>(message->transfer_chunk_index) *
                                    active_world_transfer_.chunk_size;
                                const size_t chunk_offset = static_cast<size_t>(chunk_offset_u64);
                                if (message->transfer_chunk_index < active_world_transfer_.next_chunk_index) {
                                    if (!ChunkMatchesBufferedPayload(active_world_transfer_.payload,
                                                                     chunk_offset,
                                                                     message->transfer_chunk_data)) {
                                        spdlog::error("ClientConnection: world transfer chunk retry mismatch transfer_id='{}' chunk_index={} buffered_bytes={} chunk_bytes={}",
                                                      active_world_transfer_.transfer_id,
                                                      message->transfer_chunk_index,
                                                      active_world_transfer_.payload.size(),
                                                      message->transfer_chunk_data.size());
                                        should_exit_ = true;
                                        request_disconnect();
                                        break;
                                    }
                                    KARMA_TRACE("net.client",
                                                "ClientConnection: world transfer retry chunk acknowledged transfer_id='{}' chunk_index={} buffered_chunks={} buffered_bytes={}",
                                                active_world_transfer_.transfer_id,
                                                message->transfer_chunk_index,
                                                active_world_transfer_.next_chunk_index,
                                                active_world_transfer_.payload.size());
                                    break;
                                }
                                if (message->transfer_chunk_index > active_world_transfer_.next_chunk_index) {
                                    spdlog::error("ClientConnection: world transfer chunk gap transfer_id='{}' expected={} got={}",
                                                  active_world_transfer_.transfer_id,
                                                  active_world_transfer_.next_chunk_index,
                                                  message->transfer_chunk_index);
                                    should_exit_ = true;
                                    request_disconnect();
                                    break;
                                }
                                if (chunk_offset != active_world_transfer_.payload.size()) {
                                    spdlog::error("ClientConnection: world transfer chunk offset mismatch transfer_id='{}' chunk_index={} offset={} buffered_bytes={}",
                                                  active_world_transfer_.transfer_id,
                                                  message->transfer_chunk_index,
                                                  chunk_offset,
                                                  active_world_transfer_.payload.size());
                                    should_exit_ = true;
                                    request_disconnect();
                                    break;
                                }
                                active_world_transfer_.payload.insert(active_world_transfer_.payload.end(),
                                                                      message->transfer_chunk_data.begin(),
                                                                      message->transfer_chunk_data.end());
                                ++active_world_transfer_.next_chunk_index;
                                break;
                            }
                            case bz3::net::ServerMessageType::WorldTransferEnd:
                                if (!pending_world_package_.active || !active_world_transfer_.active) {
                                    spdlog::error("ClientConnection: unexpected world transfer end transfer_id='{}' (pending_init={} active_transfer={})",
                                                  message->transfer_id,
                                                  pending_world_package_.active ? 1 : 0,
                                                  active_world_transfer_.active ? 1 : 0);
                                    should_exit_ = true;
                                    request_disconnect();
                                    break;
                                }
                                if (message->transfer_id != active_world_transfer_.transfer_id) {
                                    spdlog::error("ClientConnection: world transfer end transfer_id mismatch expected='{}' got='{}'",
                                                  active_world_transfer_.transfer_id,
                                                  message->transfer_id);
                                    should_exit_ = true;
                                    request_disconnect();
                                    break;
                                }
                                if (message->transfer_chunk_count != active_world_transfer_.next_chunk_index) {
                                    spdlog::error("ClientConnection: world transfer end chunk_count mismatch transfer_id='{}' expected={} got={}",
                                                  active_world_transfer_.transfer_id,
                                                  active_world_transfer_.next_chunk_index,
                                                  message->transfer_chunk_count);
                                    should_exit_ = true;
                                    request_disconnect();
                                    break;
                                }
                                if (message->transfer_total_bytes != active_world_transfer_.payload.size()) {
                                    spdlog::error("ClientConnection: world transfer end size mismatch transfer_id='{}' expected={} got={}",
                                                  active_world_transfer_.transfer_id,
                                                  message->transfer_total_bytes,
                                                  active_world_transfer_.payload.size());
                                    should_exit_ = true;
                                    request_disconnect();
                                    break;
                                }
                                KARMA_TRACE("net.client",
                                            "ClientConnection: world transfer end transfer_id='{}' mode={} chunks={} bytes={}",
                                            active_world_transfer_.transfer_id,
                                            active_world_transfer_.is_delta ? "delta" : "full",
                                            active_world_transfer_.next_chunk_index,
                                            active_world_transfer_.payload.size());
                                if (!ApplyWorldPackageForServer(host_,
                                                                port_,
                                                                pending_world_package_.world_name,
                                                                pending_world_package_.world_id,
                                                                pending_world_package_.world_revision,
                                                                pending_world_package_.world_hash,
                                                                pending_world_package_.world_content_hash,
                                                                pending_world_package_.world_manifest_hash,
                                                                pending_world_package_.world_manifest_file_count,
                                                                pending_world_package_.world_size,
                                                                pending_world_package_.world_manifest,
                                                                active_world_transfer_.payload,
                                                                active_world_transfer_.is_delta,
                                                                active_world_transfer_.delta_base_world_id,
                                                                active_world_transfer_.delta_base_world_revision,
                                                                active_world_transfer_.delta_base_world_hash,
                                                                active_world_transfer_.delta_base_world_content_hash)) {
                                    spdlog::error("ClientConnection: failed to apply chunked world package for '{}'",
                                                  pending_world_package_.world_name);
                                    should_exit_ = true;
                                    request_disconnect();
                                    break;
                                }
                                pending_world_package_ = {};
                                active_world_transfer_ = {};
                                break;
                            case bz3::net::ServerMessageType::SessionSnapshot:
                                KARMA_TRACE("net.client",
                                            "ClientConnection: snapshot sessions={}",
                                            message->sessions.size());
                                for (const auto& session : message->sessions) {
                                    KARMA_TRACE("net.client",
                                                "ClientConnection: snapshot session id={} name='{}'",
                                                session.session_id,
                                                session.session_name);
                                }
                                if (init_received_ &&
                                    !join_bootstrap_complete_logged_ &&
                                    !pending_world_package_.active &&
                                    !active_world_transfer_.active) {
                                    join_bootstrap_complete_logged_ = true;
                                    KARMA_TRACE("net.client",
                                                "ClientConnection: join bootstrap complete client_id={} world='{}' server='{}' sessions={}",
                                                assigned_client_id_,
                                                init_world_name_,
                                                init_server_name_,
                                                message->sessions.size());
                                    if (audio_event_callback_) {
                                        audio_event_callback_(AudioEvent::PlayerSpawn);
                                    }
                                }
                                break;
                            case bz3::net::ServerMessageType::PlayerSpawn:
                                KARMA_TRACE("net.client",
                                            "ClientConnection: player spawn client_id={}",
                                            message->event_client_id);
                                if (audio_event_callback_) {
                                    audio_event_callback_(AudioEvent::PlayerSpawn);
                                }
                                break;
                            case bz3::net::ServerMessageType::PlayerDeath:
                                KARMA_TRACE("net.client",
                                            "ClientConnection: player death client_id={}",
                                            message->event_client_id);
                                if (audio_event_callback_) {
                                    audio_event_callback_(AudioEvent::PlayerDeath);
                                }
                                break;
                            case bz3::net::ServerMessageType::CreateShot: {
                                KARMA_TRACE("net.client",
                                            "ClientConnection: create shot id={} source_client_id={}",
                                            message->event_shot_id,
                                            message->event_client_id);
                                const bool is_local_echo =
                                    (assigned_client_id_ != 0 && message->event_client_id == assigned_client_id_);
                                if (audio_event_callback_ && !is_local_echo) {
                                    audio_event_callback_(AudioEvent::ShotFire);
                                }
                                break;
                            }
                            case bz3::net::ServerMessageType::RemoveShot:
                                KARMA_TRACE("net.client",
                                            "ClientConnection: remove shot id={} global={}",
                                            message->event_shot_id,
                                            message->event_shot_is_global ? 1 : 0);
                                break;
                            default:
                                KARMA_TRACE("net.client",
                                            "ClientConnection: server message payload={} bytes={}",
                                            message->other_payload,
                                            transport_event.payload.size());
                                break;
                        }
                    } else {
                        KARMA_TRACE("net.client",
                                    "ClientConnection: invalid server payload bytes={}",
                                    transport_event.payload.size());
                    }
                } else {
                    KARMA_TRACE("net.client",
                                "ClientConnection: invalid server payload bytes={}",
                                transport_event.payload.size());
                }
                break;
            }
            case karma::network::ClientTransportEventType::Disconnected:
                KARMA_TRACE("net.client",
                            "ClientConnection: disconnected from {}:{}",
                            host_,
                            port_);
                connected_ = false;
                pending_world_package_ = {};
                active_world_transfer_ = {};
                break;
            case karma::network::ClientTransportEventType::Connected:
                KARMA_TRACE("net.client",
                            "ClientConnection: transport reconnected to {}:{}; replaying join bootstrap",
                            host_,
                            port_);
                connected_ = true;
                join_sent_ = false;
                leave_sent_ = false;
                assigned_client_id_ = 0;
                init_received_ = false;
                join_bootstrap_complete_logged_ = false;
                init_world_name_.clear();
                init_server_name_.clear();
                pending_world_package_ = {};
                active_world_transfer_ = {};
                if (!sendJoinRequest()) {
                    spdlog::error("ClientConnection: failed to resend join request after reconnect");
                    should_exit_ = true;
                    request_disconnect();
                }
                break;
            default:
                break;
        }
    }
}

void ClientConnection::shutdown() {
    if (!started_) {
        return;
    }
    started_ = false;

    if (connected_) {
        static_cast<void>(sendLeave());
    }

    if (transport_ && transport_->isConnected()) {
        transport_->disconnect(0);
        const bool disconnected = transport_->waitForDisconnect(50);
        if (!disconnected) {
            transport_->resetConnection();
        }
    }

    connected_ = false;
    pending_world_package_ = {};
    active_world_transfer_ = {};
    closeTransport();
}

bool ClientConnection::isConnected() const {
    return connected_;
}

bool ClientConnection::shouldExit() const {
    return should_exit_;
}

bool ClientConnection::sendRequestPlayerSpawn() {
    if (!connected_ || !transport_ || !transport_->isConnected() || assigned_client_id_ == 0) {
        return false;
    }

    const auto payload = bz3::net::EncodeClientRequestPlayerSpawn(assigned_client_id_);
    if (payload.empty()) {
        return false;
    }

    if (!sendPayloadReliable(payload)) {
        return false;
    }
    KARMA_TRACE("net.client",
                "ClientConnection: sent request_player_spawn client_id={}",
                assigned_client_id_);
    return true;
}

bool ClientConnection::sendCreateShot() {
    if (!connected_ || !transport_ || !transport_->isConnected() || assigned_client_id_ == 0) {
        return false;
    }

    const uint32_t local_shot_id = next_local_shot_id_++;
    const auto payload = bz3::net::EncodeClientCreateShot(
        assigned_client_id_,
        local_shot_id,
        bz3::net::Vec3{0.0f, 0.0f, 0.0f},
        bz3::net::Vec3{0.0f, 0.0f, 0.0f});
    if (payload.empty()) {
        return false;
    }

    if (!sendPayloadReliable(payload)) {
        return false;
    }
    KARMA_TRACE("net.client",
                "ClientConnection: sent create_shot client_id={} local_shot_id={}",
                assigned_client_id_,
                local_shot_id);
    return true;
}

bool ClientConnection::sendJoinRequest() {
    if (!connected_ || !transport_ || !transport_->isConnected() || join_sent_) {
        return connected_ && join_sent_;
    }

    const auto cached_world_identity = ReadCachedWorldIdentityForServer(host_, port_);
    const std::string_view cached_world_hash =
        cached_world_identity.has_value() ? std::string_view(cached_world_identity->world_hash) : std::string_view{};
    const std::string_view cached_world_id =
        cached_world_identity.has_value() ? std::string_view(cached_world_identity->world_id) : std::string_view{};
    const std::string_view cached_world_revision =
        cached_world_identity.has_value() ? std::string_view(cached_world_identity->world_revision) : std::string_view{};
    const std::string_view cached_world_content_hash =
        cached_world_identity.has_value() ? std::string_view(cached_world_identity->world_content_hash) : std::string_view{};
    std::string cached_world_manifest_hash_storage{};
    uint32_t cached_world_manifest_file_count = 0;
    std::vector<bz3::net::WorldManifestEntry> cached_world_manifest{};
    if (cached_world_identity.has_value()) {
        const std::filesystem::path server_cache_dir =
            karma::data::EnsureUserWorldDirectoryForServer(host_, port_);
        cached_world_manifest = ReadCachedWorldManifest(server_cache_dir);
        cached_world_manifest_hash_storage = ComputeManifestHash(cached_world_manifest);
        cached_world_manifest_file_count = static_cast<uint32_t>(cached_world_manifest.size());
    }
    const std::string_view cached_world_manifest_hash = cached_world_manifest_hash_storage;
    const auto payload = bz3::net::EncodeClientJoinRequest(player_name_,
                                                           bz3::net::kProtocolVersion,
                                                           cached_world_hash,
                                                           cached_world_id,
                                                           cached_world_revision,
                                                           cached_world_content_hash,
                                                           cached_world_manifest_hash,
                                                           cached_world_manifest_file_count,
                                                           cached_world_manifest);
    if (payload.empty()) {
        return false;
    }

    if (!sendPayloadReliable(payload)) {
        return false;
    }
    join_sent_ = true;

    KARMA_TRACE("net.client",
                "ClientConnection: sent join request name='{}' protocol={} cached_world_hash='{}' cached_world_id='{}' cached_world_revision='{}' cached_world_content_hash='{}' cached_world_manifest_hash='{}' cached_world_manifest_files={} cached_world_manifest_entries={} to {}:{}",
                player_name_,
                bz3::net::kProtocolVersion,
                cached_world_hash.empty() ? "-" : cached_world_hash,
                cached_world_id.empty() ? "-" : cached_world_id,
                cached_world_revision.empty() ? "-" : cached_world_revision,
                cached_world_content_hash.empty() ? "-" : cached_world_content_hash,
                cached_world_manifest_hash.empty() ? "-" : cached_world_manifest_hash,
                cached_world_manifest_file_count,
                cached_world_manifest.size(),
                host_,
                port_);
    return true;
}

bool ClientConnection::sendLeave() {
    if (!connected_ || !transport_ || !transport_->isConnected() || leave_sent_) {
        return connected_ && leave_sent_;
    }

    const auto payload = bz3::net::EncodeClientLeave(assigned_client_id_);
    if (payload.empty()) {
        return false;
    }

    if (!sendPayloadReliable(payload)) {
        return false;
    }
    leave_sent_ = true;
    KARMA_TRACE("net.client",
                "ClientConnection: sent leave");
    return true;
}

bool ClientConnection::sendPayloadReliable(const std::vector<std::byte>& payload) {
    if (payload.empty() || !transport_ || !transport_->isConnected()) {
        return false;
    }
    return transport_->sendReliable(payload);
}

void ClientConnection::closeTransport() {
    if (transport_) {
        transport_->close();
        transport_.reset();
    }
}

} // namespace bz3::client::net

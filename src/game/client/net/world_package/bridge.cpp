#include "client/net/world_package/internal.hpp"

#include "karma/common/content/cache_store.hpp"
#include "karma/common/content/manifest.hpp"
#include "karma/common/content/package_apply.hpp"
#include "karma/common/content/primitives.hpp"
#include "karma/common/data/path_resolver.hpp"
#include "karma/common/logging/logging.hpp"

#include <spdlog/spdlog.h>

#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bz3::client::net {

namespace {

std::vector<karma::common::content::ManifestEntry> ToContentManifest(
    const std::vector<bz3::net::WorldManifestEntry>& manifest) {
    std::vector<karma::common::content::ManifestEntry> converted{};
    converted.reserve(manifest.size());
    for (const auto& entry : manifest) {
        converted.push_back(karma::common::content::ManifestEntry{
            .path = entry.path,
            .size = entry.size,
            .hash = entry.hash});
    }
    return converted;
}

std::vector<bz3::net::WorldManifestEntry> FromContentManifest(
    const std::vector<karma::common::content::ManifestEntry>& manifest) {
    std::vector<bz3::net::WorldManifestEntry> converted{};
    converted.reserve(manifest.size());
    for (const auto& entry : manifest) {
        converted.push_back(bz3::net::WorldManifestEntry{
            .path = entry.path,
            .size = entry.size,
            .hash = entry.hash});
    }
    return converted;
}

karma::common::content::CachedContentIdentity ToContentIdentity(const CachedWorldIdentity& identity) {
    return karma::common::content::CachedContentIdentity{
        .world_hash = identity.world_hash,
        .world_content_hash = identity.world_content_hash,
        .world_id = identity.world_id,
        .world_revision = identity.world_revision};
}

CachedWorldIdentity FromContentIdentity(const karma::common::content::CachedContentIdentity& identity) {
    return CachedWorldIdentity{
        .world_hash = identity.world_hash,
        .world_content_hash = identity.world_content_hash,
        .world_id = identity.world_id,
        .world_revision = identity.world_revision};
}

} // namespace

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
    return karma::common::content::HashStringFNV1a(value);
}

void HashStringFNV1a(uint64_t& hash, std::string_view value) {
    karma::common::content::HashStringFNV1a(hash, value);
}

void HashBytesFNV1a(uint64_t& hash, const std::byte* bytes, size_t count) {
    karma::common::content::HashBytesFNV1a(hash, bytes, count);
}

void HashBytesFNV1a(uint64_t& hash, std::string_view value) {
    karma::common::content::HashBytesFNV1a(hash, value);
}

void HashSeparatorFNV1a(uint64_t& hash) {
    karma::common::content::HashSeparatorFNV1a(hash);
}

std::string Hash64Hex(uint64_t hash) {
    return karma::common::content::Hash64Hex(hash);
}

void HashChunkChainFNV1a(uint64_t& hash, uint32_t chunk_index, const std::vector<std::byte>& chunk_data) {
    karma::common::content::HashChunkChainFNV1a(hash, chunk_index, chunk_data);
}

bool InitIncludesWorldMetadata(const bz3::net::ServerMessage& message) {
    return !message.world_data.empty() ||
           message.world_size > 0 ||
           !message.world_hash.empty() ||
           !message.world_content_hash.empty() ||
           !message.world_manifest_hash.empty() ||
           message.world_manifest_file_count > 0 ||
           !message.world_manifest.empty();
}

bool IsChunkInTransferBounds(uint64_t total_bytes,
                             uint32_t chunk_size,
                             uint32_t chunk_index,
                             size_t chunk_bytes) {
    return karma::common::content::IsChunkInTransferBounds(total_bytes, chunk_size, chunk_index, chunk_bytes);
}

bool ChunkMatchesBufferedPayload(const std::vector<std::byte>& payload,
                                 size_t chunk_offset,
                                 const std::vector<std::byte>& chunk_data) {
    return karma::common::content::ChunkMatchesBufferedPayload(payload, chunk_offset, chunk_data);
}

std::string SanitizeCachePathComponent(std::string_view input, std::string_view fallback_prefix) {
    return karma::common::content::SanitizeCachePathComponent(input,
                                                      fallback_prefix,
                                                      kMaxCachePathComponentLen);
}

std::filesystem::path PackageRootForIdentity(const std::filesystem::path& server_cache_dir,
                                             std::string_view world_id,
                                             std::string_view world_revision,
                                             std::string_view world_package_cache_key) {
    return karma::common::content::PackageRootForIdentity(WorldPackagesByWorldRoot(server_cache_dir),
                                                  world_id,
                                                  world_revision,
                                                  world_package_cache_key,
                                                  kMaxCachePathComponentLen);
}

std::string ResolveWorldPackageCacheKey(std::string_view world_content_hash, std::string_view world_hash) {
    return karma::common::content::ResolveWorldPackageCacheKey(world_content_hash, world_hash);
}

std::string ComputeManifestHash(const std::vector<bz3::net::WorldManifestEntry>& manifest) {
    return karma::common::content::ComputeManifestHash(ToContentManifest(manifest));
}

bool VerifyExtractedWorldPackage(const std::filesystem::path& package_root,
                                 std::string_view world_name,
                                 std::string_view expected_world_content_hash,
                                 std::string_view expected_world_manifest_hash,
                                 uint32_t expected_world_manifest_file_count,
                                 const std::vector<bz3::net::WorldManifestEntry>& expected_world_manifest,
                                 std::string_view stage_name) {
    const auto summary = karma::common::content::ComputeDirectoryManifestSummary(package_root);
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
        const auto expected = karma::common::content::SortManifestEntries(ToContentManifest(expected_world_manifest));
        const auto actual = karma::common::content::SortManifestEntries(summary->entries);
        if (!karma::common::content::ManifestEntriesEqual(expected, actual)) {
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

std::vector<bz3::net::WorldManifestEntry> ReadCachedWorldManifest(
    const std::filesystem::path& server_cache_dir) {
    bool malformed = false;
    const auto manifest = karma::common::content::ReadCachedManifestFile(ActiveWorldManifestPath(server_cache_dir),
                                                                 &malformed);
    if (malformed) {
        spdlog::warn("ClientConnection: cached world manifest '{}' is malformed; ignoring",
                     ActiveWorldManifestPath(server_cache_dir).string());
    }
    return FromContentManifest(manifest);
}

bool PersistCachedWorldManifest(const std::filesystem::path& server_cache_dir,
                                const std::vector<bz3::net::WorldManifestEntry>& manifest) {
    return karma::common::content::PersistCachedManifestFile(ActiveWorldManifestPath(server_cache_dir),
                                                     ToContentManifest(manifest));
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

    const auto plan = karma::common::content::BuildManifestDiffPlan(ToContentManifest(cached_manifest),
                                                             ToContentManifest(incoming_manifest));
    if (cached_manifest.empty()) {
        KARMA_TRACE("net.client",
                    "ClientConnection: manifest diff plan world='{}' cached_entries=0 incoming_entries={} unchanged=0 added={} modified=0 removed=0 potential_transfer_bytes={} reused_bytes=0",
                    world_name,
                    incoming_manifest.size(),
                    incoming_manifest.size(),
                    plan.potential_transfer_bytes);
        return;
    }

    KARMA_TRACE("net.client",
                "ClientConnection: manifest diff plan world='{}' cached_entries={} incoming_entries={} unchanged={} added={} modified={} removed={} potential_transfer_bytes={} reused_bytes={}",
                world_name,
                cached_manifest.size(),
                incoming_manifest.size(),
                plan.unchanged_entries,
                plan.added_entries,
                plan.modified_entries,
                plan.removed_entries,
                plan.potential_transfer_bytes,
                plan.reused_bytes);
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
            karma::common::data::EnsureUserWorldDirectoryForServer(host, port);
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

std::string WorldCacheDirName(std::string_view world_id) {
    return karma::common::content::WorldCacheDirName(world_id, kMaxCachePathComponentLen);
}

std::string RevisionCacheDirName(std::string_view world_revision) {
    return karma::common::content::RevisionCacheDirName(world_revision, kMaxCachePathComponentLen);
}

std::filesystem::path BuildPackageStagingRoot(const std::filesystem::path& package_root) {
    return karma::common::content::BuildPackageStagingRoot(package_root);
}

std::filesystem::path BuildPackageBackupRoot(const std::filesystem::path& package_root) {
    return karma::common::content::BuildPackageBackupRoot(package_root);
}

void CleanupStaleTemporaryDirectories(const std::filesystem::path& package_root) {
    karma::common::content::CleanupStaleTemporaryDirectories(package_root, "ClientConnection");
}

bool ActivateStagedPackageRootAtomically(const std::filesystem::path& package_root,
                                         const std::filesystem::path& staging_root) {
    return karma::common::content::ActivateStagedPackageRootAtomically(package_root,
                                                               staging_root,
                                                               "ClientConnection");
}

void TouchPathIfPresent(const std::filesystem::path& path) {
    karma::common::content::TouchPathIfPresent(path);
}

std::filesystem::file_time_type LastWriteTimeOrMin(const std::filesystem::path& path) {
    return karma::common::content::LastWriteTimeOrMin(path);
}

void PruneWorldPackageCache(const std::filesystem::path& server_cache_dir,
                            std::string_view active_world_id,
                            std::string_view active_world_revision,
                            std::string_view active_world_package_key) {
    // Keep retention policy engine-owned here so server-delivered world config cannot influence
    // cache pruning behavior via runtime layers.
    const auto result = karma::common::content::PruneWorldPackageCache(WorldPackagesByWorldRoot(server_cache_dir),
                                                               active_world_id,
                                                               active_world_revision,
                                                               active_world_package_key,
                                                               kDefaultMaxRevisionsPerWorld,
                                                               kDefaultMaxPackagesPerRevision,
                                                               kMaxCachePathComponentLen);

    for (const auto& warning : result.warnings) {
        switch (warning.kind) {
        case karma::common::content::CachePruneWarningKind::PrunePackage:
            spdlog::warn("ClientConnection: failed to prune cached world package '{}': {}",
                         warning.path.string(),
                         warning.message);
            break;
        case karma::common::content::CachePruneWarningKind::RemoveEmptyRevision:
            spdlog::warn("ClientConnection: failed to remove empty cached revision '{}': {}",
                         warning.path.string(),
                         warning.message);
            break;
        case karma::common::content::CachePruneWarningKind::PruneRevision:
            spdlog::warn("ClientConnection: failed to prune cached world revision '{}': {}",
                         warning.path.string(),
                         warning.message);
            break;
        case karma::common::content::CachePruneWarningKind::RemoveEmptyWorldDir:
            spdlog::warn("ClientConnection: failed to remove empty cached world dir '{}': {}",
                         warning.path.string(),
                         warning.message);
            break;
        }
    }

    for (const auto& path : result.pruned_package_paths) {
        KARMA_TRACE("net.client",
                    "ClientConnection: pruned cached world package '{}'",
                    path.string());
    }
    for (const auto& path : result.pruned_revision_paths) {
        KARMA_TRACE("net.client",
                    "ClientConnection: pruned cached world revision '{}'",
                    path.string());
    }

    KARMA_TRACE("net.client",
                "ClientConnection: cache prune summary worlds={} revisions={} packages={} pruned_revisions={} pruned_packages={}",
                result.scanned_world_dirs,
                result.scanned_revision_dirs,
                result.scanned_package_dirs,
                result.pruned_revision_dirs,
                result.pruned_package_dirs);
}

std::string ComputeWorldPackageHash(const std::vector<std::byte>& bytes) {
    return karma::common::content::ComputeWorldPackageHash(bytes);
}

bool HasPackageIdentity(const CachedWorldIdentity& identity) {
    return karma::common::content::HasPackageIdentity(ToContentIdentity(identity));
}

bool HasRequiredIdentityFields(const CachedWorldIdentity& identity) {
    return karma::common::content::HasRequiredIdentityFields(ToContentIdentity(identity));
}

std::optional<CachedWorldIdentity> ReadCachedWorldIdentityFile(const std::filesystem::path& identity_file) {
    const auto identity = karma::common::content::ReadCachedIdentityFile(identity_file);
    if (!identity.has_value()) {
        return std::nullopt;
    }
    return FromContentIdentity(*identity);
}

std::optional<CachedWorldIdentity> ReadCachedWorldIdentityForServer(const std::string& host, uint16_t port) {
    try {
        const auto server_cache_dir = karma::common::data::EnsureUserWorldDirectoryForServer(host, port);
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
    return karma::common::content::PersistCachedIdentityFile(ActiveWorldIdentityPath(server_cache_dir),
                                                     world_hash,
                                                     world_content_hash,
                                                     world_id,
                                                     world_revision);
}

std::optional<CachedWorldIdentity> ReadCachedWorldIdentity(const std::filesystem::path& server_cache_dir) {
    return ReadCachedWorldIdentityFile(ActiveWorldIdentityPath(server_cache_dir));
}

bool NormalizeRelativePath(std::string_view raw_path, std::filesystem::path* out) {
    return karma::common::content::NormalizeRelativePath(raw_path, out);
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

    size_t removed_paths = 0;
    if (!karma::common::content::ApplyDeltaArchiveOverBasePackage(target_root,
                                                          base_root,
                                                          world_name,
                                                          world_content_hash,
                                                          world_manifest_hash,
                                                          world_manifest_file_count,
                                                          ToContentManifest(world_manifest),
                                                          delta_archive,
                                                          "ClientConnection",
                                                          &removed_paths)) {
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

namespace detail {

bool InitIncludesWorldMetadata(const bz3::net::ServerMessage& message) {
    return ::bz3::client::net::InitIncludesWorldMetadata(message);
}

bool IsChunkInTransferBounds(uint64_t total_bytes,
                             uint32_t chunk_size,
                             uint32_t chunk_index,
                             size_t chunk_bytes) {
    return ::bz3::client::net::IsChunkInTransferBounds(total_bytes,
                                                       chunk_size,
                                                       chunk_index,
                                                       chunk_bytes);
}

bool ChunkMatchesBufferedPayload(const std::vector<std::byte>& payload,
                                 size_t chunk_offset,
                                 const std::vector<std::byte>& chunk_data) {
    return ::bz3::client::net::ChunkMatchesBufferedPayload(payload, chunk_offset, chunk_data);
}

void HashBytesFNV1a(uint64_t& hash, const std::byte* bytes, size_t count) {
    ::bz3::client::net::HashBytesFNV1a(hash, bytes, count);
}

void HashChunkChainFNV1a(uint64_t& hash,
                         uint32_t chunk_index,
                         const std::vector<std::byte>& chunk_data) {
    ::bz3::client::net::HashChunkChainFNV1a(hash, chunk_index, chunk_data);
}

std::string Hash64Hex(uint64_t hash) {
    return ::bz3::client::net::Hash64Hex(hash);
}

bool HasCachedWorldPackageForServer(const std::string& host,
                                    uint16_t port,
                                    std::string_view world_id,
                                    std::string_view world_revision,
                                    std::string_view world_content_hash,
                                    std::string_view world_hash) {
    return ::bz3::client::net::HasCachedWorldPackageForServer(host,
                                                              port,
                                                              world_id,
                                                              world_revision,
                                                              world_content_hash,
                                                              world_hash);
}

std::optional<CachedWorldIdentity> ReadCachedWorldIdentityForServer(const std::string& host, uint16_t port) {
    return ::bz3::client::net::ReadCachedWorldIdentityForServer(host, port);
}

std::vector<bz3::net::WorldManifestEntry> ReadCachedWorldManifest(
    const std::filesystem::path& server_cache_dir) {
    return ::bz3::client::net::ReadCachedWorldManifest(server_cache_dir);
}

std::string ComputeManifestHash(const std::vector<bz3::net::WorldManifestEntry>& manifest) {
    return ::bz3::client::net::ComputeManifestHash(manifest);
}

} // namespace detail

} // namespace bz3::client::net

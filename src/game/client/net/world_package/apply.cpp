#include "client/net/world_package/internal.hpp"

#include "karma/common/config_store.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "karma/common/logging.hpp"
#include "karma/common/world_archive.hpp"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace bz3::client::net {

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
                                bool is_delta_transfer,
                                std::string_view delta_base_world_id,
                                std::string_view delta_base_world_revision,
                                std::string_view delta_base_world_hash,
                                std::string_view delta_base_world_content_hash) {
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
                                bool is_delta_transfer,
                                std::string_view delta_base_world_id,
                                std::string_view delta_base_world_revision,
                                std::string_view delta_base_world_hash,
                                std::string_view delta_base_world_content_hash) {
    return ::bz3::client::net::ApplyWorldPackageForServer(host,
                                                          port,
                                                          world_name,
                                                          world_id,
                                                          world_revision,
                                                          world_hash,
                                                          world_content_hash,
                                                          world_manifest_hash,
                                                          world_manifest_file_count,
                                                          world_size,
                                                          world_manifest,
                                                          world_data,
                                                          is_delta_transfer,
                                                          delta_base_world_id,
                                                          delta_base_world_revision,
                                                          delta_base_world_hash,
                                                          delta_base_world_content_hash);
}

} // namespace detail

} // namespace bz3::client::net

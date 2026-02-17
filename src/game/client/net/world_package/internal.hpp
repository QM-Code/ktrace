#pragma once

#include "client/net/connection/internal.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bz3::client::net {

inline constexpr const char* kRuntimeLayerLabel = "world package config";
inline constexpr const char* kPackageMountId = "world.package";
inline constexpr const char* kWorldIdentityFile = "active_world_identity.txt";
inline constexpr const char* kWorldManifestFile = "active_world_manifest.txt";
inline constexpr const char* kWorldPackagesDir = "world-packages";
inline constexpr const char* kWorldPackagesByWorldDir = "by-world";
inline constexpr const char* kDeltaRemovedPathsFile = "__bz3_delta_removed_paths.txt";
inline constexpr const char* kDeltaMetaFile = "__bz3_delta_meta.txt";
inline constexpr size_t kMaxCachePathComponentLen = 96;
inline constexpr uint16_t kDefaultMaxRevisionsPerWorld = 4;
inline constexpr uint16_t kDefaultMaxPackagesPerRevision = 2;
inline constexpr uint64_t kFNV1aOffsetBasis64 = detail::kFNV1aOffsetBasis64;

using CachedWorldIdentity = detail::CachedWorldIdentity;

std::filesystem::path ActiveWorldIdentityPath(const std::filesystem::path& server_cache_dir);
std::filesystem::path ActiveWorldManifestPath(const std::filesystem::path& server_cache_dir);
std::filesystem::path WorldPackagesByWorldRoot(const std::filesystem::path& server_cache_dir);

uint64_t HashStringFNV1a(std::string_view value);
void HashStringFNV1a(uint64_t& hash, std::string_view value);
void HashBytesFNV1a(uint64_t& hash, const std::byte* bytes, size_t count);
void HashBytesFNV1a(uint64_t& hash, std::string_view value);
void HashSeparatorFNV1a(uint64_t& hash);
std::string Hash64Hex(uint64_t hash);
void HashChunkChainFNV1a(uint64_t& hash, uint32_t chunk_index, const std::vector<std::byte>& chunk_data);

bool InitIncludesWorldMetadata(const bz3::net::ServerMessage& message);
bool IsChunkInTransferBounds(uint64_t total_bytes,
                             uint32_t chunk_size,
                             uint32_t chunk_index,
                             size_t chunk_bytes);
bool ChunkMatchesBufferedPayload(const std::vector<std::byte>& payload,
                                 size_t chunk_offset,
                                 const std::vector<std::byte>& chunk_data);

std::string SanitizeCachePathComponent(std::string_view input, std::string_view fallback_prefix);
std::filesystem::path PackageRootForIdentity(const std::filesystem::path& server_cache_dir,
                                             std::string_view world_id,
                                             std::string_view world_revision,
                                             std::string_view world_package_cache_key);
std::string ResolveWorldPackageCacheKey(std::string_view world_content_hash, std::string_view world_hash);

std::string ComputeManifestHash(const std::vector<bz3::net::WorldManifestEntry>& manifest);
bool VerifyExtractedWorldPackage(const std::filesystem::path& package_root,
                                 std::string_view world_name,
                                 std::string_view expected_world_content_hash,
                                 std::string_view expected_world_manifest_hash,
                                 uint32_t expected_world_manifest_file_count,
                                 const std::vector<bz3::net::WorldManifestEntry>& expected_world_manifest,
                                 std::string_view stage_name);
std::vector<bz3::net::WorldManifestEntry> ReadCachedWorldManifest(
    const std::filesystem::path& server_cache_dir);
bool PersistCachedWorldManifest(const std::filesystem::path& server_cache_dir,
                                const std::vector<bz3::net::WorldManifestEntry>& manifest);
void LogManifestDiffPlan(std::string_view world_name,
                         const std::vector<bz3::net::WorldManifestEntry>& cached_manifest,
                         const std::vector<bz3::net::WorldManifestEntry>& incoming_manifest);

bool HasCachedWorldPackageForServer(const std::string& host,
                                    uint16_t port,
                                    std::string_view world_id,
                                    std::string_view world_revision,
                                    std::string_view world_content_hash,
                                    std::string_view world_hash);
std::string WorldCacheDirName(std::string_view world_id);
std::string RevisionCacheDirName(std::string_view world_revision);
std::filesystem::path BuildPackageStagingRoot(const std::filesystem::path& package_root);
std::filesystem::path BuildPackageBackupRoot(const std::filesystem::path& package_root);
void CleanupStaleTemporaryDirectories(const std::filesystem::path& package_root);
bool ActivateStagedPackageRootAtomically(const std::filesystem::path& package_root,
                                         const std::filesystem::path& staging_root);
bool ExtractWorldArchiveAtomically(const std::vector<std::byte>& world_data,
                                   const std::filesystem::path& package_root,
                                   std::string_view world_name,
                                   std::string_view expected_world_content_hash,
                                   std::string_view expected_world_manifest_hash,
                                   uint32_t expected_world_manifest_file_count,
                                   const std::vector<bz3::net::WorldManifestEntry>& expected_world_manifest);
void TouchPathIfPresent(const std::filesystem::path& path);
std::filesystem::file_time_type LastWriteTimeOrMin(const std::filesystem::path& path);
void PruneWorldPackageCache(const std::filesystem::path& server_cache_dir,
                            std::string_view active_world_id,
                            std::string_view active_world_revision,
                            std::string_view active_world_package_key);
std::string ComputeWorldPackageHash(const std::vector<std::byte>& bytes);

bool HasPackageIdentity(const CachedWorldIdentity& identity);
bool HasRequiredIdentityFields(const CachedWorldIdentity& identity);
std::optional<CachedWorldIdentity> ReadCachedWorldIdentityFile(const std::filesystem::path& identity_file);
std::optional<CachedWorldIdentity> ReadCachedWorldIdentityForServer(const std::string& host, uint16_t port);
bool PersistCachedWorldIdentity(const std::filesystem::path& server_cache_dir,
                                std::string_view world_hash,
                                std::string_view world_content_hash,
                                std::string_view world_id,
                                std::string_view world_revision);
std::optional<CachedWorldIdentity> ReadCachedWorldIdentity(const std::filesystem::path& server_cache_dir);
bool NormalizeRelativePath(std::string_view raw_path, std::filesystem::path* out);
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
                                     const std::vector<std::byte>& delta_archive);
bool ValidateCachedWorldIdentity(const std::filesystem::path& server_cache_dir,
                                 std::string_view world_name,
                                 std::string_view expected_world_hash,
                                 std::string_view expected_world_content_hash,
                                 std::string_view expected_world_id,
                                 std::string_view expected_world_revision,
                                 std::string_view expected_world_manifest_hash,
                                 uint32_t expected_world_manifest_file_count,
                                 bool require_exact_revision = true);
void ClearCachedWorldIdentity(const std::filesystem::path& server_cache_dir);

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
                                std::string_view delta_base_world_content_hash = {});

} // namespace bz3::client::net

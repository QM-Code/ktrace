#pragma once

#include "net/protocol_codec.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bz3::client::net::detail {

inline constexpr uint64_t kFNV1aOffsetBasis64 = 14695981039346656037ULL;

struct CachedWorldIdentity {
    std::string world_hash{};
    std::string world_content_hash{};
    std::string world_id{};
    std::string world_revision{};
};

bool InitIncludesWorldMetadata(const bz3::net::ServerMessage& message);
bool IsChunkInTransferBounds(uint64_t total_bytes,
                             uint32_t chunk_size,
                             uint32_t chunk_index,
                             size_t chunk_bytes);
bool ChunkMatchesBufferedPayload(const std::vector<std::byte>& payload,
                                 size_t chunk_offset,
                                 const std::vector<std::byte>& chunk_data);
void HashBytesFNV1a(uint64_t& hash, const std::byte* bytes, size_t count);
void HashChunkChainFNV1a(uint64_t& hash,
                         uint32_t chunk_index,
                         const std::vector<std::byte>& chunk_data);
std::string Hash64Hex(uint64_t hash);

bool HasCachedWorldPackageForServer(const std::string& host,
                                    uint16_t port,
                                    std::string_view world_id,
                                    std::string_view world_revision,
                                    std::string_view world_content_hash,
                                    std::string_view world_hash);
std::optional<CachedWorldIdentity> ReadCachedWorldIdentityForServer(const std::string& host, uint16_t port);
std::vector<bz3::net::WorldManifestEntry> ReadCachedWorldManifest(
    const std::filesystem::path& server_cache_dir);
std::string ComputeManifestHash(const std::vector<bz3::net::WorldManifestEntry>& manifest);

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

} // namespace bz3::client::net::detail

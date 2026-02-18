#pragma once

#include "karma/common/content/types.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace karma::common::content {

uint64_t HashStringFNV1a(std::string_view value);
void HashStringFNV1a(uint64_t& hash, std::string_view value);
void HashBytesFNV1a(uint64_t& hash, const std::byte* bytes, size_t count);
void HashBytesFNV1a(uint64_t& hash, std::string_view value);
void HashSeparatorFNV1a(uint64_t& hash);
std::string Hash64Hex(uint64_t hash);
void HashChunkChainFNV1a(uint64_t& hash, uint32_t chunk_index, const std::vector<std::byte>& chunk_data);

bool IsChunkInTransferBounds(uint64_t total_bytes,
                             uint32_t chunk_size,
                             uint32_t chunk_index,
                             size_t chunk_bytes);
bool ChunkMatchesBufferedPayload(const std::vector<std::byte>& payload,
                                 size_t chunk_offset,
                                 const std::vector<std::byte>& chunk_data);
bool NormalizeRelativePath(std::string_view raw_path, std::filesystem::path* out);

std::string ComputeWorldPackageHash(const std::vector<std::byte>& bytes);

} // namespace karma::common::content

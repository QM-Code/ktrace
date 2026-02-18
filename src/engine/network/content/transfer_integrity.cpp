#include "karma/network/content/transfer_integrity.hpp"

#include "karma/common/content/types.hpp"

#include <algorithm>
#include <string>
#include <string_view>

namespace karma::network::content {
namespace {

void HashBytesFNV1a(uint64_t& hash, std::string_view value) {
    for (const char ch : value) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(ch));
        hash *= karma::common::content::kFNV1aPrime64;
    }
}

void HashBytesFNV1a(uint64_t& hash, const std::byte* bytes, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        hash ^= static_cast<uint64_t>(std::to_integer<unsigned char>(bytes[i]));
        hash *= karma::common::content::kFNV1aPrime64;
    }
}

void HashSeparatorFNV1a(uint64_t& hash) {
    hash ^= static_cast<uint64_t>(0);
    hash *= karma::common::content::kFNV1aPrime64;
}

} // namespace

void HashChunkChainFNV1a(uint64_t& hash,
                         uint32_t chunk_index,
                         const std::vector<std::byte>& chunk_data) {
    HashBytesFNV1a(hash, std::to_string(chunk_index));
    HashSeparatorFNV1a(hash);
    HashBytesFNV1a(hash, chunk_data.data(), chunk_data.size());
    HashSeparatorFNV1a(hash);
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

} // namespace karma::network::content

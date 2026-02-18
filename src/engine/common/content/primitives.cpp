#include "karma/common/content/primitives.hpp"
#include "karma/network/content/transfer_integrity.hpp"

#include <iomanip>
#include <sstream>
#include <string>

namespace karma::common::content {

uint64_t HashStringFNV1a(std::string_view value) {
    uint64_t hash = kFNV1aOffsetBasis64;
    HashStringFNV1a(hash, value);
    return hash;
}

void HashStringFNV1a(uint64_t& hash, std::string_view value) {
    for (const char ch : value) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(ch));
        hash *= kFNV1aPrime64;
    }
}

void HashBytesFNV1a(uint64_t& hash, const std::byte* bytes, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        hash ^= static_cast<uint64_t>(std::to_integer<unsigned char>(bytes[i]));
        hash *= kFNV1aPrime64;
    }
}

void HashBytesFNV1a(uint64_t& hash, std::string_view value) {
    const auto* bytes = reinterpret_cast<const std::byte*>(value.data());
    HashBytesFNV1a(hash, bytes, value.size());
}

void HashSeparatorFNV1a(uint64_t& hash) {
    hash ^= static_cast<uint64_t>(0);
    hash *= kFNV1aPrime64;
}

std::string Hash64Hex(uint64_t hash) {
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

void HashChunkChainFNV1a(uint64_t& hash,
                         uint32_t chunk_index,
                         const std::vector<std::byte>& chunk_data) {
    karma::network::content::HashChunkChainFNV1a(hash, chunk_index, chunk_data);
}

bool IsChunkInTransferBounds(uint64_t total_bytes,
                             uint32_t chunk_size,
                             uint32_t chunk_index,
                             size_t chunk_bytes) {
    return karma::network::content::IsChunkInTransferBounds(
        total_bytes, chunk_size, chunk_index, chunk_bytes);
}

bool ChunkMatchesBufferedPayload(const std::vector<std::byte>& payload,
                                 size_t chunk_offset,
                                 const std::vector<std::byte>& chunk_data) {
    return karma::network::content::ChunkMatchesBufferedPayload(
        payload, chunk_offset, chunk_data);
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

std::string ComputeWorldPackageHash(const std::vector<std::byte>& bytes) {
    uint64_t hash = kFNV1aOffsetBasis64;
    HashBytesFNV1a(hash, bytes.data(), bytes.size());
    return Hash64Hex(hash);
}

} // namespace karma::common::content

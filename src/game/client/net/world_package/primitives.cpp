#include "client/net/world_package/internal.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace bz3::client::net {

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
    uint64_t hash = kFNV1aOffsetBasis64;
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

void HashChunkChainFNV1a(uint64_t& hash, uint32_t chunk_index, const std::vector<std::byte>& chunk_data) {
    HashBytesFNV1a(hash, std::to_string(chunk_index));
    HashSeparatorFNV1a(hash);
    HashBytesFNV1a(hash, chunk_data.data(), chunk_data.size());
    HashSeparatorFNV1a(hash);
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


} // namespace bz3::client::net

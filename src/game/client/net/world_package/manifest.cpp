#include "client/net/world_package/internal.hpp"

#include "karma/common/logging.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace bz3::client::net {

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


} // namespace bz3::client::net

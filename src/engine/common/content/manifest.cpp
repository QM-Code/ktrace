#include "karma/common/content/manifest.hpp"

#include "karma/common/content/primitives.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace karma::common::content {

namespace {

bool ManifestEntryLess(const ManifestEntry& lhs, const ManifestEntry& rhs) {
    if (lhs.path != rhs.path) {
        return lhs.path < rhs.path;
    }
    if (lhs.size != rhs.size) {
        return lhs.size < rhs.size;
    }
    return lhs.hash < rhs.hash;
}

} // namespace

std::optional<DirectoryManifestSummary> ComputeDirectoryManifestSummary(
    const std::filesystem::path& root) {
    try {
        std::vector<std::filesystem::path> files{};
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            files.push_back(entry.path());
        }
        std::sort(files.begin(), files.end());

        uint64_t content_hash = kFNV1aOffsetBasis64;
        uint64_t manifest_hash = kFNV1aOffsetBasis64;
        std::array<char, 64 * 1024> buffer{};
        const std::byte separator = std::byte{0};
        std::vector<ManifestEntry> entries{};
        entries.reserve(files.size());

        for (const auto& file_path : files) {
            const std::filesystem::path rel_path = std::filesystem::relative(file_path, root);
            const std::string rel = rel_path.generic_string();
            HashStringFNV1a(content_hash, rel);
            HashBytesFNV1a(content_hash, &separator, 1);

            std::ifstream input(file_path, std::ios::binary);
            if (!input) {
                return std::nullopt;
            }

            uint64_t file_hash = kFNV1aOffsetBasis64;
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

            entries.push_back(ManifestEntry{
                .path = rel,
                .size = file_size,
                .hash = file_hash_hex});
        }

        return DirectoryManifestSummary{
            .content_hash = Hash64Hex(content_hash),
            .manifest_hash = Hash64Hex(manifest_hash),
            .entries = std::move(entries)};
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::string ComputeManifestHash(const std::vector<ManifestEntry>& manifest) {
    if (manifest.empty()) {
        return {};
    }

    std::vector<const ManifestEntry*> ordered_entries{};
    ordered_entries.reserve(manifest.size());
    for (const auto& entry : manifest) {
        ordered_entries.push_back(&entry);
    }
    std::sort(ordered_entries.begin(),
              ordered_entries.end(),
              [](const ManifestEntry* lhs, const ManifestEntry* rhs) {
                  return ManifestEntryLess(*lhs, *rhs);
              });

    uint64_t hash = kFNV1aOffsetBasis64;
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

std::vector<ManifestEntry> SortManifestEntries(const std::vector<ManifestEntry>& entries) {
    auto ordered = entries;
    std::sort(ordered.begin(), ordered.end(), ManifestEntryLess);
    return ordered;
}

bool ManifestEntriesEqual(const std::vector<ManifestEntry>& lhs,
                          const std::vector<ManifestEntry>& rhs) {
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

ManifestDiffPlan BuildManifestDiffPlan(const std::vector<ManifestEntry>& cached_manifest,
                                       const std::vector<ManifestEntry>& incoming_manifest) {
    ManifestDiffPlan plan{};
    plan.cached_entries = cached_manifest.size();
    plan.incoming_entries = incoming_manifest.size();

    if (incoming_manifest.empty()) {
        return plan;
    }
    plan.incoming_manifest_available = true;

    for (const auto& entry : incoming_manifest) {
        plan.potential_transfer_bytes += entry.size;
    }

    if (cached_manifest.empty()) {
        plan.added_entries = incoming_manifest.size();
        plan.delta_transfer_bytes = plan.potential_transfer_bytes;
        plan.changed_paths.reserve(incoming_manifest.size());
        for (const auto& entry : incoming_manifest) {
            plan.changed_paths.push_back(entry.path);
        }
        return plan;
    }

    std::unordered_map<std::string, const ManifestEntry*> cached_by_path{};
    cached_by_path.reserve(cached_manifest.size());
    for (const auto& entry : cached_manifest) {
        cached_by_path[entry.path] = &entry;
    }

    for (const auto& entry : incoming_manifest) {
        const auto it = cached_by_path.find(entry.path);
        if (it == cached_by_path.end()) {
            ++plan.added_entries;
            plan.changed_paths.push_back(entry.path);
            continue;
        }
        const ManifestEntry& cached_entry = *it->second;
        if (cached_entry.size == entry.size && cached_entry.hash == entry.hash) {
            ++plan.unchanged_entries;
            plan.reused_bytes += entry.size;
        } else {
            ++plan.modified_entries;
            plan.changed_paths.push_back(entry.path);
        }
        cached_by_path.erase(it);
    }
    plan.removed_entries = cached_by_path.size();
    plan.removed_paths.reserve(plan.removed_entries);
    for (const auto& [path, entry_ptr] : cached_by_path) {
        (void)entry_ptr;
        plan.removed_paths.push_back(path);
    }
    plan.delta_transfer_bytes = plan.potential_transfer_bytes - plan.reused_bytes;
    return plan;
}

} // namespace karma::common::content

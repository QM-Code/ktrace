#pragma once

#include "karma/common/content/types.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace karma::common::content {

struct ManifestEntry {
    std::string path{};
    uint64_t size = 0;
    std::string hash{};
};

struct DirectoryManifestSummary {
    std::string content_hash{};
    std::string manifest_hash{};
    std::vector<ManifestEntry> entries{};
};

struct ManifestDiffPlan {
    bool incoming_manifest_available = false;
    size_t cached_entries = 0;
    size_t incoming_entries = 0;
    size_t unchanged_entries = 0;
    size_t added_entries = 0;
    size_t modified_entries = 0;
    size_t removed_entries = 0;
    uint64_t potential_transfer_bytes = 0;
    uint64_t reused_bytes = 0;
    uint64_t delta_transfer_bytes = 0;
    std::vector<std::string> changed_paths{};
    std::vector<std::string> removed_paths{};
};

std::optional<DirectoryManifestSummary> ComputeDirectoryManifestSummary(const std::filesystem::path& root);
std::string ComputeManifestHash(const std::vector<ManifestEntry>& manifest);
std::vector<ManifestEntry> SortManifestEntries(const std::vector<ManifestEntry>& entries);
bool ManifestEntriesEqual(const std::vector<ManifestEntry>& lhs,
                          const std::vector<ManifestEntry>& rhs);
ManifestDiffPlan BuildManifestDiffPlan(const std::vector<ManifestEntry>& cached_manifest,
                                       const std::vector<ManifestEntry>& incoming_manifest);

} // namespace karma::common::content

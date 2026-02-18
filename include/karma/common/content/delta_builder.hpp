#pragma once

#include "karma/common/content/manifest.hpp"
#include "karma/common/content/types.hpp"

#include <filesystem>
#include <optional>
#include <string_view>

namespace karma::common::content {

inline constexpr const char* kDeltaRemovedPathsFile = "__bz3_delta_removed_paths.txt";
inline constexpr const char* kDeltaMetaFile = "__bz3_delta_meta.txt";

std::optional<ArchiveBytes> BuildDeltaArchiveFromManifestDiff(
    const std::filesystem::path& world_dir,
    const ManifestDiffPlan& diff_plan,
    std::string_view world_id,
    std::string_view target_world_revision,
    std::string_view base_world_revision,
    std::string_view log_prefix);

} // namespace karma::common::content

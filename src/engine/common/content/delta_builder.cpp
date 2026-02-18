#include "karma/common/content/delta_builder.hpp"

#include "karma/common/content/archive.hpp"
#include "karma/common/content/primitives.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <fstream>

namespace karma::common::content {

std::optional<ArchiveBytes> BuildDeltaArchiveFromManifestDiff(
    const std::filesystem::path& world_dir,
    const ManifestDiffPlan& diff_plan,
    std::string_view world_id,
    std::string_view target_world_revision,
    std::string_view base_world_revision,
    std::string_view log_prefix) {
    try {
        const auto nonce = static_cast<unsigned long long>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        const std::filesystem::path staging_dir =
            std::filesystem::temp_directory_path() / ("bz3-world-delta-" + std::to_string(nonce));
        const std::filesystem::path archive_path = staging_dir.string() + ".zip";
        std::error_code ec;

        auto cleanup = [&]() {
            std::filesystem::remove_all(staging_dir, ec);
            ec.clear();
            std::filesystem::remove(archive_path, ec);
        };

        std::filesystem::remove_all(staging_dir, ec);
        ec.clear();
        std::filesystem::create_directories(staging_dir, ec);
        if (ec) {
            spdlog::error("{}: failed to create delta staging dir '{}': {}",
                          log_prefix,
                          staging_dir.string(),
                          ec.message());
            cleanup();
            return std::nullopt;
        }

        for (const auto& rel_path : diff_plan.changed_paths) {
            std::filesystem::path normalized_rel{};
            if (!NormalizeRelativePath(rel_path, &normalized_rel)) {
                spdlog::error("{}: invalid delta path '{}' for world '{}'",
                              log_prefix,
                              rel_path,
                              world_id);
                cleanup();
                return std::nullopt;
            }
            const std::filesystem::path source_path = world_dir / normalized_rel;
            if (!std::filesystem::exists(source_path) || !std::filesystem::is_regular_file(source_path)) {
                spdlog::error("{}: delta source missing '{}' for world '{}'",
                              log_prefix,
                              source_path.string(),
                              world_id);
                cleanup();
                return std::nullopt;
            }
            const std::filesystem::path target_path = staging_dir / normalized_rel;
            std::filesystem::create_directories(target_path.parent_path(), ec);
            if (ec) {
                spdlog::error("{}: failed to create delta parent dir '{}': {}",
                              log_prefix,
                              target_path.parent_path().string(),
                              ec.message());
                cleanup();
                return std::nullopt;
            }
            std::filesystem::copy_file(source_path,
                                       target_path,
                                       std::filesystem::copy_options::overwrite_existing,
                                       ec);
            if (ec) {
                spdlog::error("{}: failed to copy delta file '{}' -> '{}': {}",
                              log_prefix,
                              source_path.string(),
                              target_path.string(),
                              ec.message());
                cleanup();
                return std::nullopt;
            }
        }

        {
            std::ofstream removed_out(staging_dir / kDeltaRemovedPathsFile, std::ios::trunc);
            if (!removed_out) {
                spdlog::error("{}: failed to write delta removals file for world '{}'",
                              log_prefix,
                              world_id);
                cleanup();
                return std::nullopt;
            }
            for (const auto& removed : diff_plan.removed_paths) {
                removed_out << removed << '\n';
            }
        }

        {
            std::ofstream meta_out(staging_dir / kDeltaMetaFile, std::ios::trunc);
            if (!meta_out) {
                spdlog::error("{}: failed to write delta meta file for world '{}'",
                              log_prefix,
                              world_id);
                cleanup();
                return std::nullopt;
            }
            meta_out << "world_id=" << world_id << '\n';
            meta_out << "base_world_revision=" << base_world_revision << '\n';
            meta_out << "target_world_revision=" << target_world_revision << '\n';
            meta_out << "changed_entries=" << diff_plan.changed_paths.size() << '\n';
            meta_out << "removed_entries=" << diff_plan.removed_paths.size() << '\n';
        }

        auto delta_bytes = BuildWorldArchive(staging_dir);
        cleanup();
        return delta_bytes;
    } catch (const std::exception& ex) {
        spdlog::error("{}: failed to build world delta archive for world '{}': {}",
                      log_prefix,
                      world_id,
                      ex.what());
        return std::nullopt;
    }
}

} // namespace karma::common::content

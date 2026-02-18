#include "karma/common/content/package_apply.hpp"

#include "karma/common/content/archive.hpp"
#include "karma/common/content/delta_builder.hpp"
#include "karma/common/content/primitives.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace karma::common::content {

namespace {

bool VerifyExtractedPackage(const std::filesystem::path& package_root,
                            std::string_view world_name,
                            std::string_view expected_world_content_hash,
                            std::string_view expected_world_manifest_hash,
                            uint32_t expected_world_manifest_file_count,
                            const std::vector<ManifestEntry>& expected_world_manifest,
                            std::string_view stage_name,
                            std::string_view log_prefix) {
    const auto summary = ComputeDirectoryManifestSummary(package_root);
    if (!summary.has_value()) {
        spdlog::error("{}: failed to verify {} world package '{}' at '{}'",
                      log_prefix,
                      stage_name,
                      world_name,
                      package_root.string());
        return false;
    }

    if (!expected_world_content_hash.empty() &&
        summary->content_hash != expected_world_content_hash) {
        spdlog::error("{}: {} content hash mismatch for world '{}' (expected='{}' got='{}')",
                      log_prefix,
                      stage_name,
                      world_name,
                      expected_world_content_hash,
                      summary->content_hash);
        return false;
    }

    if (!expected_world_manifest_hash.empty() &&
        summary->manifest_hash != expected_world_manifest_hash) {
        spdlog::error("{}: {} manifest hash mismatch for world '{}' (expected='{}' got='{}')",
                      log_prefix,
                      stage_name,
                      world_name,
                      expected_world_manifest_hash,
                      summary->manifest_hash);
        return false;
    }

    if (expected_world_manifest_file_count > 0 &&
        summary->entries.size() != expected_world_manifest_file_count) {
        spdlog::error("{}: {} manifest file count mismatch for world '{}' (expected={} got={})",
                      log_prefix,
                      stage_name,
                      world_name,
                      expected_world_manifest_file_count,
                      summary->entries.size());
        return false;
    }

    if (!expected_world_manifest.empty()) {
        const auto expected = SortManifestEntries(expected_world_manifest);
        const auto actual = SortManifestEntries(summary->entries);
        if (!ManifestEntriesEqual(expected, actual)) {
            spdlog::error("{}: {} manifest entries mismatch for world '{}' (expected_entries={} got_entries={})",
                          log_prefix,
                          stage_name,
                          world_name,
                          expected.size(),
                          actual.size());
            return false;
        }
    }

    return true;
}

} // namespace

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

void CleanupStaleTemporaryDirectories(const std::filesystem::path& package_root,
                                      std::string_view log_prefix) {
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
            spdlog::warn("{}: failed to remove stale package temp dir '{}': {}",
                         log_prefix,
                         entry.path().string(),
                         remove_ec.message());
        }
    }
}

bool ActivateStagedPackageRootAtomically(const std::filesystem::path& package_root,
                                         const std::filesystem::path& staging_root,
                                         std::string_view log_prefix) {
    const std::filesystem::path backup_root = BuildPackageBackupRoot(package_root);
    std::error_code ec;
    bool moved_existing_root = false;
    const bool package_root_exists = std::filesystem::exists(package_root, ec);
    if (ec) {
        spdlog::error("{}: failed to query package directory '{}': {}",
                      log_prefix,
                      package_root.string(),
                      ec.message());
        return false;
    }

    if (package_root_exists) {
        std::filesystem::remove_all(backup_root, ec);
        ec.clear();
        std::filesystem::rename(package_root, backup_root, ec);
        if (ec) {
            spdlog::error("{}: failed to move package '{}' -> '{}': {}",
                          log_prefix,
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
                spdlog::error("{}: failed to rollback package '{}' -> '{}' after activation failure: {}",
                              log_prefix,
                              backup_root.string(),
                              package_root.string(),
                              rollback_ec.message());
            }
        }
        spdlog::error("{}: failed to activate staged package '{}' -> '{}': {}",
                      log_prefix,
                      staging_root.string(),
                      package_root.string(),
                      ec.message());
        return false;
    }

    if (moved_existing_root) {
        std::error_code remove_ec;
        std::filesystem::remove_all(backup_root, remove_ec);
        if (remove_ec) {
            spdlog::warn("{}: failed to remove previous package backup '{}': {}",
                         log_prefix,
                         backup_root.string(),
                         remove_ec.message());
        }
    }

    CleanupStaleTemporaryDirectories(package_root, log_prefix);
    return true;
}

bool ExtractArchiveAtomically(const ArchiveBytes& world_data,
                              const std::filesystem::path& package_root,
                              std::string_view world_name,
                              std::string_view expected_world_content_hash,
                              std::string_view expected_world_manifest_hash,
                              uint32_t expected_world_manifest_file_count,
                              const std::vector<ManifestEntry>& expected_world_manifest,
                              std::string_view log_prefix) {
    const std::filesystem::path staging_root = BuildPackageStagingRoot(package_root);
    std::error_code ec;
    std::filesystem::remove_all(staging_root, ec);
    ec.clear();
    std::filesystem::create_directories(staging_root, ec);
    if (ec) {
        spdlog::error("{}: failed to create staging directory '{}': {}",
                      log_prefix,
                      staging_root.string(),
                      ec.message());
        return false;
    }

    if (!ExtractWorldArchive(world_data, staging_root)) {
        std::filesystem::remove_all(staging_root, ec);
        spdlog::error("{}: failed to extract world archive into staging '{}'",
                      log_prefix,
                      staging_root.string());
        return false;
    }

    if (!VerifyExtractedPackage(staging_root,
                                world_name,
                                expected_world_content_hash,
                                expected_world_manifest_hash,
                                expected_world_manifest_file_count,
                                expected_world_manifest,
                                "staged full",
                                log_prefix)) {
        std::filesystem::remove_all(staging_root, ec);
        return false;
    }

    return ActivateStagedPackageRootAtomically(package_root, staging_root, log_prefix);
}

bool ApplyDeltaArchiveOverBasePackage(const std::filesystem::path& target_root,
                                      const std::filesystem::path& base_root,
                                      std::string_view world_name,
                                      std::string_view expected_world_content_hash,
                                      std::string_view expected_world_manifest_hash,
                                      uint32_t expected_world_manifest_file_count,
                                      const std::vector<ManifestEntry>& expected_world_manifest,
                                      const ArchiveBytes& delta_archive,
                                      std::string_view log_prefix,
                                      size_t* removed_paths_out) {
    if (removed_paths_out) {
        *removed_paths_out = 0;
    }

    const std::filesystem::path staging_root = BuildPackageStagingRoot(target_root);
    std::error_code ec;
    std::filesystem::remove_all(staging_root, ec);
    ec.clear();
    std::filesystem::create_directories(staging_root.parent_path(), ec);
    if (ec) {
        spdlog::error("{}: failed to create target package parent '{}': {}",
                      log_prefix,
                      staging_root.parent_path().string(),
                      ec.message());
        return false;
    }

    std::filesystem::copy(base_root, staging_root, std::filesystem::copy_options::recursive, ec);
    if (ec) {
        spdlog::error("{}: failed to clone delta base package '{}' -> '{}': {}",
                      log_prefix,
                      base_root.string(),
                      staging_root.string(),
                      ec.message());
        return false;
    }

    if (!ExtractWorldArchive(delta_archive, staging_root)) {
        spdlog::error("{}: failed to extract world delta archive into '{}'",
                      log_prefix,
                      staging_root.string());
        std::filesystem::remove_all(staging_root, ec);
        return false;
    }

    size_t removed_paths = 0;
    const std::filesystem::path removed_paths_file = staging_root / kDeltaRemovedPathsFile;
    if (std::filesystem::exists(removed_paths_file) && std::filesystem::is_regular_file(removed_paths_file)) {
        std::ifstream removals_in(removed_paths_file);
        if (!removals_in) {
            spdlog::error("{}: failed to read delta removals file '{}'",
                          log_prefix,
                          removed_paths_file.string());
            std::filesystem::remove_all(staging_root, ec);
            return false;
        }
        std::string line{};
        while (std::getline(removals_in, line)) {
            if (line.empty()) {
                continue;
            }
            std::filesystem::path normalized_rel{};
            if (!NormalizeRelativePath(line, &normalized_rel)) {
                spdlog::error("{}: invalid delta removal path '{}'", log_prefix, line);
                std::filesystem::remove_all(staging_root, ec);
                return false;
            }
            std::error_code remove_ec;
            std::filesystem::remove_all(staging_root / normalized_rel, remove_ec);
            if (remove_ec) {
                spdlog::error("{}: failed to apply delta removal path '{}' in '{}': {}",
                              log_prefix,
                              line,
                              staging_root.string(),
                              remove_ec.message());
                std::filesystem::remove_all(staging_root, ec);
                return false;
            }
            ++removed_paths;
        }
    }

    std::filesystem::remove(staging_root / kDeltaRemovedPathsFile, ec);
    ec.clear();
    std::filesystem::remove(staging_root / kDeltaMetaFile, ec);
    ec.clear();

    if (!VerifyExtractedPackage(staging_root,
                                world_name,
                                expected_world_content_hash,
                                expected_world_manifest_hash,
                                expected_world_manifest_file_count,
                                expected_world_manifest,
                                "staged delta",
                                log_prefix)) {
        std::filesystem::remove_all(staging_root, ec);
        return false;
    }

    if (!ActivateStagedPackageRootAtomically(target_root, staging_root, log_prefix)) {
        std::filesystem::remove_all(staging_root, ec);
        return false;
    }

    if (removed_paths_out) {
        *removed_paths_out = removed_paths;
    }
    return true;
}

} // namespace karma::common::content

#pragma once

#include "karma/common/content/manifest.hpp"
#include "karma/common/content/types.hpp"

#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

namespace karma::common::content {

std::filesystem::path BuildPackageStagingRoot(const std::filesystem::path& package_root);
std::filesystem::path BuildPackageBackupRoot(const std::filesystem::path& package_root);
void CleanupStaleTemporaryDirectories(const std::filesystem::path& package_root,
                                      std::string_view log_prefix);
bool ActivateStagedPackageRootAtomically(const std::filesystem::path& package_root,
                                         const std::filesystem::path& staging_root,
                                         std::string_view log_prefix);

bool ExtractArchiveAtomically(const ArchiveBytes& world_data,
                              const std::filesystem::path& package_root,
                              std::string_view world_name,
                              std::string_view expected_world_content_hash,
                              std::string_view expected_world_manifest_hash,
                              uint32_t expected_world_manifest_file_count,
                              const std::vector<ManifestEntry>& expected_world_manifest,
                              std::string_view log_prefix);

bool ApplyDeltaArchiveOverBasePackage(const std::filesystem::path& target_root,
                                      const std::filesystem::path& base_root,
                                      std::string_view world_name,
                                      std::string_view expected_world_content_hash,
                                      std::string_view expected_world_manifest_hash,
                                      uint32_t expected_world_manifest_file_count,
                                      const std::vector<ManifestEntry>& expected_world_manifest,
                                      const ArchiveBytes& delta_archive,
                                      std::string_view log_prefix,
                                      size_t* removed_paths_out = nullptr);

} // namespace karma::common::content

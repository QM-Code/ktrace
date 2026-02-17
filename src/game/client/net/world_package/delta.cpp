#include "client/net/world_package/internal.hpp"

#include "karma/common/logging.hpp"
#include "karma/common/world_archive.hpp"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace bz3::client::net {

bool ApplyDeltaArchiveOverCachedBase(const std::filesystem::path& server_cache_dir,
                                     std::string_view world_name,
                                     std::string_view world_id,
                                     std::string_view world_revision,
                                     std::string_view world_hash,
                                     std::string_view world_content_hash,
                                     std::string_view world_manifest_hash,
                                     uint32_t world_manifest_file_count,
                                     const std::vector<bz3::net::WorldManifestEntry>& world_manifest,
                                     std::string_view base_world_id,
                                     std::string_view base_world_revision,
                                     std::string_view base_world_hash,
                                     std::string_view base_world_content_hash,
                                     const std::vector<std::byte>& delta_archive) {
    if (base_world_id.empty() || base_world_revision.empty()) {
        spdlog::error("ClientConnection: delta transfer missing base world identity metadata");
        return false;
    }
    if (base_world_id != world_id) {
        spdlog::error("ClientConnection: delta transfer base world id mismatch target='{}' base='{}'",
                      world_id,
                      base_world_id);
        return false;
    }

    const std::string target_package_cache_key = ResolveWorldPackageCacheKey(world_content_hash, world_hash);
    const std::filesystem::path target_root =
        PackageRootForIdentity(server_cache_dir, world_id, world_revision, target_package_cache_key);
    const std::filesystem::path staging_root = BuildPackageStagingRoot(target_root);
    const std::string base_package_cache_key =
        ResolveWorldPackageCacheKey(base_world_content_hash, base_world_hash);
    const std::filesystem::path base_root = PackageRootForIdentity(server_cache_dir,
                                                                   base_world_id,
                                                                   base_world_revision,
                                                                   base_package_cache_key);
    if (!std::filesystem::exists(base_root) || !std::filesystem::is_directory(base_root)) {
        spdlog::error("ClientConnection: delta base world package is missing '{}' for id='{}' rev='{}'",
                      base_root.string(),
                      base_world_id,
                      base_world_revision);
        return false;
    }

    std::error_code ec;
    std::filesystem::remove_all(staging_root, ec);
    ec.clear();
    std::filesystem::create_directories(staging_root.parent_path(), ec);
    if (ec) {
        spdlog::error("ClientConnection: failed to create target package parent '{}': {}",
                      staging_root.parent_path().string(),
                      ec.message());
        return false;
    }

    std::filesystem::copy(base_root, staging_root, std::filesystem::copy_options::recursive, ec);
    if (ec) {
        spdlog::error("ClientConnection: failed to clone delta base package '{}' -> '{}': {}",
                      base_root.string(),
                      staging_root.string(),
                      ec.message());
        return false;
    }

    if (!world::ExtractWorldArchive(delta_archive, staging_root)) {
        spdlog::error("ClientConnection: failed to extract world delta archive into '{}'",
                      staging_root.string());
        std::filesystem::remove_all(staging_root, ec);
        return false;
    }

    size_t removed_paths = 0;
    const std::filesystem::path removed_paths_file = staging_root / kDeltaRemovedPathsFile;
    if (std::filesystem::exists(removed_paths_file) && std::filesystem::is_regular_file(removed_paths_file)) {
        std::ifstream removals_in(removed_paths_file);
        if (!removals_in) {
            spdlog::error("ClientConnection: failed to read delta removals file '{}'",
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
                spdlog::error("ClientConnection: invalid delta removal path '{}'", line);
                std::filesystem::remove_all(staging_root, ec);
                return false;
            }
            std::error_code remove_ec;
            std::filesystem::remove_all(staging_root / normalized_rel, remove_ec);
            if (remove_ec) {
                spdlog::error("ClientConnection: failed to apply delta removal path '{}' in '{}': {}",
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

    if (!VerifyExtractedWorldPackage(staging_root,
                                     world_name,
                                     world_content_hash,
                                     world_manifest_hash,
                                     world_manifest_file_count,
                                     world_manifest,
                                     "staged delta")) {
        std::filesystem::remove_all(staging_root, ec);
        return false;
    }

    if (!ActivateStagedPackageRootAtomically(target_root, staging_root)) {
        std::filesystem::remove_all(staging_root, ec);
        return false;
    }

    KARMA_TRACE("net.client",
                "ClientConnection: applied world delta archive target='{}' base='{}' removed_paths={}",
                target_root.string(),
                base_root.string(),
                removed_paths);
    return true;
}


} // namespace bz3::client::net

#include "client/net/world_package/internal.hpp"

#include "karma/common/config_store.hpp"
#include "karma/common/content/archive.hpp"
#include "karma/common/content/sync_facade.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "karma/common/logging.hpp"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace bz3::client::net {

namespace {

std::vector<karma::content::ManifestEntry> ToContentManifest(
    const std::vector<bz3::net::WorldManifestEntry>& manifest) {
    std::vector<karma::content::ManifestEntry> converted{};
    converted.reserve(manifest.size());
    for (const auto& entry : manifest) {
        converted.push_back(karma::content::ManifestEntry{
            .path = entry.path,
            .size = entry.size,
            .hash = entry.hash});
    }
    return converted;
}

} // namespace

namespace detail {

bool ApplyWorldPackageForServer(const std::string& host,
                                uint16_t port,
                                std::string_view world_name,
                                std::string_view world_id,
                                std::string_view world_revision,
                                std::string_view world_hash,
                                std::string_view world_content_hash,
                                std::string_view world_manifest_hash,
                                uint32_t world_manifest_file_count,
                                uint64_t world_size,
                                const std::vector<bz3::net::WorldManifestEntry>& world_manifest,
                                const std::vector<std::byte>& world_data,
                                bool is_delta_transfer,
                                std::string_view delta_base_world_id,
                                std::string_view delta_base_world_revision,
                                std::string_view delta_base_world_hash,
                                std::string_view delta_base_world_content_hash) {
    const std::filesystem::path server_cache_dir =
        karma::data::EnsureUserWorldDirectoryForServer(host, port);
    const std::filesystem::path active_identity_path = ActiveWorldIdentityPath(server_cache_dir);
    const std::filesystem::path active_manifest_path = ActiveWorldManifestPath(server_cache_dir);
    const std::filesystem::path world_packages_by_world_root = WorldPackagesByWorldRoot(server_cache_dir);
    if (world_id.empty() || world_revision.empty()) {
        spdlog::error("ClientConnection: missing world identity metadata for world '{}' (id='{}' rev='{}')",
                      world_name,
                      world_id,
                      world_revision);
        return false;
    }

    if (world_data.empty() && world_hash.empty() && world_content_hash.empty()) {
        static_cast<void>(karma::config::ConfigStore::RemoveRuntimeLayer(kRuntimeLayerLabel));
        karma::data::ClearPackageMounts();
        ClearCachedWorldIdentity(server_cache_dir);
        KARMA_TRACE("net.client",
                    "ClientConnection: bundled world mode '{}' id='{}' rev='{}' (no world package transfer)",
                    world_name,
                    world_id,
                    world_revision);
        return true;
    }

    karma::content::ClientContentSyncRequest sync_request{};
    sync_request.source_host = host;
    sync_request.source_port = port;
    sync_request.world_packages_by_world_root = world_packages_by_world_root;
    sync_request.active_identity_path = active_identity_path;
    sync_request.active_manifest_path = active_manifest_path;
    sync_request.world_name = std::string(world_name);
    sync_request.world_id = std::string(world_id);
    sync_request.world_revision = std::string(world_revision);
    sync_request.world_hash = std::string(world_hash);
    sync_request.world_content_hash = std::string(world_content_hash);
    sync_request.world_manifest_hash = std::string(world_manifest_hash);
    sync_request.world_manifest_file_count = world_manifest_file_count;
    sync_request.world_size = world_size;
    sync_request.world_manifest = ToContentManifest(world_manifest);
    sync_request.world_data = world_data;
    sync_request.is_delta_transfer = is_delta_transfer;
    sync_request.delta_base_world_id = std::string(delta_base_world_id);
    sync_request.delta_base_world_revision = std::string(delta_base_world_revision);
    sync_request.delta_base_world_hash = std::string(delta_base_world_hash);
    sync_request.delta_base_world_content_hash = std::string(delta_base_world_content_hash);
    sync_request.require_exact_revision = true;
    sync_request.max_revisions_per_world = kDefaultMaxRevisionsPerWorld;
    sync_request.max_packages_per_revision = kDefaultMaxPackagesPerRevision;
    sync_request.max_component_len = kMaxCachePathComponentLen;

    karma::content::ClientContentSyncResult sync_result{};
    if (!karma::content::ApplyIncomingPackageToCache(sync_request,
                                                     &sync_result,
                                                     "ClientConnection")) {
        if (world_data.empty() &&
            (!world_hash.empty() || !world_content_hash.empty()) &&
            !world_id.empty() &&
            !world_revision.empty()) {
            ClearCachedWorldIdentity(server_cache_dir);
        }
        return false;
    }

    auto world_config = karma::content::ReadWorldJsonFile(sync_result.package_root / "config.json");
    if (world_config.has_value() && !world_config->is_object()) {
        spdlog::error("ClientConnection: world package config.json is not a JSON object for world '{}'",
                      world_name);
        return false;
    }

    static_cast<void>(karma::config::ConfigStore::RemoveRuntimeLayer(kRuntimeLayerLabel));
    karma::data::ClearPackageMounts();
    karma::data::RegisterPackageMount(kPackageMountId, sync_result.package_root);

    if (world_config.has_value() &&
        !karma::config::ConfigStore::AddRuntimeLayer(kRuntimeLayerLabel,
                                                     *world_config,
                                                     sync_result.package_root)) {
        karma::data::ClearPackageMounts();
        spdlog::error("ClientConnection: failed to add runtime layer for world '{}'", world_name);
        return false;
    }

    KARMA_TRACE("net.client",
                "ClientConnection: applied world package world='{}' id='{}' rev='{}' hash='{}' content_hash='{}' bytes={} manifest_entries={} cache='{}' cache_hit={} transfer_mode={}",
                world_name,
                world_id,
                world_revision,
                world_hash.empty() ? "-" : std::string(world_hash),
                world_content_hash.empty() ? "-" : std::string(world_content_hash),
                world_data.size(),
                sync_result.effective_world_manifest.size(),
                sync_result.package_root.string(),
                sync_result.cache_hit ? 1 : 0,
                sync_result.transfer_mode);
    return true;
}

} // namespace detail

} // namespace bz3::client::net

#pragma once

#include "karma/common/content/cache_store.hpp"
#include "karma/common/content/manifest.hpp"
#include "karma/common/content/types.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace karma::content {

struct ServerCachedContentState {
    std::string world_hash{};
    std::string world_id{};
    std::string world_revision{};
    std::string world_content_hash{};
    std::string world_manifest_hash{};
    uint32_t world_manifest_file_count = 0;
    std::vector<ManifestEntry> world_manifest{};
};

struct ServerContentSyncRequest {
    std::filesystem::path world_dir{};
    std::string world_name{};
    std::string world_id{};
    std::string world_revision{};
    std::string world_package_hash{};
    std::string world_content_hash{};
    std::string world_manifest_hash{};
    uint32_t world_manifest_file_count = 0;
    std::vector<ManifestEntry> world_manifest{};
    ArchiveBytes world_package{};
    ServerCachedContentState cached_state{};
};

struct ServerContentSyncPlan {
    bool cache_identity_match = false;
    bool cache_hash_match = false;
    bool cache_content_match = false;
    bool cache_manifest_match = false;
    bool cache_hit = false;
    std::string cache_reason{"miss"};
    ManifestDiffPlan manifest_diff{};
    bool send_world_package = false;
    bool send_manifest_entries = false;
    std::string transfer_mode{"none"};
    uint64_t transfer_bytes = 0;
    bool transfer_is_delta = false;
    std::string transfer_delta_base_world_id{};
    std::string transfer_delta_base_world_revision{};
    std::string transfer_delta_base_world_hash{};
    std::string transfer_delta_base_world_content_hash{};
    ArchiveBytes delta_world_package{};
};

ServerContentSyncPlan BuildDefaultServerContentSyncPlan(const ServerContentSyncRequest& request,
                                                        std::string_view log_prefix);

struct ClientContentSyncRequest {
    std::string source_host{};
    uint16_t source_port = 0;
    std::filesystem::path world_packages_by_world_root{};
    std::filesystem::path active_identity_path{};
    std::filesystem::path active_manifest_path{};
    std::string world_name{};
    std::string world_id{};
    std::string world_revision{};
    std::string world_hash{};
    std::string world_content_hash{};
    std::string world_manifest_hash{};
    uint32_t world_manifest_file_count = 0;
    uint64_t world_size = 0;
    std::vector<ManifestEntry> world_manifest{};
    ArchiveBytes world_data{};
    bool is_delta_transfer = false;
    std::string delta_base_world_id{};
    std::string delta_base_world_revision{};
    std::string delta_base_world_hash{};
    std::string delta_base_world_content_hash{};
    bool require_exact_revision = true;
    uint16_t max_revisions_per_world = kDefaultMaxRevisionsPerWorld;
    uint16_t max_packages_per_revision = kDefaultMaxPackagesPerRevision;
    size_t max_component_len = kMaxCachePathComponentLen;
};

struct ClientContentSyncResult {
    std::vector<ManifestEntry> cached_manifest{};
    std::vector<ManifestEntry> effective_world_manifest{};
    std::string world_package_cache_key{};
    std::filesystem::path package_root{};
    bool cache_hit = false;
    std::string transfer_mode{"none"};
};

bool ApplyIncomingPackageToCache(const ClientContentSyncRequest& request,
                                 ClientContentSyncResult* result,
                                 std::string_view log_prefix);

} // namespace karma::content

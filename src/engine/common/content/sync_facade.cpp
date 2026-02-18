#include "karma/common/content/sync_facade.hpp"

#include "karma/common/content/delta_builder.hpp"
#include "karma/common/content/package_apply.hpp"
#include "karma/common/content/primitives.hpp"
#include "karma/common/logging/logging.hpp"

#include <spdlog/spdlog.h>

#include <filesystem>

namespace karma::common::content {

namespace {

void LogServerTransferSelectionDecision(const ServerContentSyncRequest& request,
                                        const ServerContentSyncPlan& plan,
                                        std::string_view log_prefix) {
    KARMA_TRACE(
        "net.server",
        "{}: world transfer selection world='{}' id='{}' rev='{}' full_bytes={} delta_bytes={} saved_bytes={} selected_mode={} reason={}",
        log_prefix,
        request.world_name.empty() ? "-" : request.world_name,
        request.world_id.empty() ? "-" : request.world_id,
        request.world_revision.empty() ? "-" : request.world_revision,
        plan.transfer_full_bytes,
        plan.transfer_delta_bytes,
        plan.transfer_saved_bytes,
        plan.transfer_mode,
        plan.transfer_selection_reason);
}

void LogClientManifestDiffPlan(std::string_view world_name,
                               const std::vector<ManifestEntry>& cached_manifest,
                               const std::vector<ManifestEntry>& incoming_manifest,
                               std::string_view log_prefix) {
    if (incoming_manifest.empty()) {
        KARMA_TRACE("net.client",
                    "{}: manifest diff plan skipped world='{}' (incoming manifest unavailable, cached_entries={})",
                    log_prefix,
                    world_name,
                    cached_manifest.size());
        return;
    }

    const auto plan = BuildManifestDiffPlan(cached_manifest, incoming_manifest);
    if (cached_manifest.empty()) {
        KARMA_TRACE("net.client",
                    "{}: manifest diff plan world='{}' cached_entries=0 incoming_entries={} unchanged=0 added={} modified=0 removed=0 potential_transfer_bytes={} reused_bytes=0",
                    log_prefix,
                    world_name,
                    incoming_manifest.size(),
                    incoming_manifest.size(),
                    plan.potential_transfer_bytes);
        return;
    }

    KARMA_TRACE("net.client",
                "{}: manifest diff plan world='{}' cached_entries={} incoming_entries={} unchanged={} added={} modified={} removed={} potential_transfer_bytes={} reused_bytes={}",
                log_prefix,
                world_name,
                cached_manifest.size(),
                incoming_manifest.size(),
                plan.unchanged_entries,
                plan.added_entries,
                plan.modified_entries,
                plan.removed_entries,
                plan.potential_transfer_bytes,
                plan.reused_bytes);
}

bool ValidateCachedIdentityForRequest(const ClientContentSyncRequest& request,
                                      const std::vector<ManifestEntry>& cached_manifest,
                                      std::string_view log_prefix) {
    const auto identity = ReadCachedIdentityFile(request.active_identity_path);
    if (!identity.has_value()) {
        spdlog::error("{}: cache identity metadata is missing for world '{}'",
                      log_prefix,
                      request.world_name);
        return false;
    }

    const bool id_match = identity->world_id == request.world_id;
    const bool revision_match = identity->world_revision == request.world_revision;
    const bool hash_match =
        !request.world_hash.empty() && identity->world_hash == request.world_hash;
    const bool content_hash_match = !request.world_content_hash.empty() &&
                                    identity->world_content_hash == request.world_content_hash;
    bool manifest_match = false;
    uint32_t cached_manifest_file_count = 0;
    std::string cached_manifest_hash{};
    if (!request.world_manifest_hash.empty()) {
        cached_manifest_file_count = static_cast<uint32_t>(cached_manifest.size());
        cached_manifest_hash = ComputeManifestHash(cached_manifest);
        manifest_match = cached_manifest_hash == request.world_manifest_hash &&
                         cached_manifest_file_count == request.world_manifest_file_count;
    }

    const bool package_match = hash_match || content_hash_match || manifest_match;
    if (!id_match || (request.require_exact_revision && !revision_match) || !package_match) {
        spdlog::error("{}: cache identity mismatch for world '{}' (expected hash='{}' content_hash='{}' id='{}' rev='{}' manifest_hash='{}' manifest_files={} require_exact_revision={}, got hash='{}' content_hash='{}' id='{}' rev='{}' manifest_hash='{}' manifest_files={})",
                      log_prefix,
                      request.world_name,
                      request.world_hash,
                      request.world_content_hash,
                      request.world_id,
                      request.world_revision,
                      request.world_manifest_hash,
                      request.world_manifest_file_count,
                      request.require_exact_revision ? 1 : 0,
                      identity->world_hash,
                      identity->world_content_hash,
                      identity->world_id,
                      identity->world_revision,
                      cached_manifest_hash,
                      cached_manifest_file_count);
        return false;
    }

    return true;
}

void LogClientCachePruneResult(const CachePruneResult& result, std::string_view log_prefix) {
    for (const auto& warning : result.warnings) {
        switch (warning.kind) {
        case CachePruneWarningKind::PrunePackage:
            spdlog::warn("{}: failed to prune cached world package '{}': {}",
                         log_prefix,
                         warning.path.string(),
                         warning.message);
            break;
        case CachePruneWarningKind::RemoveEmptyRevision:
            spdlog::warn("{}: failed to remove empty cached revision '{}': {}",
                         log_prefix,
                         warning.path.string(),
                         warning.message);
            break;
        case CachePruneWarningKind::PruneRevision:
            spdlog::warn("{}: failed to prune cached world revision '{}': {}",
                         log_prefix,
                         warning.path.string(),
                         warning.message);
            break;
        case CachePruneWarningKind::RemoveEmptyWorldDir:
            spdlog::warn("{}: failed to remove empty cached world dir '{}': {}",
                         log_prefix,
                         warning.path.string(),
                         warning.message);
            break;
        }
    }

    for (const auto& path : result.pruned_package_paths) {
        KARMA_TRACE("net.client",
                    "{}: pruned cached world package '{}'",
                    log_prefix,
                    path.string());
    }
    for (const auto& path : result.pruned_revision_paths) {
        KARMA_TRACE("net.client",
                    "{}: pruned cached world revision '{}'",
                    log_prefix,
                    path.string());
    }

    KARMA_TRACE("net.client",
                "{}: cache prune summary worlds={} revisions={} packages={} pruned_revisions={} pruned_packages={}",
                log_prefix,
                result.scanned_world_dirs,
                result.scanned_revision_dirs,
                result.scanned_package_dirs,
                result.pruned_revision_dirs,
                result.pruned_package_dirs);
}

} // namespace

ServerCachedContentState BuildServerCachedContentState(
    std::string_view world_hash,
    std::string_view world_id,
    std::string_view world_revision,
    std::string_view world_content_hash,
    std::string_view world_manifest_hash,
    uint32_t world_manifest_file_count,
    std::vector<ManifestEntry> world_manifest) {
    ServerCachedContentState cached_state{};
    cached_state.world_hash = std::string(world_hash);
    cached_state.world_id = std::string(world_id);
    cached_state.world_revision = std::string(world_revision);
    cached_state.world_content_hash = std::string(world_content_hash);
    cached_state.world_manifest_hash = std::string(world_manifest_hash);
    cached_state.world_manifest_file_count = world_manifest_file_count;
    cached_state.world_manifest = std::move(world_manifest);
    return cached_state;
}

ServerContentSyncRequest BuildServerContentSyncRequest(
    const std::filesystem::path& world_dir,
    std::string_view world_name,
    std::string_view world_id,
    std::string_view world_revision,
    std::string_view world_package_hash,
    std::string_view world_content_hash,
    std::string_view world_manifest_hash,
    uint32_t world_manifest_file_count,
    std::vector<ManifestEntry> world_manifest,
    const ArchiveBytes& world_package,
    ServerCachedContentState cached_state) {
    ServerContentSyncRequest request{};
    request.world_dir = world_dir;
    request.world_name = std::string(world_name);
    request.world_id = std::string(world_id);
    request.world_revision = std::string(world_revision);
    request.world_package_hash = std::string(world_package_hash);
    request.world_content_hash = std::string(world_content_hash);
    request.world_manifest_hash = std::string(world_manifest_hash);
    request.world_manifest_file_count = world_manifest_file_count;
    request.world_manifest = std::move(world_manifest);
    request.world_package = world_package;
    request.cached_state = std::move(cached_state);
    return request;
}

ServerContentSyncPlan BuildDefaultServerContentSyncPlan(const ServerContentSyncRequest& request,
                                                        std::string_view log_prefix) {
    ServerContentSyncPlan plan{};
    plan.cache_identity_match = !request.world_id.empty() && !request.world_revision.empty() &&
                               request.cached_state.world_id == request.world_id &&
                               request.cached_state.world_revision == request.world_revision;
    plan.cache_hash_match = !request.world_package_hash.empty() &&
                            request.cached_state.world_hash == request.world_package_hash;
    plan.cache_content_match = !request.world_content_hash.empty() &&
                               request.cached_state.world_content_hash == request.world_content_hash;
    plan.cache_manifest_match = !request.world_manifest_hash.empty() &&
                                request.cached_state.world_manifest_hash == request.world_manifest_hash &&
                                request.cached_state.world_manifest_file_count ==
                                    request.world_manifest_file_count;
    plan.cache_hit = plan.cache_identity_match &&
                     (plan.cache_hash_match || plan.cache_content_match || plan.cache_manifest_match);
    if (plan.cache_hit) {
        if (plan.cache_hash_match) {
            plan.cache_reason = "package_hash";
        } else if (plan.cache_content_match) {
            plan.cache_reason = "content_hash";
        } else {
            plan.cache_reason = "manifest_summary";
        }
    }

    plan.manifest_diff = BuildManifestDiffPlan(request.cached_state.world_manifest,
                                               request.world_manifest);
    plan.send_world_package = !request.world_package.empty() && !plan.cache_hit;
    plan.send_manifest_entries = !plan.cache_hit || !plan.cache_manifest_match;
    plan.transfer_full_bytes = request.world_package.size();
    if (!plan.send_world_package) {
        plan.transfer_selection_reason = plan.cache_hit ? "cache_hit" : "world_package_missing";
        LogServerTransferSelectionDecision(request, plan, log_prefix);
        return plan;
    }

    plan.transfer_mode = "chunked_full";
    plan.transfer_bytes = plan.transfer_full_bytes;
    const bool same_world_base_identity = !request.world_id.empty() &&
                                          request.cached_state.world_id == request.world_id &&
                                          !request.cached_state.world_revision.empty();
    const bool has_manifest_diff = plan.manifest_diff.incoming_manifest_available;
    const bool has_delta_reuse_signal =
        plan.manifest_diff.reused_bytes > 0 || plan.manifest_diff.removed_entries > 0;
    const bool can_try_delta =
        has_manifest_diff && same_world_base_identity && has_delta_reuse_signal;
    if (!can_try_delta) {
        if (!has_manifest_diff) {
            plan.transfer_selection_reason = "manifest_diff_unavailable";
        } else if (!same_world_base_identity) {
            plan.transfer_selection_reason = "base_identity_mismatch";
        } else {
            plan.transfer_selection_reason = "manifest_diff_no_reuse";
        }
        LogServerTransferSelectionDecision(request, plan, log_prefix);
        return plan;
    }

    const auto delta_archive = BuildDeltaArchiveFromManifestDiff(request.world_dir,
                                                                 plan.manifest_diff,
                                                                 request.world_id,
                                                                 request.world_revision,
                                                                 request.cached_state.world_revision,
                                                                 log_prefix);
    if (!delta_archive.has_value()) {
        plan.transfer_selection_reason = "delta_build_failed";
        LogServerTransferSelectionDecision(request, plan, log_prefix);
        return plan;
    }

    plan.transfer_delta_bytes = delta_archive->size();
    if (plan.transfer_delta_bytes == 0) {
        plan.transfer_selection_reason = "delta_empty";
        LogServerTransferSelectionDecision(request, plan, log_prefix);
        return plan;
    }

    if (plan.transfer_delta_bytes >= plan.transfer_full_bytes) {
        plan.transfer_selection_reason = "delta_not_smaller";
        LogServerTransferSelectionDecision(request, plan, log_prefix);
        return plan;
    }

    plan.transfer_saved_bytes = plan.transfer_full_bytes - plan.transfer_delta_bytes;
    if (plan.transfer_saved_bytes < kMinDeltaSelectionSavingsBytes) {
        plan.transfer_selection_reason = "delta_savings_below_minimum";
        LogServerTransferSelectionDecision(request, plan, log_prefix);
        return plan;
    }

    plan.delta_world_package = *delta_archive;
    plan.transfer_bytes = plan.transfer_delta_bytes;
    plan.transfer_is_delta = true;
    plan.transfer_mode = "chunked_delta";
    plan.transfer_delta_base_world_id = request.cached_state.world_id;
    plan.transfer_delta_base_world_revision = request.cached_state.world_revision;
    plan.transfer_delta_base_world_hash = request.cached_state.world_hash;
    plan.transfer_delta_base_world_content_hash = request.cached_state.world_content_hash;
    plan.transfer_selection_reason = "delta_selected";
    LogServerTransferSelectionDecision(request, plan, log_prefix);
    return plan;
}

bool ApplyIncomingPackageToCache(const ClientContentSyncRequest& request,
                                 ClientContentSyncResult* result,
                                 std::string_view log_prefix) {
    if (result == nullptr) {
        return false;
    }
    *result = {};

    if (request.world_id.empty() || request.world_revision.empty()) {
        spdlog::error("{}: missing world identity metadata for world '{}' (id='{}' rev='{}')",
                      log_prefix,
                      request.world_name,
                      request.world_id,
                      request.world_revision);
        return false;
    }

    bool malformed_manifest = false;
    result->cached_manifest = ReadCachedManifestFile(request.active_manifest_path, &malformed_manifest);
    if (malformed_manifest) {
        spdlog::warn("{}: cached world manifest '{}' is malformed; ignoring",
                     log_prefix,
                     request.active_manifest_path.string());
    }

    if (!request.world_manifest.empty()) {
        result->effective_world_manifest = request.world_manifest;
    } else {
        const bool can_reuse_cached_manifest =
            request.world_data.empty() &&
            !request.world_manifest_hash.empty() &&
            request.world_manifest_file_count > 0 &&
            !result->cached_manifest.empty() &&
            ComputeManifestHash(result->cached_manifest) == request.world_manifest_hash &&
            result->cached_manifest.size() == request.world_manifest_file_count;
        if (can_reuse_cached_manifest) {
            result->effective_world_manifest = result->cached_manifest;
            KARMA_TRACE("net.client",
                        "{}: init omitted manifest entries for world='{}'; reusing cached manifest entries={} hash='{}'",
                        log_prefix,
                        request.world_name,
                        result->effective_world_manifest.size(),
                        request.world_manifest_hash);
        } else if (request.world_data.empty() && !request.world_manifest_hash.empty() &&
                   request.world_manifest_file_count > 0) {
            spdlog::warn("{}: init omitted manifest entries for world '{}' but cached manifest is unavailable/mismatched (manifest_hash='{}' manifest_files={})",
                         log_prefix,
                         request.world_name,
                         request.world_manifest_hash,
                         request.world_manifest_file_count);
        }
    }
    LogClientManifestDiffPlan(request.world_name,
                              result->cached_manifest,
                              result->effective_world_manifest,
                              log_prefix);

    result->world_package_cache_key =
        ResolveWorldPackageCacheKey(request.world_content_hash, request.world_hash);
    result->package_root = PackageRootForIdentity(request.world_packages_by_world_root,
                                                  request.world_id,
                                                  request.world_revision,
                                                  result->world_package_cache_key,
                                                  request.max_component_len);

    if (!request.world_data.empty()) {
        if (request.is_delta_transfer) {
            if (request.delta_base_world_id.empty() || request.delta_base_world_revision.empty()) {
                spdlog::error("{}: delta transfer missing base world identity metadata", log_prefix);
                return false;
            }
            if (request.delta_base_world_id != request.world_id) {
                spdlog::error("{}: delta transfer base world id mismatch target='{}' base='{}'",
                              log_prefix,
                              request.world_id,
                              request.delta_base_world_id);
                return false;
            }

            const std::string base_package_cache_key =
                ResolveWorldPackageCacheKey(request.delta_base_world_content_hash,
                                            request.delta_base_world_hash);
            const std::filesystem::path base_root = PackageRootForIdentity(
                request.world_packages_by_world_root,
                request.delta_base_world_id,
                request.delta_base_world_revision,
                base_package_cache_key,
                request.max_component_len);
            if (!std::filesystem::exists(base_root) || !std::filesystem::is_directory(base_root)) {
                spdlog::error("{}: delta base world package is missing '{}' for id='{}' rev='{}'",
                              log_prefix,
                              base_root.string(),
                              request.delta_base_world_id,
                              request.delta_base_world_revision);
                return false;
            }

            size_t removed_paths = 0;
            if (!ApplyDeltaArchiveOverBasePackage(result->package_root,
                                                  base_root,
                                                  request.world_name,
                                                  request.world_content_hash,
                                                  request.world_manifest_hash,
                                                  request.world_manifest_file_count,
                                                  result->effective_world_manifest,
                                                  request.world_data,
                                                  log_prefix,
                                                  &removed_paths)) {
                return false;
            }

            KARMA_TRACE("net.client",
                        "{}: applied world delta archive target='{}' base='{}' removed_paths={}",
                        log_prefix,
                        result->package_root.string(),
                        base_root.string(),
                        removed_paths);
            result->transfer_mode = "delta";
        } else {
            if (request.world_size > 0 && request.world_size != request.world_data.size()) {
                spdlog::error("{}: world package size mismatch for '{}' (expected={} got={})",
                              log_prefix,
                              request.world_name,
                              request.world_size,
                              request.world_data.size());
                return false;
            }
            if (!request.world_hash.empty()) {
                const std::string computed_hash = ComputeWorldPackageHash(request.world_data);
                if (computed_hash != request.world_hash) {
                    spdlog::error("{}: world package hash mismatch for '{}' (expected={} got={})",
                                  log_prefix,
                                  request.world_name,
                                  request.world_hash,
                                  computed_hash);
                    return false;
                }
            }

            if (!ExtractArchiveAtomically(request.world_data,
                                          result->package_root,
                                          request.world_name,
                                          request.world_content_hash,
                                          request.world_manifest_hash,
                                          request.world_manifest_file_count,
                                          result->effective_world_manifest,
                                          log_prefix)) {
                spdlog::error("{}: failed to extract world package for world '{}'",
                              log_prefix,
                              request.world_name);
                return false;
            }
            result->transfer_mode = "full";
        }
    } else {
        if (!std::filesystem::exists(result->package_root) || !std::filesystem::is_directory(result->package_root)) {
            spdlog::error("{}: server skipped world package transfer for '{}' hash='{}' content_hash='{}', but cache is missing",
                          log_prefix,
                          request.world_name,
                          request.world_hash,
                          request.world_content_hash);
            return false;
        }
        if (!ValidateCachedIdentityForRequest(request, result->cached_manifest, log_prefix)) {
            return false;
        }
        result->cache_hit = true;
        result->transfer_mode = "none";
    }

    if (!PersistCachedIdentityFile(request.active_identity_path,
                                   request.world_hash,
                                   request.world_content_hash,
                                   request.world_id,
                                   request.world_revision)) {
        if (!request.source_host.empty() && request.source_port > 0) {
            spdlog::warn("{}: failed to persist cached world identity hash='{}' content_hash='{}' id='{}' rev='{}' for {}:{}",
                         log_prefix,
                         request.world_hash,
                         request.world_content_hash,
                         request.world_id,
                         request.world_revision,
                         request.source_host,
                         request.source_port);
        } else {
            spdlog::warn("{}: failed to persist cached world identity hash='{}' content_hash='{}' id='{}' rev='{}'",
                         log_prefix,
                         request.world_hash,
                         request.world_content_hash,
                         request.world_id,
                         request.world_revision);
        }
    }
    if (!PersistCachedManifestFile(request.active_manifest_path, result->effective_world_manifest)) {
        if (!request.source_host.empty() && request.source_port > 0) {
            spdlog::warn("{}: failed to persist cached world manifest entries={} for {}:{}",
                         log_prefix,
                         result->effective_world_manifest.size(),
                         request.source_host,
                         request.source_port);
        } else {
            spdlog::warn("{}: failed to persist cached world manifest entries={}",
                         log_prefix,
                         result->effective_world_manifest.size());
        }
    }

    TouchPathIfPresent(result->package_root);
    TouchPathIfPresent(result->package_root.parent_path());
    TouchPathIfPresent(result->package_root.parent_path().parent_path());
    const auto prune_result = PruneWorldPackageCache(request.world_packages_by_world_root,
                                                     request.world_id,
                                                     request.world_revision,
                                                     result->world_package_cache_key,
                                                     request.max_revisions_per_world,
                                                     request.max_packages_per_revision,
                                                     request.max_component_len);
    LogClientCachePruneResult(prune_result, log_prefix);
    return true;
}

} // namespace karma::common::content

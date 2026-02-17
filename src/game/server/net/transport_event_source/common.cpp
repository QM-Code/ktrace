#include "server/net/transport_event_source/internal.hpp"

#include "karma/common/logging.hpp"
#include "karma/common/world_archive.hpp"
#include "net/protocol_codec.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <exception>
#include <fstream>

namespace bz3::server::net::detail {

std::string DefaultPlayerName(uint32_t client_id) {
    return "player-" + std::to_string(client_id);
}

std::string ResolvePlayerName(const bz3::net::ClientMessage& message, uint32_t client_id) {
    if (!message.player_name.empty()) {
        return message.player_name;
    }
    return DefaultPlayerName(client_id);
}

bool NormalizeRelativePath(std::string_view raw_path, std::filesystem::path* out) {
    if (!out || raw_path.empty()) {
        return false;
    }
    std::filesystem::path normalized = std::filesystem::path(raw_path).lexically_normal();
    if (normalized.empty() || normalized == "." || normalized.is_absolute() ||
        normalized.has_root_path()) {
        return false;
    }
    for (const auto& part : normalized) {
        if (part == "..") {
            return false;
        }
    }
    *out = normalized;
    return true;
}

ManifestDiffPlan BuildServerManifestDiffPlan(const std::vector<WorldManifestEntry>& cached_manifest,
                                             const std::vector<WorldManifestEntry>& incoming_manifest) {
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

    std::unordered_map<std::string, const WorldManifestEntry*> cached_by_path{};
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
        const WorldManifestEntry& cached_entry = *it->second;
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

void LogServerManifestDiffPlan(uint32_t client_id,
                               std::string_view world_name,
                               const ManifestDiffPlan& plan) {
    if (!plan.incoming_manifest_available) {
        KARMA_TRACE("net.server",
                    "ServerEventSource: manifest diff plan skipped client_id={} world='{}' (incoming manifest unavailable, cached_entries={})",
                    client_id,
                    world_name,
                    plan.cached_entries);
        return;
    }

    KARMA_TRACE("net.server",
                "ServerEventSource: manifest diff plan client_id={} world='{}' cached_entries={} incoming_entries={} unchanged={} added={} modified={} removed={} potential_transfer_bytes={} reused_bytes={} delta_transfer_bytes={}",
                client_id,
                world_name,
                plan.cached_entries,
                plan.incoming_entries,
                plan.unchanged_entries,
                plan.added_entries,
                plan.modified_entries,
                plan.removed_entries,
                plan.potential_transfer_bytes,
                plan.reused_bytes,
                plan.delta_transfer_bytes);
}

std::optional<std::vector<std::byte>> BuildWorldDeltaArchive(const std::filesystem::path& world_dir,
                                                             const ManifestDiffPlan& diff_plan,
                                                             std::string_view world_id,
                                                             std::string_view target_world_revision,
                                                             std::string_view base_world_revision) {
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
            spdlog::error("ServerEventSource: failed to create delta staging dir '{}': {}",
                          staging_dir.string(),
                          ec.message());
            cleanup();
            return std::nullopt;
        }

        for (const auto& rel_path : diff_plan.changed_paths) {
            std::filesystem::path normalized_rel{};
            if (!NormalizeRelativePath(rel_path, &normalized_rel)) {
                spdlog::error("ServerEventSource: invalid delta path '{}' for world '{}'",
                              rel_path,
                              world_id);
                cleanup();
                return std::nullopt;
            }
            const std::filesystem::path source_path = world_dir / normalized_rel;
            if (!std::filesystem::exists(source_path) || !std::filesystem::is_regular_file(source_path)) {
                spdlog::error("ServerEventSource: delta source missing '{}' for world '{}'",
                              source_path.string(),
                              world_id);
                cleanup();
                return std::nullopt;
            }
            const std::filesystem::path target_path = staging_dir / normalized_rel;
            std::filesystem::create_directories(target_path.parent_path(), ec);
            if (ec) {
                spdlog::error("ServerEventSource: failed to create delta parent dir '{}': {}",
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
                spdlog::error("ServerEventSource: failed to copy delta file '{}' -> '{}': {}",
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
                spdlog::error("ServerEventSource: failed to write delta removals file for world '{}'",
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
                spdlog::error("ServerEventSource: failed to write delta meta file for world '{}'",
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

        auto delta_bytes = world::BuildWorldArchive(staging_dir);
        cleanup();
        return delta_bytes;
    } catch (const std::exception& ex) {
        spdlog::error("ServerEventSource: failed to build world delta archive for world '{}': {}",
                      world_id,
                      ex.what());
        return std::nullopt;
    }
}

} // namespace bz3::server::net::detail

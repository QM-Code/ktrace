#include "server/runtime/internal.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace bz3::server::runtime_detail {

std::vector<net::SessionSnapshotEntry> ToNetSessionSnapshot(
    const std::vector<karma::network::ServerSessionSnapshotEntry>& sessions) {
    std::vector<net::SessionSnapshotEntry> out{};
    out.reserve(sessions.size());
    for (const auto& session : sessions) {
        out.push_back(net::SessionSnapshotEntry{
            session.session_id,
            session.session_name});
    }
    return out;
}

std::vector<net::WorldManifestEntry> ToNetWorldManifest(
    const std::vector<karma::network::ServerWorldManifestEntry>& manifest) {
    std::vector<net::WorldManifestEntry> out{};
    out.reserve(manifest.size());
    for (const auto& entry : manifest) {
        out.push_back(net::WorldManifestEntry{
            .path = entry.path,
            .size = entry.size,
            .hash = entry.hash});
    }
    return out;
}

void EmitJoinResultToEventSource(net::ServerEventSource& event_source,
                                 const karma::network::ServerJoinResultPayload& payload) {
    static const std::vector<net::WorldManifestEntry> empty_world_manifest{};
    static const std::vector<std::byte> empty_world_package{};

    const auto* world_state = payload.world;
    const auto* world_manifest =
        (world_state && world_state->world_manifest) ? world_state->world_manifest : nullptr;
    const auto* world_package =
        (world_state && world_state->world_package) ? world_state->world_package : &empty_world_package;
    const auto net_sessions = ToNetSessionSnapshot(payload.sessions);
    const auto net_world_manifest = world_manifest ? ToNetWorldManifest(*world_manifest) : empty_world_manifest;
    event_source.onJoinResult(payload.client_id,
                              payload.accepted,
                              payload.reason,
                              world_state ? world_state->world_name : std::string_view{},
                              world_state ? world_state->world_id : std::string_view{},
                              world_state ? world_state->world_revision : std::string_view{},
                              world_state ? world_state->world_package_hash : std::string_view{},
                              world_state ? world_state->world_content_hash : std::string_view{},
                              world_state ? world_state->world_manifest_hash : std::string_view{},
                              world_state ? world_state->world_manifest_file_count : 0U,
                              world_state ? world_state->world_package_size : 0U,
                              world_state ? world_state->world_dir : std::filesystem::path{},
                              net_sessions,
                              net_world_manifest,
                              *world_package);
}

} // namespace bz3::server::runtime_detail

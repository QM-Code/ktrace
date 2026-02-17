#include "client/net/connection.hpp"
#include "client/net/connection/internal.hpp"

#include "karma/network/client_transport.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "karma/common/logging.hpp"
#include "net/protocol_codec.hpp"
#include "net/protocol.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace bz3::client::net {

bool ClientConnection::sendRequestPlayerSpawn() {
    if (!connected_ || !transport_ || !transport_->isConnected() || assigned_client_id_ == 0) {
        return false;
    }

    const auto payload = bz3::net::EncodeClientRequestPlayerSpawn(assigned_client_id_);
    if (payload.empty()) {
        return false;
    }

    if (!sendPayloadReliable(payload)) {
        return false;
    }
    KARMA_TRACE("net.client",
                "ClientConnection: sent request_player_spawn client_id={}",
                assigned_client_id_);
    return true;
}

bool ClientConnection::sendCreateShot() {
    if (!connected_ || !transport_ || !transport_->isConnected() || assigned_client_id_ == 0) {
        return false;
    }

    const uint32_t local_shot_id = next_local_shot_id_++;
    const auto payload = bz3::net::EncodeClientCreateShot(
        assigned_client_id_,
        local_shot_id,
        bz3::net::Vec3{0.0f, 0.0f, 0.0f},
        bz3::net::Vec3{0.0f, 0.0f, 0.0f});
    if (payload.empty()) {
        return false;
    }

    if (!sendPayloadReliable(payload)) {
        return false;
    }
    KARMA_TRACE("net.client",
                "ClientConnection: sent create_shot client_id={} local_shot_id={}",
                assigned_client_id_,
                local_shot_id);
    return true;
}

bool ClientConnection::sendJoinRequest() {
    if (!connected_ || !transport_ || !transport_->isConnected() || join_sent_) {
        return connected_ && join_sent_;
    }

    const auto cached_world_identity = detail::ReadCachedWorldIdentityForServer(host_, port_);
    const std::string_view cached_world_hash =
        cached_world_identity.has_value() ? std::string_view(cached_world_identity->world_hash) : std::string_view{};
    const std::string_view cached_world_id =
        cached_world_identity.has_value() ? std::string_view(cached_world_identity->world_id) : std::string_view{};
    const std::string_view cached_world_revision =
        cached_world_identity.has_value() ? std::string_view(cached_world_identity->world_revision) : std::string_view{};
    const std::string_view cached_world_content_hash =
        cached_world_identity.has_value() ? std::string_view(cached_world_identity->world_content_hash) : std::string_view{};
    std::string cached_world_manifest_hash_storage{};
    uint32_t cached_world_manifest_file_count = 0;
    std::vector<bz3::net::WorldManifestEntry> cached_world_manifest{};
    if (cached_world_identity.has_value()) {
        const std::filesystem::path server_cache_dir =
            karma::data::EnsureUserWorldDirectoryForServer(host_, port_);
        cached_world_manifest = detail::ReadCachedWorldManifest(server_cache_dir);
        cached_world_manifest_hash_storage = detail::ComputeManifestHash(cached_world_manifest);
        cached_world_manifest_file_count = static_cast<uint32_t>(cached_world_manifest.size());
    }
    const std::string_view cached_world_manifest_hash = cached_world_manifest_hash_storage;
    const auto payload = bz3::net::EncodeClientJoinRequest(player_name_,
                                                           bz3::net::kProtocolVersion,
                                                           cached_world_hash,
                                                           cached_world_id,
                                                           cached_world_revision,
                                                           cached_world_content_hash,
                                                           cached_world_manifest_hash,
                                                           cached_world_manifest_file_count,
                                                           cached_world_manifest,
                                                           auth_payload_);
    if (payload.empty()) {
        return false;
    }

    if (!sendPayloadReliable(payload)) {
        return false;
    }
    join_sent_ = true;

    KARMA_TRACE("net.client",
                "ClientConnection: sent join request name='{}' protocol={} auth_payload_present={} cached_world_hash='{}' cached_world_id='{}' cached_world_revision='{}' cached_world_content_hash='{}' cached_world_manifest_hash='{}' cached_world_manifest_files={} cached_world_manifest_entries={} to {}:{}",
                player_name_,
                bz3::net::kProtocolVersion,
                auth_payload_.empty() ? 0 : 1,
                cached_world_hash.empty() ? "-" : cached_world_hash,
                cached_world_id.empty() ? "-" : cached_world_id,
                cached_world_revision.empty() ? "-" : cached_world_revision,
                cached_world_content_hash.empty() ? "-" : cached_world_content_hash,
                cached_world_manifest_hash.empty() ? "-" : cached_world_manifest_hash,
                cached_world_manifest_file_count,
                cached_world_manifest.size(),
                host_,
                port_);
    return true;
}

bool ClientConnection::sendLeave() {
    if (!connected_ || !transport_ || !transport_->isConnected() || leave_sent_) {
        return connected_ && leave_sent_;
    }

    const auto payload = bz3::net::EncodeClientLeave(assigned_client_id_);
    if (payload.empty()) {
        return false;
    }

    if (!sendPayloadReliable(payload)) {
        return false;
    }
    leave_sent_ = true;
    KARMA_TRACE("net.client",
                "ClientConnection: sent leave");
    return true;
}

bool ClientConnection::sendPayloadReliable(const std::vector<std::byte>& payload) {
    if (payload.empty() || !transport_ || !transport_->isConnected()) {
        return false;
    }
    return transport_->sendReliable(payload);
}

} // namespace bz3::client::net

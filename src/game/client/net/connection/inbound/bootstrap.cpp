#include "client/net/connection.hpp"
#include "client/net/connection/internal.hpp"

#include "karma/common/logging/logging.hpp"
#include "net/protocol.hpp"

#include <spdlog/spdlog.h>

#include <string>

namespace bz3::client::net {

void ClientConnection::handleJoinResponse(const bz3::net::ServerMessage& message) {
    if (message.join_accepted) {
        KARMA_TRACE("net.client",
                    "ClientConnection: join accepted by {}:{}",
                    host_,
                    port_);
    } else {
        const std::string reason = message.reason.empty()
            ? std::string("Join rejected by server.")
            : message.reason;
        spdlog::error("ClientConnection: join rejected: {}", reason);
        KARMA_TRACE("net.client",
                    "ClientConnection: join rejected by {}:{} reason='{}'",
                    host_,
                    port_,
                    reason);
        should_exit_ = true;
        requestDisconnect();
    }
}

void ClientConnection::handleInit(const bz3::net::ServerMessage& message) {
    assigned_client_id_ = message.client_id;
    init_received_ = true;
    init_world_name_ = message.world_name;
    init_server_name_ = message.server_name;
    KARMA_TRACE("net.client",
                "ClientConnection: init world='{}' id='{}' rev='{}' hash='{}' content_hash='{}' manifest_hash='{}' manifest_files={} manifest_entries={} server='{}' client_id={} protocol={}",
                message.world_name,
                message.world_id.empty() ? "-" : message.world_id,
                message.world_revision.empty() ? "-" : message.world_revision,
                message.world_hash.empty() ? "-" : message.world_hash,
                message.world_content_hash.empty() ? "-" : message.world_content_hash,
                message.world_manifest_hash.empty() ? "-" : message.world_manifest_hash,
                message.world_manifest_file_count,
                message.world_manifest.size(),
                message.server_name,
                message.client_id,
                message.protocol_version);
    if (message.protocol_version != bz3::net::kProtocolVersion) {
        spdlog::error("ClientConnection: init protocol mismatch server={} client={}",
                      message.protocol_version,
                      bz3::net::kProtocolVersion);
        should_exit_ = true;
        requestDisconnect();
        return;
    }

    const bool has_world_metadata = detail::InitIncludesWorldMetadata(message);
    if (has_world_metadata && (message.world_id.empty() || message.world_revision.empty())) {
        spdlog::error("ClientConnection: init world metadata missing identity world='{}' id='{}' rev='{}' size={} hash='{}' content_hash='{}'",
                      message.world_name,
                      message.world_id,
                      message.world_revision,
                      message.world_size,
                      message.world_hash,
                      message.world_content_hash);
        should_exit_ = true;
        requestDisconnect();
        return;
    }
    if (!message.world_manifest.empty() && message.world_manifest_file_count > 0 &&
        message.world_manifest.size() != message.world_manifest_file_count) {
        spdlog::error("ClientConnection: init manifest metadata mismatch world='{}' manifest_files={} manifest_entries={}",
                      message.world_name,
                      message.world_manifest_file_count,
                      message.world_manifest.size());
        should_exit_ = true;
        requestDisconnect();
        return;
    }
    if (!message.world_manifest.empty() && message.world_manifest_hash.empty()) {
        spdlog::error("ClientConnection: init manifest entries missing manifest_hash world='{}' manifest_entries={}",
                      message.world_name,
                      message.world_manifest.size());
        should_exit_ = true;
        requestDisconnect();
        return;
    }
    if (message.world_manifest_file_count > 0 && message.world_manifest_hash.empty()) {
        spdlog::error("ClientConnection: init manifest file count missing manifest_hash world='{}' manifest_files={}",
                      message.world_name,
                      message.world_manifest_file_count);
        should_exit_ = true;
        requestDisconnect();
        return;
    }

    const bool expects_inline_payload = !message.world_data.empty();
    const bool cached_package_available = detail::HasCachedWorldPackageForServer(host_,
                                                                                  port_,
                                                                                  message.world_id,
                                                                                  message.world_revision,
                                                                                  message.world_content_hash,
                                                                                  message.world_hash);
    const bool expect_chunk_transfer =
        !expects_inline_payload && message.world_size > 0 && !cached_package_available;
    if (expect_chunk_transfer) {
        pending_world_package_.active = true;
        pending_world_package_.world_name = message.world_name;
        pending_world_package_.world_id = message.world_id;
        pending_world_package_.world_revision = message.world_revision;
        pending_world_package_.world_hash = message.world_hash;
        pending_world_package_.world_content_hash = message.world_content_hash;
        pending_world_package_.world_manifest_hash = message.world_manifest_hash;
        pending_world_package_.world_manifest_file_count = message.world_manifest_file_count;
        pending_world_package_.world_size = message.world_size;
        pending_world_package_.world_manifest = message.world_manifest;
        active_world_transfer_ = {};
        KARMA_TRACE("net.client",
                    "ClientConnection: init awaiting chunk transfer world='{}' id='{}' rev='{}' bytes={} hash='{}' content_hash='{}' manifest_entries={}",
                    message.world_name,
                    message.world_id.empty() ? "-" : message.world_id,
                    message.world_revision.empty() ? "-" : message.world_revision,
                    message.world_size,
                    message.world_hash.empty() ? "-" : message.world_hash,
                    message.world_content_hash.empty() ? "-" : message.world_content_hash,
                    message.world_manifest.size());
        return;
    }

    if (!detail::ApplyWorldPackageForServer(host_,
                                            port_,
                                            message.world_name,
                                            message.world_id,
                                            message.world_revision,
                                            message.world_hash,
                                            message.world_content_hash,
                                            message.world_manifest_hash,
                                            message.world_manifest_file_count,
                                            message.world_size,
                                            message.world_manifest,
                                            message.world_data)) {
        spdlog::error("ClientConnection: failed to apply world package for '{}'", message.world_name);
        should_exit_ = true;
        requestDisconnect();
    }
}

} // namespace bz3::client::net

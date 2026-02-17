#include "client/net/connection.hpp"
#include "client/net/connection/internal.hpp"

#include <spdlog/spdlog.h>

namespace bz3::client::net {

void ClientConnection::handleWorldTransferBegin(const bz3::net::ServerMessage& message) {
    const karma::network::content::TransferReceiverExpectation expectation{
        .pending_world_package_active = pending_world_package_.active,
        .world_name = pending_world_package_.world_name,
        .world_id = pending_world_package_.world_id,
        .world_revision = pending_world_package_.world_revision,
        .world_hash = pending_world_package_.world_hash,
        .world_content_hash = pending_world_package_.world_content_hash};
    const karma::network::content::TransferBeginPacket packet{
        .transfer_id = message.transfer_id,
        .transfer_world_id = message.transfer_world_id,
        .transfer_world_revision = message.transfer_world_revision,
        .transfer_total_bytes = message.transfer_total_bytes,
        .transfer_chunk_size = message.transfer_chunk_size,
        .transfer_world_hash = message.transfer_world_hash,
        .transfer_world_content_hash = message.transfer_world_content_hash,
        .transfer_is_delta = message.transfer_is_delta,
        .transfer_delta_base_world_id = message.transfer_delta_base_world_id,
        .transfer_delta_base_world_revision = message.transfer_delta_base_world_revision,
        .transfer_delta_base_world_hash = message.transfer_delta_base_world_hash,
        .transfer_delta_base_world_content_hash = message.transfer_delta_base_world_content_hash};
    if (!karma::network::content::HandleTransferBegin(expectation,
                                                      packet,
                                                      &active_world_transfer_,
                                                      "ClientConnection")) {
        should_exit_ = true;
        requestDisconnect();
    }
}

void ClientConnection::handleWorldTransferChunk(const bz3::net::ServerMessage& message) {
    const karma::network::content::TransferChunkPacket packet{
        .transfer_id = message.transfer_id,
        .transfer_chunk_index = message.transfer_chunk_index,
        .transfer_chunk_data = &message.transfer_chunk_data};
    if (!karma::network::content::HandleTransferChunk(packet,
                                                      &active_world_transfer_,
                                                      "ClientConnection")) {
        should_exit_ = true;
        requestDisconnect();
    }
}

void ClientConnection::handleWorldTransferEnd(const bz3::net::ServerMessage& message) {
    const karma::network::content::TransferEndPacket packet{
        .transfer_id = message.transfer_id,
        .transfer_chunk_count = message.transfer_chunk_count,
        .transfer_total_bytes = message.transfer_total_bytes,
        .transfer_world_hash = message.transfer_world_hash,
        .transfer_world_content_hash = message.transfer_world_content_hash};
    if (!karma::network::content::HandleTransferEnd(pending_world_package_.active,
                                                    packet,
                                                    &active_world_transfer_,
                                                    "ClientConnection")) {
        should_exit_ = true;
        requestDisconnect();
        return;
    }
    if (!detail::ApplyWorldPackageForServer(host_,
                                            port_,
                                            pending_world_package_.world_name,
                                            pending_world_package_.world_id,
                                            pending_world_package_.world_revision,
                                            pending_world_package_.world_hash,
                                            pending_world_package_.world_content_hash,
                                            pending_world_package_.world_manifest_hash,
                                            pending_world_package_.world_manifest_file_count,
                                            pending_world_package_.world_size,
                                            pending_world_package_.world_manifest,
                                            active_world_transfer_.payload,
                                            active_world_transfer_.is_delta,
                                            active_world_transfer_.delta_base_world_id,
                                            active_world_transfer_.delta_base_world_revision,
                                            active_world_transfer_.delta_base_world_hash,
                                            active_world_transfer_.delta_base_world_content_hash)) {
        spdlog::error("ClientConnection: failed to apply chunked world package for '{}'",
                      pending_world_package_.world_name);
        should_exit_ = true;
        requestDisconnect();
        return;
    }

    pending_world_package_ = {};
    active_world_transfer_ = {};
}

} // namespace bz3::client::net

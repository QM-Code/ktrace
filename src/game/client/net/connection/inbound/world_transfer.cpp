#include "client/net/connection.hpp"
#include "client/net/connection/internal.hpp"

#include "karma/common/logging.hpp"

#include <spdlog/spdlog.h>

#include <cstddef>
#include <string>

namespace bz3::client::net {

void ClientConnection::handleWorldTransferBegin(const bz3::net::ServerMessage& message) {
    if (!pending_world_package_.active) {
        spdlog::error("ClientConnection: unexpected world transfer begin transfer_id='{}' (no pending world init)",
                      message.transfer_id);
        should_exit_ = true;
        requestDisconnect();
        return;
    }
    if (message.transfer_id.empty()) {
        spdlog::error("ClientConnection: world transfer begin missing transfer_id");
        should_exit_ = true;
        requestDisconnect();
        return;
    }
    if (message.transfer_world_id != pending_world_package_.world_id ||
        message.transfer_world_revision != pending_world_package_.world_revision) {
        spdlog::error("ClientConnection: world transfer begin identity mismatch transfer_id='{}' begin_id='{}' begin_rev='{}' init_id='{}' init_rev='{}'",
                      message.transfer_id,
                      message.transfer_world_id,
                      message.transfer_world_revision,
                      pending_world_package_.world_id,
                      pending_world_package_.world_revision);
        should_exit_ = true;
        requestDisconnect();
        return;
    }
    if (message.transfer_is_delta &&
        (message.transfer_delta_base_world_id.empty() ||
         message.transfer_delta_base_world_revision.empty())) {
        spdlog::error("ClientConnection: world delta transfer begin missing base identity transfer_id='{}' base_id='{}' base_rev='{}'",
                      message.transfer_id,
                      message.transfer_delta_base_world_id,
                      message.transfer_delta_base_world_revision);
        should_exit_ = true;
        requestDisconnect();
        return;
    }
    if (message.transfer_total_bytes > 0 && message.transfer_chunk_size == 0) {
        spdlog::error("ClientConnection: world transfer begin invalid chunk size transfer_id='{}' total_bytes={} chunk_size={}",
                      message.transfer_id,
                      message.transfer_total_bytes,
                      message.transfer_chunk_size);
        should_exit_ = true;
        requestDisconnect();
        return;
    }
    if (!pending_world_package_.world_hash.empty() &&
        message.transfer_world_hash != pending_world_package_.world_hash) {
        spdlog::error("ClientConnection: world transfer begin hash mismatch transfer_id='{}' expected='{}' got='{}'",
                      message.transfer_id,
                      pending_world_package_.world_hash,
                      message.transfer_world_hash);
        should_exit_ = true;
        requestDisconnect();
        return;
    }
    if (!pending_world_package_.world_content_hash.empty() &&
        message.transfer_world_content_hash != pending_world_package_.world_content_hash) {
        spdlog::error("ClientConnection: world transfer begin content_hash mismatch transfer_id='{}' expected='{}' got='{}'",
                      message.transfer_id,
                      pending_world_package_.world_content_hash,
                      message.transfer_world_content_hash);
        should_exit_ = true;
        requestDisconnect();
        return;
    }

    const std::string expected_transfer_world_hash = pending_world_package_.world_hash.empty()
        ? message.transfer_world_hash
        : pending_world_package_.world_hash;
    const std::string expected_transfer_world_content_hash =
        pending_world_package_.world_content_hash.empty()
        ? message.transfer_world_content_hash
        : pending_world_package_.world_content_hash;

    bool resumed_transfer = false;
    uint32_t resumed_chunks = 0;
    size_t resumed_bytes = 0;
    if (active_world_transfer_.active) {
        const bool transfer_compatible =
            active_world_transfer_.total_bytes_expected == message.transfer_total_bytes &&
            active_world_transfer_.chunk_size == message.transfer_chunk_size &&
            active_world_transfer_.is_delta == message.transfer_is_delta &&
            active_world_transfer_.delta_base_world_id == message.transfer_delta_base_world_id &&
            active_world_transfer_.delta_base_world_revision ==
                message.transfer_delta_base_world_revision &&
            active_world_transfer_.delta_base_world_hash == message.transfer_delta_base_world_hash &&
            active_world_transfer_.delta_base_world_content_hash ==
                message.transfer_delta_base_world_content_hash &&
            active_world_transfer_.transfer_world_hash == expected_transfer_world_hash &&
            active_world_transfer_.transfer_world_content_hash ==
                expected_transfer_world_content_hash &&
            active_world_transfer_.payload.size() <= message.transfer_total_bytes;
        if (!transfer_compatible) {
            KARMA_TRACE("net.client",
                        "ClientConnection: world transfer restart reset transfer_id='{}' previous_transfer_id='{}' previous_chunks={} previous_bytes={} total_bytes={} chunk_size={}",
                        message.transfer_id,
                        active_world_transfer_.transfer_id,
                        active_world_transfer_.next_chunk_index,
                        active_world_transfer_.payload.size(),
                        message.transfer_total_bytes,
                        message.transfer_chunk_size);
            active_world_transfer_ = {};
        } else {
            resumed_transfer = active_world_transfer_.next_chunk_index > 0;
            resumed_chunks = active_world_transfer_.next_chunk_index;
            resumed_bytes = active_world_transfer_.payload.size();
        }
    }

    if (!active_world_transfer_.active) {
        active_world_transfer_.active = true;
        active_world_transfer_.is_delta = message.transfer_is_delta;
        active_world_transfer_.delta_base_world_id = message.transfer_delta_base_world_id;
        active_world_transfer_.delta_base_world_revision = message.transfer_delta_base_world_revision;
        active_world_transfer_.delta_base_world_hash = message.transfer_delta_base_world_hash;
        active_world_transfer_.delta_base_world_content_hash =
            message.transfer_delta_base_world_content_hash;
        active_world_transfer_.transfer_world_hash = expected_transfer_world_hash;
        active_world_transfer_.transfer_world_content_hash = expected_transfer_world_content_hash;
        active_world_transfer_.total_bytes_expected = message.transfer_total_bytes;
        active_world_transfer_.chunk_size = message.transfer_chunk_size;
        active_world_transfer_.next_chunk_index = 0;
        active_world_transfer_.payload_hash = detail::kFNV1aOffsetBasis64;
        active_world_transfer_.chunk_chain_hash = detail::kFNV1aOffsetBasis64;
        active_world_transfer_.payload.clear();
        if (message.transfer_total_bytes > 0) {
            active_world_transfer_.payload.reserve(static_cast<size_t>(message.transfer_total_bytes));
        }
    }
    active_world_transfer_.transfer_id = message.transfer_id;
    KARMA_TRACE("net.client",
                "ClientConnection: world transfer begin transfer_id='{}' mode={} world='{}' id='{}' rev='{}' total_bytes={} chunk_size={} hash='{}' content_hash='{}' base_id='{}' base_rev='{}' resume={} resumed_chunks={} resumed_bytes={}",
                active_world_transfer_.transfer_id,
                active_world_transfer_.is_delta ? "delta" : "full",
                pending_world_package_.world_name,
                pending_world_package_.world_id,
                pending_world_package_.world_revision,
                active_world_transfer_.total_bytes_expected,
                active_world_transfer_.chunk_size,
                active_world_transfer_.transfer_world_hash.empty()
                    ? "-"
                    : active_world_transfer_.transfer_world_hash,
                active_world_transfer_.transfer_world_content_hash.empty()
                    ? "-"
                    : active_world_transfer_.transfer_world_content_hash,
                active_world_transfer_.delta_base_world_id.empty()
                    ? "-"
                    : active_world_transfer_.delta_base_world_id,
                active_world_transfer_.delta_base_world_revision.empty()
                    ? "-"
                    : active_world_transfer_.delta_base_world_revision,
                resumed_transfer ? 1 : 0,
                resumed_chunks,
                resumed_bytes);
}

void ClientConnection::handleWorldTransferChunk(const bz3::net::ServerMessage& message) {
    if (!active_world_transfer_.active) {
        spdlog::error("ClientConnection: unexpected world transfer chunk transfer_id='{}' (no active transfer)",
                      message.transfer_id);
        should_exit_ = true;
        requestDisconnect();
        return;
    }
    if (message.transfer_id != active_world_transfer_.transfer_id) {
        spdlog::error("ClientConnection: world transfer chunk transfer_id mismatch expected='{}' got='{}'",
                      active_world_transfer_.transfer_id,
                      message.transfer_id);
        should_exit_ = true;
        requestDisconnect();
        return;
    }
    if (!detail::IsChunkInTransferBounds(active_world_transfer_.total_bytes_expected,
                                         active_world_transfer_.chunk_size,
                                         message.transfer_chunk_index,
                                         message.transfer_chunk_data.size())) {
        spdlog::error("ClientConnection: world transfer chunk bounds mismatch transfer_id='{}' chunk_index={} chunk_bytes={} total_bytes={} chunk_size={}",
                      active_world_transfer_.transfer_id,
                      message.transfer_chunk_index,
                      message.transfer_chunk_data.size(),
                      active_world_transfer_.total_bytes_expected,
                      active_world_transfer_.chunk_size);
        should_exit_ = true;
        requestDisconnect();
        return;
    }

    const uint64_t chunk_offset_u64 =
        static_cast<uint64_t>(message.transfer_chunk_index) * active_world_transfer_.chunk_size;
    const size_t chunk_offset = static_cast<size_t>(chunk_offset_u64);
    if (message.transfer_chunk_index < active_world_transfer_.next_chunk_index) {
        if (!detail::ChunkMatchesBufferedPayload(active_world_transfer_.payload,
                                                 chunk_offset,
                                                 message.transfer_chunk_data)) {
            spdlog::error("ClientConnection: world transfer chunk retry mismatch transfer_id='{}' chunk_index={} buffered_bytes={} chunk_bytes={}",
                          active_world_transfer_.transfer_id,
                          message.transfer_chunk_index,
                          active_world_transfer_.payload.size(),
                          message.transfer_chunk_data.size());
            should_exit_ = true;
            requestDisconnect();
            return;
        }
        KARMA_TRACE("net.client",
                    "ClientConnection: world transfer retry chunk acknowledged transfer_id='{}' chunk_index={} buffered_chunks={} buffered_bytes={}",
                    active_world_transfer_.transfer_id,
                    message.transfer_chunk_index,
                    active_world_transfer_.next_chunk_index,
                    active_world_transfer_.payload.size());
        return;
    }
    if (message.transfer_chunk_index > active_world_transfer_.next_chunk_index) {
        spdlog::error("ClientConnection: world transfer chunk gap transfer_id='{}' expected={} got={}",
                      active_world_transfer_.transfer_id,
                      active_world_transfer_.next_chunk_index,
                      message.transfer_chunk_index);
        should_exit_ = true;
        requestDisconnect();
        return;
    }
    if (chunk_offset != active_world_transfer_.payload.size()) {
        spdlog::error("ClientConnection: world transfer chunk offset mismatch transfer_id='{}' chunk_index={} offset={} buffered_bytes={}",
                      active_world_transfer_.transfer_id,
                      message.transfer_chunk_index,
                      chunk_offset,
                      active_world_transfer_.payload.size());
        should_exit_ = true;
        requestDisconnect();
        return;
    }

    detail::HashBytesFNV1a(active_world_transfer_.payload_hash,
                           message.transfer_chunk_data.data(),
                           message.transfer_chunk_data.size());
    detail::HashChunkChainFNV1a(active_world_transfer_.chunk_chain_hash,
                                message.transfer_chunk_index,
                                message.transfer_chunk_data);
    active_world_transfer_.payload.insert(active_world_transfer_.payload.end(),
                                          message.transfer_chunk_data.begin(),
                                          message.transfer_chunk_data.end());
    ++active_world_transfer_.next_chunk_index;
}

void ClientConnection::handleWorldTransferEnd(const bz3::net::ServerMessage& message) {
    if (!pending_world_package_.active || !active_world_transfer_.active) {
        spdlog::error("ClientConnection: unexpected world transfer end transfer_id='{}' (pending_init={} active_transfer={})",
                      message.transfer_id,
                      pending_world_package_.active ? 1 : 0,
                      active_world_transfer_.active ? 1 : 0);
        should_exit_ = true;
        requestDisconnect();
        return;
    }
    if (message.transfer_id != active_world_transfer_.transfer_id) {
        spdlog::error("ClientConnection: world transfer end transfer_id mismatch expected='{}' got='{}'",
                      active_world_transfer_.transfer_id,
                      message.transfer_id);
        should_exit_ = true;
        requestDisconnect();
        return;
    }
    if (message.transfer_chunk_count != active_world_transfer_.next_chunk_index) {
        spdlog::error("ClientConnection: world transfer end chunk_count mismatch transfer_id='{}' expected={} got={}",
                      active_world_transfer_.transfer_id,
                      active_world_transfer_.next_chunk_index,
                      message.transfer_chunk_count);
        should_exit_ = true;
        requestDisconnect();
        return;
    }
    if (message.transfer_total_bytes != active_world_transfer_.payload.size()) {
        spdlog::error("ClientConnection: world transfer end size mismatch transfer_id='{}' expected={} got={}",
                      active_world_transfer_.transfer_id,
                      message.transfer_total_bytes,
                      active_world_transfer_.payload.size());
        should_exit_ = true;
        requestDisconnect();
        return;
    }
    if (!active_world_transfer_.transfer_world_hash.empty() &&
        message.transfer_world_hash != active_world_transfer_.transfer_world_hash) {
        spdlog::error("ClientConnection: world transfer end hash mismatch transfer_id='{}' expected='{}' got='{}'",
                      active_world_transfer_.transfer_id,
                      active_world_transfer_.transfer_world_hash,
                      message.transfer_world_hash);
        should_exit_ = true;
        requestDisconnect();
        return;
    }
    if (!active_world_transfer_.transfer_world_content_hash.empty() &&
        message.transfer_world_content_hash != active_world_transfer_.transfer_world_content_hash) {
        spdlog::error("ClientConnection: world transfer end content_hash mismatch transfer_id='{}' expected='{}' got='{}'",
                      active_world_transfer_.transfer_id,
                      active_world_transfer_.transfer_world_content_hash,
                      message.transfer_world_content_hash);
        should_exit_ = true;
        requestDisconnect();
        return;
    }

    const std::string streamed_payload_hash = detail::Hash64Hex(active_world_transfer_.payload_hash);
    if (!active_world_transfer_.is_delta && !active_world_transfer_.transfer_world_hash.empty() &&
        streamed_payload_hash != active_world_transfer_.transfer_world_hash) {
        spdlog::error("ClientConnection: world transfer payload hash mismatch transfer_id='{}' expected='{}' got='{}'",
                      active_world_transfer_.transfer_id,
                      active_world_transfer_.transfer_world_hash,
                      streamed_payload_hash);
        should_exit_ = true;
        requestDisconnect();
        return;
    }

    const std::string streamed_chunk_chain_hash =
        detail::Hash64Hex(active_world_transfer_.chunk_chain_hash);
    KARMA_TRACE("net.client",
                "ClientConnection: world transfer end transfer_id='{}' mode={} chunks={} bytes={} payload_hash='{}' chunk_chain='{}'",
                active_world_transfer_.transfer_id,
                active_world_transfer_.is_delta ? "delta" : "full",
                active_world_transfer_.next_chunk_index,
                active_world_transfer_.payload.size(),
                streamed_payload_hash,
                streamed_chunk_chain_hash);
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

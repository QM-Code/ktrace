#include "karma/network/content/transfer_receiver.hpp"

#include "karma/common/content/primitives.hpp"
#include "karma/common/logging.hpp"

#include <spdlog/spdlog.h>

#include <string>

namespace karma::network::content {

bool HandleTransferBegin(const TransferReceiverExpectation& expectation,
                         const TransferBeginPacket& packet,
                         TransferReceiverState* state,
                         std::string_view log_prefix,
                         TransferBeginOutcome* outcome) {
    if (outcome != nullptr) {
        *outcome = {};
    }
    if (state == nullptr) {
        return false;
    }

    if (!expectation.pending_world_package_active) {
        spdlog::error("{}: unexpected world transfer begin transfer_id='{}' (no pending world init)",
                      log_prefix,
                      packet.transfer_id);
        return false;
    }
    if (packet.transfer_id.empty()) {
        spdlog::error("{}: world transfer begin missing transfer_id", log_prefix);
        return false;
    }
    if (packet.transfer_world_id != expectation.world_id ||
        packet.transfer_world_revision != expectation.world_revision) {
        spdlog::error("{}: world transfer begin identity mismatch transfer_id='{}' begin_id='{}' begin_rev='{}' init_id='{}' init_rev='{}'",
                      log_prefix,
                      packet.transfer_id,
                      packet.transfer_world_id,
                      packet.transfer_world_revision,
                      expectation.world_id,
                      expectation.world_revision);
        return false;
    }
    if (packet.transfer_is_delta &&
        (packet.transfer_delta_base_world_id.empty() ||
         packet.transfer_delta_base_world_revision.empty())) {
        spdlog::error("{}: world delta transfer begin missing base identity transfer_id='{}' base_id='{}' base_rev='{}'",
                      log_prefix,
                      packet.transfer_id,
                      packet.transfer_delta_base_world_id,
                      packet.transfer_delta_base_world_revision);
        return false;
    }
    if (packet.transfer_total_bytes > 0 && packet.transfer_chunk_size == 0) {
        spdlog::error("{}: world transfer begin invalid chunk size transfer_id='{}' total_bytes={} chunk_size={}",
                      log_prefix,
                      packet.transfer_id,
                      packet.transfer_total_bytes,
                      packet.transfer_chunk_size);
        return false;
    }
    if (!expectation.world_hash.empty() &&
        packet.transfer_world_hash != expectation.world_hash) {
        spdlog::error("{}: world transfer begin hash mismatch transfer_id='{}' expected='{}' got='{}'",
                      log_prefix,
                      packet.transfer_id,
                      expectation.world_hash,
                      packet.transfer_world_hash);
        return false;
    }
    if (!expectation.world_content_hash.empty() &&
        packet.transfer_world_content_hash != expectation.world_content_hash) {
        spdlog::error("{}: world transfer begin content_hash mismatch transfer_id='{}' expected='{}' got='{}'",
                      log_prefix,
                      packet.transfer_id,
                      expectation.world_content_hash,
                      packet.transfer_world_content_hash);
        return false;
    }

    const std::string expected_transfer_world_hash = expectation.world_hash.empty()
        ? std::string(packet.transfer_world_hash)
        : std::string(expectation.world_hash);
    const std::string expected_transfer_world_content_hash =
        expectation.world_content_hash.empty()
        ? std::string(packet.transfer_world_content_hash)
        : std::string(expectation.world_content_hash);

    bool resumed_transfer = false;
    uint32_t resumed_chunks = 0;
    size_t resumed_bytes = 0;
    if (state->active) {
        const bool transfer_compatible =
            state->total_bytes_expected == packet.transfer_total_bytes &&
            state->chunk_size == packet.transfer_chunk_size &&
            state->is_delta == packet.transfer_is_delta &&
            state->delta_base_world_id == packet.transfer_delta_base_world_id &&
            state->delta_base_world_revision == packet.transfer_delta_base_world_revision &&
            state->delta_base_world_hash == packet.transfer_delta_base_world_hash &&
            state->delta_base_world_content_hash == packet.transfer_delta_base_world_content_hash &&
            state->transfer_world_hash == expected_transfer_world_hash &&
            state->transfer_world_content_hash == expected_transfer_world_content_hash &&
            state->payload.size() <= packet.transfer_total_bytes;
        if (!transfer_compatible) {
            KARMA_TRACE("net.client",
                        "{}: world transfer restart reset transfer_id='{}' previous_transfer_id='{}' previous_chunks={} previous_bytes={} total_bytes={} chunk_size={}",
                        log_prefix,
                        packet.transfer_id,
                        state->transfer_id,
                        state->next_chunk_index,
                        state->payload.size(),
                        packet.transfer_total_bytes,
                        packet.transfer_chunk_size);
            *state = {};
        } else {
            resumed_transfer = state->next_chunk_index > 0;
            resumed_chunks = state->next_chunk_index;
            resumed_bytes = state->payload.size();
        }
    }

    if (!state->active) {
        state->active = true;
        state->is_delta = packet.transfer_is_delta;
        state->delta_base_world_id = packet.transfer_delta_base_world_id;
        state->delta_base_world_revision = packet.transfer_delta_base_world_revision;
        state->delta_base_world_hash = packet.transfer_delta_base_world_hash;
        state->delta_base_world_content_hash = packet.transfer_delta_base_world_content_hash;
        state->transfer_world_hash = expected_transfer_world_hash;
        state->transfer_world_content_hash = expected_transfer_world_content_hash;
        state->total_bytes_expected = packet.transfer_total_bytes;
        state->chunk_size = packet.transfer_chunk_size;
        state->next_chunk_index = 0;
        state->payload_hash = kTransferFNV1aOffsetBasis64;
        state->chunk_chain_hash = kTransferFNV1aOffsetBasis64;
        state->payload.clear();
        if (packet.transfer_total_bytes > 0) {
            state->payload.reserve(static_cast<size_t>(packet.transfer_total_bytes));
        }
    }
    state->transfer_id = packet.transfer_id;

    KARMA_TRACE("net.client",
                "{}: world transfer begin transfer_id='{}' mode={} world='{}' id='{}' rev='{}' total_bytes={} chunk_size={} hash='{}' content_hash='{}' base_id='{}' base_rev='{}' resume={} resumed_chunks={} resumed_bytes={}",
                log_prefix,
                state->transfer_id,
                state->is_delta ? "delta" : "full",
                expectation.world_name,
                expectation.world_id,
                expectation.world_revision,
                state->total_bytes_expected,
                state->chunk_size,
                state->transfer_world_hash.empty() ? "-" : state->transfer_world_hash,
                state->transfer_world_content_hash.empty() ? "-" : state->transfer_world_content_hash,
                state->delta_base_world_id.empty() ? "-" : state->delta_base_world_id,
                state->delta_base_world_revision.empty() ? "-" : state->delta_base_world_revision,
                resumed_transfer ? 1 : 0,
                resumed_chunks,
                resumed_bytes);

    if (outcome != nullptr) {
        outcome->resumed_transfer = resumed_transfer;
        outcome->resumed_chunks = resumed_chunks;
        outcome->resumed_bytes = resumed_bytes;
    }
    return true;
}

bool HandleTransferChunk(const TransferChunkPacket& packet,
                         TransferReceiverState* state,
                         std::string_view log_prefix) {
    if (state == nullptr) {
        return false;
    }
    if (!state->active) {
        spdlog::error("{}: unexpected world transfer chunk transfer_id='{}' (no active transfer)",
                      log_prefix,
                      packet.transfer_id);
        return false;
    }
    if (packet.transfer_chunk_data == nullptr) {
        spdlog::error("{}: world transfer chunk payload missing transfer_id='{}' chunk_index={}",
                      log_prefix,
                      packet.transfer_id,
                      packet.transfer_chunk_index);
        return false;
    }
    const auto& chunk_data = *packet.transfer_chunk_data;
    if (packet.transfer_id != state->transfer_id) {
        spdlog::error("{}: world transfer chunk transfer_id mismatch expected='{}' got='{}'",
                      log_prefix,
                      state->transfer_id,
                      packet.transfer_id);
        return false;
    }
    if (!karma::content::IsChunkInTransferBounds(state->total_bytes_expected,
                                                 state->chunk_size,
                                                 packet.transfer_chunk_index,
                                                 chunk_data.size())) {
        spdlog::error("{}: world transfer chunk bounds mismatch transfer_id='{}' chunk_index={} chunk_bytes={} total_bytes={} chunk_size={}",
                      log_prefix,
                      state->transfer_id,
                      packet.transfer_chunk_index,
                      chunk_data.size(),
                      state->total_bytes_expected,
                      state->chunk_size);
        return false;
    }

    const uint64_t chunk_offset_u64 =
        static_cast<uint64_t>(packet.transfer_chunk_index) * state->chunk_size;
    const size_t chunk_offset = static_cast<size_t>(chunk_offset_u64);
    if (packet.transfer_chunk_index < state->next_chunk_index) {
        if (!karma::content::ChunkMatchesBufferedPayload(state->payload,
                                                         chunk_offset,
                                                         chunk_data)) {
            spdlog::error("{}: world transfer chunk retry mismatch transfer_id='{}' chunk_index={} buffered_bytes={} chunk_bytes={}",
                          log_prefix,
                          state->transfer_id,
                          packet.transfer_chunk_index,
                          state->payload.size(),
                          chunk_data.size());
            return false;
        }
        KARMA_TRACE("net.client",
                    "{}: world transfer retry chunk acknowledged transfer_id='{}' chunk_index={} buffered_chunks={} buffered_bytes={}",
                    log_prefix,
                    state->transfer_id,
                    packet.transfer_chunk_index,
                    state->next_chunk_index,
                    state->payload.size());
        return true;
    }
    if (packet.transfer_chunk_index > state->next_chunk_index) {
        spdlog::error("{}: world transfer chunk gap transfer_id='{}' expected={} got={}",
                      log_prefix,
                      state->transfer_id,
                      state->next_chunk_index,
                      packet.transfer_chunk_index);
        return false;
    }
    if (chunk_offset != state->payload.size()) {
        spdlog::error("{}: world transfer chunk offset mismatch transfer_id='{}' chunk_index={} offset={} buffered_bytes={}",
                      log_prefix,
                      state->transfer_id,
                      packet.transfer_chunk_index,
                      chunk_offset,
                      state->payload.size());
        return false;
    }

    karma::content::HashBytesFNV1a(state->payload_hash, chunk_data.data(), chunk_data.size());
    karma::content::HashChunkChainFNV1a(state->chunk_chain_hash,
                                        packet.transfer_chunk_index,
                                        chunk_data);
    state->payload.insert(state->payload.end(), chunk_data.begin(), chunk_data.end());
    ++state->next_chunk_index;
    return true;
}

bool HandleTransferEnd(bool pending_world_package_active,
                       const TransferEndPacket& packet,
                       TransferReceiverState* state,
                       std::string_view log_prefix,
                       TransferEndOutcome* outcome) {
    if (outcome != nullptr) {
        *outcome = {};
    }
    if (state == nullptr) {
        return false;
    }
    if (!pending_world_package_active || !state->active) {
        spdlog::error("{}: unexpected world transfer end transfer_id='{}' (pending_init={} active_transfer={})",
                      log_prefix,
                      packet.transfer_id,
                      pending_world_package_active ? 1 : 0,
                      state->active ? 1 : 0);
        return false;
    }
    if (packet.transfer_id != state->transfer_id) {
        spdlog::error("{}: world transfer end transfer_id mismatch expected='{}' got='{}'",
                      log_prefix,
                      state->transfer_id,
                      packet.transfer_id);
        return false;
    }
    if (packet.transfer_chunk_count != state->next_chunk_index) {
        spdlog::error("{}: world transfer end chunk_count mismatch transfer_id='{}' expected={} got={}",
                      log_prefix,
                      state->transfer_id,
                      state->next_chunk_index,
                      packet.transfer_chunk_count);
        return false;
    }
    if (packet.transfer_total_bytes != state->payload.size()) {
        spdlog::error("{}: world transfer end size mismatch transfer_id='{}' expected={} got={}",
                      log_prefix,
                      state->transfer_id,
                      packet.transfer_total_bytes,
                      state->payload.size());
        return false;
    }
    if (!state->transfer_world_hash.empty() &&
        packet.transfer_world_hash != state->transfer_world_hash) {
        spdlog::error("{}: world transfer end hash mismatch transfer_id='{}' expected='{}' got='{}'",
                      log_prefix,
                      state->transfer_id,
                      state->transfer_world_hash,
                      packet.transfer_world_hash);
        return false;
    }
    if (!state->transfer_world_content_hash.empty() &&
        packet.transfer_world_content_hash != state->transfer_world_content_hash) {
        spdlog::error("{}: world transfer end content_hash mismatch transfer_id='{}' expected='{}' got='{}'",
                      log_prefix,
                      state->transfer_id,
                      state->transfer_world_content_hash,
                      packet.transfer_world_content_hash);
        return false;
    }

    const std::string streamed_payload_hash = karma::content::Hash64Hex(state->payload_hash);
    if (!state->is_delta && !state->transfer_world_hash.empty() &&
        streamed_payload_hash != state->transfer_world_hash) {
        spdlog::error("{}: world transfer payload hash mismatch transfer_id='{}' expected='{}' got='{}'",
                      log_prefix,
                      state->transfer_id,
                      state->transfer_world_hash,
                      streamed_payload_hash);
        return false;
    }

    const std::string streamed_chunk_chain_hash =
        karma::content::Hash64Hex(state->chunk_chain_hash);
    KARMA_TRACE("net.client",
                "{}: world transfer end transfer_id='{}' mode={} chunks={} bytes={} payload_hash='{}' chunk_chain='{}'",
                log_prefix,
                state->transfer_id,
                state->is_delta ? "delta" : "full",
                state->next_chunk_index,
                state->payload.size(),
                streamed_payload_hash,
                streamed_chunk_chain_hash);

    if (outcome != nullptr) {
        outcome->streamed_payload_hash = streamed_payload_hash;
        outcome->streamed_chunk_chain_hash = streamed_chunk_chain_hash;
    }
    return true;
}

} // namespace karma::network::content

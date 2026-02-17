#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace karma::network::content {

inline constexpr uint64_t kTransferFNV1aOffsetBasis64 = 14695981039346656037ULL;

struct TransferReceiverExpectation {
    bool pending_world_package_active = false;
    std::string_view world_name{};
    std::string_view world_id{};
    std::string_view world_revision{};
    std::string_view world_hash{};
    std::string_view world_content_hash{};
};

struct TransferBeginPacket {
    std::string_view transfer_id{};
    std::string_view transfer_world_id{};
    std::string_view transfer_world_revision{};
    uint64_t transfer_total_bytes = 0;
    uint32_t transfer_chunk_size = 0;
    std::string_view transfer_world_hash{};
    std::string_view transfer_world_content_hash{};
    bool transfer_is_delta = false;
    std::string_view transfer_delta_base_world_id{};
    std::string_view transfer_delta_base_world_revision{};
    std::string_view transfer_delta_base_world_hash{};
    std::string_view transfer_delta_base_world_content_hash{};
};

struct TransferChunkPacket {
    std::string_view transfer_id{};
    uint32_t transfer_chunk_index = 0;
    const std::vector<std::byte>* transfer_chunk_data = nullptr;
};

struct TransferEndPacket {
    std::string_view transfer_id{};
    uint32_t transfer_chunk_count = 0;
    uint64_t transfer_total_bytes = 0;
    std::string_view transfer_world_hash{};
    std::string_view transfer_world_content_hash{};
};

struct TransferBeginOutcome {
    bool resumed_transfer = false;
    uint32_t resumed_chunks = 0;
    size_t resumed_bytes = 0;
};

struct TransferEndOutcome {
    std::string streamed_payload_hash{};
    std::string streamed_chunk_chain_hash{};
};

struct TransferReceiverState {
    bool active = false;
    std::string transfer_id{};
    std::string transfer_world_hash{};
    std::string transfer_world_content_hash{};
    bool is_delta = false;
    std::string delta_base_world_id{};
    std::string delta_base_world_revision{};
    std::string delta_base_world_hash{};
    std::string delta_base_world_content_hash{};
    uint64_t total_bytes_expected = 0;
    uint32_t chunk_size = 0;
    uint32_t next_chunk_index = 0;
    uint64_t payload_hash = kTransferFNV1aOffsetBasis64;
    uint64_t chunk_chain_hash = kTransferFNV1aOffsetBasis64;
    std::vector<std::byte> payload{};
};

bool HandleTransferBegin(const TransferReceiverExpectation& expectation,
                         const TransferBeginPacket& packet,
                         TransferReceiverState* state,
                         std::string_view log_prefix,
                         TransferBeginOutcome* outcome = nullptr);

bool HandleTransferChunk(const TransferChunkPacket& packet,
                         TransferReceiverState* state,
                         std::string_view log_prefix);

bool HandleTransferEnd(bool pending_world_package_active,
                       const TransferEndPacket& packet,
                       TransferReceiverState* state,
                       std::string_view log_prefix,
                       TransferEndOutcome* outcome = nullptr);

} // namespace karma::network::content

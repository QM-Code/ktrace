#include "net/protocol_codec.hpp"

#include "messages.pb.h"
#include "net/protocol_codec/wire_common.hpp"

namespace bz3::net {

std::vector<std::byte> EncodeServerWorldTransferBegin(std::string_view transfer_id,
                                                      std::string_view world_id,
                                                      std::string_view world_revision,
                                                      uint64_t total_bytes,
                                                      uint32_t chunk_size,
                                                      std::string_view world_hash,
                                                      std::string_view world_content_hash,
                                                      bool is_delta,
                                                      std::string_view delta_base_world_id,
                                                      std::string_view delta_base_world_revision,
                                                      std::string_view delta_base_world_hash,
                                                      std::string_view delta_base_world_content_hash) {
    karma::ServerMsg message{};
    message.set_type(karma::ServerMsg::WORLD_TRANSFER_BEGIN);
    auto* begin = message.mutable_world_transfer_begin();
    begin->set_transfer_id(std::string(transfer_id));
    begin->set_world_id(std::string(world_id));
    begin->set_world_revision(std::string(world_revision));
    begin->set_total_bytes(total_bytes);
    begin->set_chunk_size(chunk_size);
    if (!world_hash.empty()) {
        begin->set_world_hash(std::string(world_hash));
    }
    if (!world_content_hash.empty()) {
        begin->set_world_content_hash(std::string(world_content_hash));
    }
    begin->set_is_delta(is_delta);
    if (!delta_base_world_id.empty()) {
        begin->set_delta_base_world_id(std::string(delta_base_world_id));
    }
    if (!delta_base_world_revision.empty()) {
        begin->set_delta_base_world_revision(std::string(delta_base_world_revision));
    }
    if (!delta_base_world_hash.empty()) {
        begin->set_delta_base_world_hash(std::string(delta_base_world_hash));
    }
    if (!delta_base_world_content_hash.empty()) {
        begin->set_delta_base_world_content_hash(std::string(delta_base_world_content_hash));
    }
    return detail::SerializeOrEmpty(message);
}

std::vector<std::byte> EncodeServerWorldTransferChunk(std::string_view transfer_id,
                                                      uint32_t chunk_index,
                                                      const std::vector<std::byte>& chunk_data) {
    karma::ServerMsg message{};
    message.set_type(karma::ServerMsg::WORLD_TRANSFER_CHUNK);
    auto* chunk = message.mutable_world_transfer_chunk();
    chunk->set_transfer_id(std::string(transfer_id));
    chunk->set_chunk_index(chunk_index);
    if (!chunk_data.empty()) {
        chunk->set_chunk_data(reinterpret_cast<const char*>(chunk_data.data()),
                              static_cast<int>(chunk_data.size()));
    }
    return detail::SerializeOrEmpty(message);
}

std::vector<std::byte> EncodeServerWorldTransferEnd(std::string_view transfer_id,
                                                    uint32_t chunk_count,
                                                    uint64_t total_bytes,
                                                    std::string_view world_hash,
                                                    std::string_view world_content_hash) {
    karma::ServerMsg message{};
    message.set_type(karma::ServerMsg::WORLD_TRANSFER_END);
    auto* end = message.mutable_world_transfer_end();
    end->set_transfer_id(std::string(transfer_id));
    end->set_chunk_count(chunk_count);
    end->set_total_bytes(total_bytes);
    if (!world_hash.empty()) {
        end->set_world_hash(std::string(world_hash));
    }
    if (!world_content_hash.empty()) {
        end->set_world_content_hash(std::string(world_content_hash));
    }
    return detail::SerializeOrEmpty(message);
}

} // namespace bz3::net

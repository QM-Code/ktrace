#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace karma::network::content {

struct TransferSenderRequest {
    uint32_t client_id = 0;
    std::string transfer_id{};
    std::string world_id{};
    std::string world_revision{};
    std::string world_hash{};
    std::string world_content_hash{};
    bool is_delta = false;
    std::string delta_base_world_id{};
    std::string delta_base_world_revision{};
    std::string delta_base_world_hash{};
    std::string delta_base_world_content_hash{};
    const std::vector<std::byte>* world_package = nullptr;
    uint32_t chunk_size = 1;
    uint32_t max_retry_attempts = 0;
};

struct TransferSenderCallbacks {
    std::function<bool(std::string_view transfer_id,
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
                       std::string_view delta_base_world_content_hash)>
        send_begin{};
    std::function<bool(std::string_view transfer_id,
                       uint32_t chunk_index,
                       const std::vector<std::byte>& chunk_data)>
        send_chunk{};
    std::function<bool(std::string_view transfer_id,
                       uint32_t chunk_count,
                       uint64_t total_bytes,
                       std::string_view world_hash,
                       std::string_view world_content_hash)>
        send_end{};
};

bool SendWorldPackageChunked(const TransferSenderRequest& request,
                             const TransferSenderCallbacks& callbacks,
                             std::string_view log_prefix);

} // namespace karma::network::content

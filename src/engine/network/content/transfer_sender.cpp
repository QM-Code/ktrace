#include "karma/network/content/transfer_sender.hpp"

#include "karma/common/config_helpers.hpp"
#include "karma/common/logging.hpp"

#include <algorithm>
#include <cstddef>

namespace karma::network::content {

TransferSenderRuntimeConfig ReadTransferSenderRuntimeConfig(uint16_t default_chunk_size,
                                                            uint16_t default_retry_attempts) {
    TransferSenderRuntimeConfig runtime_config{};
    runtime_config.chunk_size = std::max<uint32_t>(
        1,
        static_cast<uint32_t>(karma::config::ReadUInt16Config({"network.WorldTransferChunkBytes"},
                                                               default_chunk_size)));
    runtime_config.max_retry_attempts = static_cast<uint32_t>(
        karma::config::ReadUInt16Config({"network.WorldTransferRetryAttempts"},
                                        default_retry_attempts));
    return runtime_config;
}

std::string BuildTransferId(uint32_t client_id, uint64_t transfer_sequence) {
    return std::to_string(client_id) + "-" + std::to_string(transfer_sequence);
}

TransferSenderRequest BuildTransferSenderRequest(
    uint32_t client_id,
    std::string_view transfer_id,
    std::string_view world_id,
    std::string_view world_revision,
    std::string_view world_hash,
    std::string_view world_content_hash,
    bool is_delta,
    std::string_view delta_base_world_id,
    std::string_view delta_base_world_revision,
    std::string_view delta_base_world_hash,
    std::string_view delta_base_world_content_hash,
    const std::vector<std::byte>& world_package,
    const TransferSenderRuntimeConfig& runtime_config) {
    TransferSenderRequest request{};
    request.client_id = client_id;
    request.transfer_id = std::string(transfer_id);
    request.world_id = std::string(world_id);
    request.world_revision = std::string(world_revision);
    request.world_hash = std::string(world_hash);
    request.world_content_hash = std::string(world_content_hash);
    request.is_delta = is_delta;
    request.delta_base_world_id = std::string(delta_base_world_id);
    request.delta_base_world_revision = std::string(delta_base_world_revision);
    request.delta_base_world_hash = std::string(delta_base_world_hash);
    request.delta_base_world_content_hash = std::string(delta_base_world_content_hash);
    request.world_package = &world_package;
    request.chunk_size = std::max<uint32_t>(1, runtime_config.chunk_size);
    request.max_retry_attempts = runtime_config.max_retry_attempts;
    return request;
}

bool SendWorldPackageChunked(const TransferSenderRequest& request,
                             const TransferSenderCallbacks& callbacks,
                             std::string_view log_prefix) {
    if (request.world_package == nullptr || request.world_package->empty()) {
        return true;
    }
    if (!callbacks.send_begin || !callbacks.send_chunk || !callbacks.send_end) {
        return false;
    }

    const auto& world_package = *request.world_package;
    const uint32_t chunk_size = std::max<uint32_t>(1, request.chunk_size);
    const uint32_t total_chunk_count =
        static_cast<uint32_t>((world_package.size() + chunk_size - 1) / chunk_size);

    size_t next_offset = 0;
    uint32_t next_chunk_index = 0;
    for (uint32_t attempt = 0; attempt <= request.max_retry_attempts; ++attempt) {
        const uint32_t resume_chunk_index = next_chunk_index;
        const size_t resume_offset = next_offset;
        if (!callbacks.send_begin(request.transfer_id,
                                  request.world_id,
                                  request.world_revision,
                                  world_package.size(),
                                  chunk_size,
                                  request.world_hash,
                                  request.world_content_hash,
                                  request.is_delta,
                                  request.delta_base_world_id,
                                  request.delta_base_world_revision,
                                  request.delta_base_world_hash,
                                  request.delta_base_world_content_hash)) {
            KARMA_TRACE("net.server",
                        "{}: world transfer begin send failed client_id={} transfer_id='{}' attempt={}/{} resume_chunk={}",
                        log_prefix,
                        request.client_id,
                        request.transfer_id,
                        attempt + 1,
                        request.max_retry_attempts + 1,
                        resume_chunk_index);
            if (attempt == request.max_retry_attempts) {
                return false;
            }
            continue;
        }

        bool chunk_send_failed = false;
        while (next_offset < world_package.size()) {
            const size_t remaining = world_package.size() - next_offset;
            const size_t this_chunk_size = std::min<size_t>(remaining, chunk_size);
            std::vector<std::byte> chunk{};
            chunk.insert(chunk.end(),
                         world_package.begin() + static_cast<std::ptrdiff_t>(next_offset),
                         world_package.begin() + static_cast<std::ptrdiff_t>(next_offset + this_chunk_size));
            if (!callbacks.send_chunk(request.transfer_id, next_chunk_index, chunk)) {
                chunk_send_failed = true;
                KARMA_TRACE("net.server",
                            "{}: world transfer chunk send failed client_id={} transfer_id='{}' attempt={}/{} chunk_index={} chunk_bytes={}",
                            log_prefix,
                            request.client_id,
                            request.transfer_id,
                            attempt + 1,
                            request.max_retry_attempts + 1,
                            next_chunk_index,
                            chunk.size());
                break;
            }
            next_offset += this_chunk_size;
            ++next_chunk_index;
        }

        if (chunk_send_failed) {
            if (attempt == request.max_retry_attempts) {
                return false;
            }
            KARMA_TRACE("net.server",
                        "{}: world transfer retry scheduled client_id={} transfer_id='{}' next_chunk={} sent_chunks={} bytes_sent={} attempt={}/{}",
                        log_prefix,
                        request.client_id,
                        request.transfer_id,
                        next_chunk_index,
                        next_chunk_index - resume_chunk_index,
                        next_offset - resume_offset,
                        attempt + 1,
                        request.max_retry_attempts + 1);
            continue;
        }

        if (!callbacks.send_end(request.transfer_id,
                                total_chunk_count,
                                world_package.size(),
                                request.world_hash,
                                request.world_content_hash)) {
            KARMA_TRACE("net.server",
                        "{}: world transfer end send failed client_id={} transfer_id='{}' attempt={}/{} total_chunks={}",
                        log_prefix,
                        request.client_id,
                        request.transfer_id,
                        attempt + 1,
                        request.max_retry_attempts + 1,
                        total_chunk_count);
            if (attempt == request.max_retry_attempts) {
                return false;
            }
            continue;
        }

        KARMA_TRACE("net.server",
                    "{}: world transfer sent client_id={} transfer_id='{}' mode={} chunks={} bytes={} chunk_size={} retries={} base_id='{}' base_rev='{}'",
                    log_prefix,
                    request.client_id,
                    request.transfer_id,
                    request.is_delta ? "delta" : "full",
                    total_chunk_count,
                    world_package.size(),
                    chunk_size,
                    attempt,
                    request.delta_base_world_id.empty() ? "-" : request.delta_base_world_id,
                    request.delta_base_world_revision.empty() ? "-" : request.delta_base_world_revision);
        return true;
    }

    return false;
}

} // namespace karma::network::content

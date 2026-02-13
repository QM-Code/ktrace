#include "server/heartbeat_client.hpp"

#include <curl/curl.h>

#include "karma/common/curl_global.hpp"
#include "karma/common/json.hpp"
#include "karma/common/logging.hpp"

#include <spdlog/spdlog.h>

#include <string>

namespace bz3::server {

namespace {

size_t AppendResponse(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* buffer = static_cast<std::string*>(userdata);
    const size_t total = size * nmemb;
    buffer->append(ptr, total);
    return total;
}

std::string TrimTrailingSlash(std::string value) {
    while (!value.empty() && value.back() == '/') {
        value.pop_back();
    }
    return value;
}

std::string UrlEncode(CURL* curl_handle, const std::string& value) {
    char* escaped = curl_easy_escape(curl_handle, value.c_str(), static_cast<int>(value.size()));
    if (!escaped) {
        return {};
    }
    std::string encoded(escaped);
    curl_free(escaped);
    return encoded;
}

bool PerformPost(const std::string& url,
                 const std::string& post_body,
                 long* status_out,
                 std::string* error_out,
                 std::string* body_out) {
    if (!status_out || !error_out || !body_out) {
        return false;
    }
    if (!karma::net::EnsureCurlGlobalInit()) {
        *error_out = "curl global init failed";
        return false;
    }

    CURL* curl_handle = curl_easy_init();
    if (!curl_handle) {
        *error_out = "curl_easy_init failed";
        return false;
    }

    char error_buffer[CURL_ERROR_SIZE];
    error_buffer[0] = '\0';

    curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, error_buffer);
    curl_easy_setopt(curl_handle, CURLOPT_POST, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, post_body.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, static_cast<long>(post_body.size()));
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, AppendResponse);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, body_out);

    const CURLcode result = curl_easy_perform(curl_handle);
    long status = 0;
    if (result == CURLE_OK) {
        curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &status);
    }
    curl_easy_cleanup(curl_handle);

    *status_out = status;
    if (result != CURLE_OK) {
        if (error_buffer[0] != '\0') {
            *error_out = error_buffer;
        } else {
            *error_out = curl_easy_strerror(result);
        }
        return false;
    }
    if (status < 200 || status >= 300) {
        if (!body_out->empty()) {
            try {
                const auto json_data = karma::json::Parse(*body_out);
                if (json_data.contains("message") && json_data["message"].is_string()) {
                    *error_out = json_data["message"].get<std::string>();
                }
            } catch (...) {
            }
        }
        return false;
    }
    return true;
}

} // namespace

HeartbeatClient::HeartbeatClient() = default;

HeartbeatClient::~HeartbeatClient() {
    stopWorker();
}

void HeartbeatClient::requestHeartbeat(const std::string& community_url,
                                       const std::string& server_address,
                                       int players,
                                       int max_players) {
    if (community_url.empty() || server_address.empty()) {
        return;
    }

    Request request{};
    request.community_url = community_url;
    request.server_address = server_address;
    request.players = players;
    request.max_players = max_players;

    startWorker();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        requests_.clear();
        requests_.push_back(std::move(request));
    }
    cv_.notify_one();
}

void HeartbeatClient::startWorker() {
    if (worker_.joinable()) {
        return;
    }
    stop_requested_ = false;
    worker_ = std::thread(&HeartbeatClient::workerProc, this);
}

void HeartbeatClient::stopWorker() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_requested_ = true;
        requests_.clear();
    }
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void HeartbeatClient::workerProc() {
    while (true) {
        Request request{};
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [&]() { return stop_requested_ || !requests_.empty(); });
            if (stop_requested_) {
                return;
            }
            request = std::move(requests_.front());
            requests_.pop_front();
        }

        if (!karma::net::EnsureCurlGlobalInit()) {
            spdlog::warn("HeartbeatClient: failed to initialize curl");
            continue;
        }

        CURL* curl_handle = curl_easy_init();
        if (!curl_handle) {
            spdlog::warn("HeartbeatClient: curl_easy_init failed");
            continue;
        }

        const std::string base_url = TrimTrailingSlash(request.community_url);
        const std::string encoded_server = UrlEncode(curl_handle, request.server_address);
        const std::string encoded_players = UrlEncode(curl_handle, std::to_string(request.players));
        const std::string encoded_max_players = UrlEncode(curl_handle, std::to_string(request.max_players));
        curl_easy_cleanup(curl_handle);

        if (encoded_server.empty()) {
            spdlog::warn("HeartbeatClient: failed to encode server address");
            continue;
        }

        const std::string url = base_url + "/api/heartbeat";
        const std::string body = "server=" + encoded_server
            + "&players=" + encoded_players
            + "&max=" + encoded_max_players;

        long status = 0;
        std::string error{};
        std::string response_body{};
        if (!PerformPost(url, body, &status, &error, &response_body)) {
            std::string reason{};
            if (!error.empty()) {
                reason = error;
            }
            if (status > 0 && (status < 200 || status >= 300)) {
                if (!reason.empty()) {
                    reason += ", ";
                }
                reason += "http_status=" + std::to_string(status);
            }
            if (reason.empty()) {
                reason = "request failed";
            }
            spdlog::warn("HeartbeatClient: failed to send heartbeat to {}: {}", base_url, reason);
        } else {
            KARMA_TRACE("net.server", "HeartbeatClient: sent heartbeat to {}", base_url);
        }
    }
}

} // namespace bz3::server

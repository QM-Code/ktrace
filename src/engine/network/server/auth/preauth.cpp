#include "karma/network/server/auth/preauth.hpp"

#include <curl/curl.h>

#include "karma/common/config/helpers.hpp"
#include "karma/common/serialization/json.hpp"
#include "karma/common/logging/logging.hpp"
#include "karma/network/auth/structured_payload.hpp"
#include "karma/network/http/curl_global.hpp"

#include <string>

namespace karma::network {

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

std::string NormalizeCommunityUrl(std::string community_url) {
    community_url = TrimTrailingSlash(std::move(community_url));
    if (community_url.empty()) {
        return community_url;
    }
    if (community_url.rfind("http://", 0) != 0
        && community_url.rfind("https://", 0) != 0) {
        community_url = "http://" + community_url;
    }
    return community_url;
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
    if (!karma::network::http::EnsureCurlGlobalInit()) {
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
                const auto json_data = karma::common::serialization::Parse(*body_out);
                if (json_data.contains("message") && json_data["message"].is_string()) {
                    *error_out = json_data["message"].get<std::string>();
                } else if (json_data.contains("error") && json_data["error"].is_string()) {
                    *error_out = json_data["error"].get<std::string>();
                }
            } catch (...) {
            }
        }
        return false;
    }
    return true;
}

bool EvaluateCommunityAuth(const ServerPreAuthConfig& config,
                           const ServerPreAuthRequest& request,
                           ServerPreAuthDecision* decision_out) {
    if (!decision_out) {
        return false;
    }

    const std::string community_url = NormalizeCommunityUrl(config.community_url);
    if (community_url.empty()) {
        return false;
    }

    auth::StructuredAuthPayload payload = auth::ParseStructuredAuthPayload(request.auth_payload);
    if (payload.username.empty()) {
        payload.username = std::string(request.player_name);
    }
    if (payload.username.empty()) {
        decision_out->accepted = false;
        decision_out->reject_reason = config.reject_reason;
        KARMA_TRACE("engine.server",
                    "Community pre-auth rejected client_id={} (missing username)",
                    request.client_id);
        return true;
    }

    const bool has_passhash = !payload.passhash.empty();
    const bool has_password = !payload.password.empty();
    if (!has_passhash && !has_password) {
        const std::string raw_payload(request.auth_payload);
        if (!raw_payload.empty()) {
            payload.password = raw_payload;
        }
    }

    if (payload.passhash.empty() && payload.password.empty()) {
        decision_out->accepted = false;
        decision_out->reject_reason = config.reject_reason;
        KARMA_TRACE("engine.server",
                    "Community pre-auth rejected client_id={} username='{}' (missing credential payload)",
                    request.client_id,
                    payload.username);
        return true;
    }

    if (!karma::network::http::EnsureCurlGlobalInit()) {
        decision_out->accepted = false;
        decision_out->reject_reason = config.reject_reason;
        KARMA_TRACE("engine.server",
                    "Community pre-auth request failed for client_id={} username='{}': curl init failed",
                    request.client_id,
                    payload.username);
        return true;
    }

    CURL* curl_handle = curl_easy_init();
    if (!curl_handle) {
        decision_out->accepted = false;
        decision_out->reject_reason = config.reject_reason;
        KARMA_TRACE("engine.server",
                    "Community pre-auth request failed for client_id={} username='{}': curl_easy_init failed",
                    request.client_id,
                    payload.username);
        return true;
    }

    const std::string encoded_username = UrlEncode(curl_handle, payload.username);
    const std::string encoded_passhash = UrlEncode(curl_handle, payload.passhash);
    const std::string encoded_password = UrlEncode(curl_handle, payload.password);
    const std::string encoded_world = UrlEncode(curl_handle, config.world_name);
    curl_easy_cleanup(curl_handle);

    if (encoded_username.empty()
        || (payload.passhash.empty() && encoded_password.empty())
        || (!payload.passhash.empty() && encoded_passhash.empty())) {
        decision_out->accepted = false;
        decision_out->reject_reason = config.reject_reason;
        KARMA_TRACE("engine.server",
                    "Community pre-auth request failed for client_id={} username='{}': URL encoding failed",
                    request.client_id,
                    payload.username);
        return true;
    }

    std::string body = "username=" + encoded_username;
    if (!payload.passhash.empty()) {
        body += "&passhash=" + encoded_passhash;
    } else {
        body += "&password=" + encoded_password;
    }
    if (!config.world_name.empty() && !encoded_world.empty()) {
        body += "&world=" + encoded_world;
    }

    const std::string auth_url = community_url + "/api/auth";
    long status = 0;
    std::string error{};
    std::string response_body{};
    if (!PerformPost(auth_url, body, &status, &error, &response_body)) {
        decision_out->accepted = false;
        decision_out->reject_reason = config.reject_reason;
        KARMA_TRACE("engine.server",
                    "Community pre-auth request failed for client_id={} username='{}': status={} error='{}'",
                    request.client_id,
                    payload.username,
                    status,
                    error);
        return true;
    }

    bool ok = false;
    std::string remote_error{};
    try {
        const auto response_json = karma::common::serialization::Parse(response_body);
        ok = response_json.value("ok", false);
        remote_error = response_json.value("error", std::string{});
    } catch (...) {
        decision_out->accepted = false;
        decision_out->reject_reason = config.reject_reason;
        KARMA_TRACE("engine.server",
                    "Community pre-auth request failed for client_id={} username='{}': invalid JSON response",
                    request.client_id,
                    payload.username);
        return true;
    }

    if (!ok) {
        decision_out->accepted = false;
        decision_out->reject_reason = config.reject_reason;
        KARMA_TRACE("engine.server",
                    "Community pre-auth rejected client_id={} username='{}' remote_error='{}'",
                    request.client_id,
                    payload.username,
                    remote_error);
        return true;
    }

    decision_out->accepted = true;
    decision_out->reject_reason.clear();
    KARMA_TRACE("engine.server",
                "Community pre-auth accepted client_id={} username='{}'",
                request.client_id,
                payload.username);
    return true;
}

} // namespace

ServerPreAuthConfig ReadServerPreAuthConfig() {
    ServerPreAuthConfig config{};
    config.required_password = karma::common::config::ReadStringConfig({"network.PreAuthPassword"}, std::string{});
    config.reject_reason = karma::common::config::ReadStringConfig({"network.PreAuthRejectReason"},
                                                           std::string("Authentication failed."));
    if (config.reject_reason.empty()) {
        config.reject_reason = "Authentication failed.";
    }
    return config;
}

ServerPreAuthDecision EvaluateServerPreAuth(const ServerPreAuthConfig& config,
                                            const ServerPreAuthRequest& request) {
    (void)request.peer_ip;
    (void)request.peer_port;

    ServerPreAuthDecision decision{};
    if (EvaluateCommunityAuth(config, request, &decision)) {
        return decision;
    }

    if (config.required_password.empty()) {
        decision.accepted = true;
        return decision;
    }

    if (request.auth_payload == config.required_password) {
        decision.accepted = true;
        return decision;
    }

    const auth::StructuredAuthPayload structured = auth::ParseStructuredAuthPayload(request.auth_payload);
    if (!structured.password.empty() && structured.password == config.required_password) {
        decision.accepted = true;
        return decision;
    }

    decision.accepted = false;
    decision.reject_reason = config.reject_reason;
    return decision;
}

} // namespace karma::network

#include "client/community_server_list.hpp"

#include "karma/common/curl_global.hpp"
#include "karma/common/json.hpp"

#include <curl/curl.h>

#include <cctype>
#include <cstdio>
#include <exception>
#include <string>
#include <utility>

namespace bz3::client {

namespace {

size_t CurlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    if (!userdata) {
        return 0;
    }
    auto* buffer = static_cast<std::string*>(userdata);
    const size_t total = size * nmemb;
    buffer->append(ptr, total);
    return total;
}

std::string TrimWhitespace(const std::string& input) {
    size_t start = 0;
    while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start])) != 0) {
        ++start;
    }
    size_t end = input.size();
    while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])) != 0) {
        --end;
    }
    return input.substr(start, end - start);
}

void TrimTrailingSlashes(std::string* text) {
    if (!text) {
        return;
    }
    while (!text->empty() && text->back() == '/') {
        text->pop_back();
    }
}

bool EndsWith(const std::string& value, const std::string& suffix) {
    if (suffix.size() > value.size()) {
        return false;
    }
    return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string BuildActiveServersUrl(const std::string& endpoint) {
    std::string normalized = TrimWhitespace(endpoint);
    if (normalized.empty()) {
        return {};
    }
    if (normalized.rfind("http://", 0) != 0 && normalized.rfind("https://", 0) != 0) {
        normalized = "http://" + normalized;
    }
    TrimTrailingSlashes(&normalized);
    if (EndsWith(normalized, "/api/servers/active")) {
        return normalized;
    }
    return normalized + "/api/servers/active";
}

bool ParseIntField(const karma::json::Value& object, const char* key, int* out) {
    if (!out) {
        return false;
    }
    const auto it = object.find(key);
    if (it == object.end()) {
        return false;
    }
    try {
        if (it->is_number_integer()) {
            *out = it->get<int>();
            return true;
        }
        if (it->is_number_unsigned()) {
            *out = static_cast<int>(it->get<unsigned int>());
            return true;
        }
        if (it->is_string()) {
            *out = std::stoi(it->get<std::string>());
            return true;
        }
    } catch (...) {
        return false;
    }
    return false;
}

bool FetchUrl(const std::string& url,
              std::string* body_out,
              std::string* error_out) {
    if (!body_out || !error_out) {
        return false;
    }
    body_out->clear();
    error_out->clear();

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
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, body_out);

    const CURLcode result = curl_easy_perform(curl_handle);
    long status = 0;
    if (result == CURLE_OK) {
        curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &status);
    }
    curl_easy_cleanup(curl_handle);

    if (result != CURLE_OK) {
        if (error_buffer[0] != '\0') {
            *error_out = error_buffer;
        } else {
            *error_out = curl_easy_strerror(result);
        }
        return false;
    }
    if (status < 200 || status >= 300) {
        *error_out = "http_status=" + std::to_string(status);
        return false;
    }
    return true;
}

} // namespace

bool FetchCommunityActiveList(const std::string& endpoint,
                              CommunityActiveList* out,
                              std::string* error_out) {
    if (!out || !error_out) {
        return false;
    }
    *out = CommunityActiveList{};
    error_out->clear();

    const std::string url = BuildActiveServersUrl(endpoint);
    if (url.empty()) {
        *error_out = "empty endpoint";
        return false;
    }

    std::string response_body{};
    if (!FetchUrl(url, &response_body, error_out)) {
        return false;
    }

    karma::json::Value json_data;
    try {
        json_data = karma::json::Parse(response_body);
    } catch (const std::exception& ex) {
        *error_out = std::string("invalid JSON response: ") + ex.what();
        return false;
    }
    if (!json_data.is_object()) {
        *error_out = "invalid JSON response: root is not an object";
        return false;
    }

    if (const auto it = json_data.find("community_name");
        it != json_data.end() && it->is_string()) {
        out->community_name = it->get<std::string>();
    }
    ParseIntField(json_data, "active_count", &out->active_count);
    ParseIntField(json_data, "inactive_count", &out->inactive_count);

    const auto servers_it = json_data.find("servers");
    if (servers_it == json_data.end() || !servers_it->is_array()) {
        *error_out = "invalid JSON response: missing servers array";
        return false;
    }

    for (const auto& server_json : *servers_it) {
        if (!server_json.is_object()) {
            continue;
        }
        CommunityActiveServer server{};
        if (const auto it = server_json.find("code"); it != server_json.end() && it->is_string()) {
            server.code = it->get<std::string>();
        }
        if (const auto it = server_json.find("name"); it != server_json.end() && it->is_string()) {
            server.name = it->get<std::string>();
        }
        if (const auto it = server_json.find("owner"); it != server_json.end() && it->is_string()) {
            server.owner = it->get<std::string>();
        }
        if (const auto it = server_json.find("host"); it != server_json.end() && it->is_string()) {
            server.host = it->get<std::string>();
        }

        int parsed_port = 0;
        if (ParseIntField(server_json, "port", &parsed_port) && parsed_port > 0 && parsed_port < 65536) {
            server.port = static_cast<uint16_t>(parsed_port);
        }
        ParseIntField(server_json, "num_players", &server.num_players);
        ParseIntField(server_json, "max_players", &server.max_players);

        out->servers.push_back(std::move(server));
    }

    return true;
}

void PrintCommunityActiveList(const CommunityActiveList& list) {
    if (!list.community_name.empty()) {
        std::printf("Community: %s\n", list.community_name.c_str());
    }
    std::printf("Active: %d  Inactive: %d\n", list.active_count, list.inactive_count);

    if (list.servers.empty()) {
        std::printf("No active servers reported.\n");
        return;
    }

    std::printf("Active Servers (%zu):\n", list.servers.size());
    for (const auto& server : list.servers) {
        std::printf("- %s (%s:%u)",
                    server.name.empty() ? "<unnamed>" : server.name.c_str(),
                    server.host.empty() ? "<host>" : server.host.c_str(),
                    static_cast<unsigned int>(server.port));
        if (server.num_players >= 0 || server.max_players >= 0) {
            std::printf(" players=%d/%d", server.num_players, server.max_players);
        }
        if (!server.owner.empty()) {
            std::printf(" owner=%s", server.owner.c_str());
        }
        if (!server.code.empty()) {
            std::printf(" code=%s", server.code.c_str());
        }
        std::printf("\n");
    }
}

} // namespace bz3::client

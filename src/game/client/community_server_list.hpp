#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bz3::client {

struct CommunityActiveServer {
    std::string code{};
    std::string name{};
    std::string owner{};
    std::string host{};
    uint16_t port = 0;
    int num_players = -1;
    int max_players = -1;
};

struct CommunityActiveList {
    std::string community_name{};
    int active_count = 0;
    int inactive_count = 0;
    std::vector<CommunityActiveServer> servers{};
};

bool FetchCommunityActiveList(const std::string& endpoint,
                              CommunityActiveList* out,
                              std::string* error_out);

void PrintCommunityActiveList(const CommunityActiveList& list);

} // namespace bz3::client

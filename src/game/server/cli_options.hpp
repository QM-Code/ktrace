#pragma once

#include <cstdint>
#include <string>

namespace bz3::server {

struct CLIOptions {
    bool trace_explicit = false;
    bool server_config_explicit = false;
    uint16_t listen_port = 0;
    bool listen_port_explicit = false;
    std::string data_dir;
    std::string user_config_path;
    bool data_dir_explicit = false;
    bool user_config_explicit = false;
    std::string community;
    bool community_explicit = false;
    std::string backend_physics;
    bool backend_physics_explicit = false;
    std::string backend_audio;
    bool backend_audio_explicit = false;
    bool strict_config = true;
    bool timestamp_logging = false;
    std::string trace_channels;
    std::string server_config_path;
};

CLIOptions ParseCLIOptions(int argc, char** argv);

} // namespace bz3::server

#pragma once

#include <cstdint>
#include <string>

namespace bz3::server {

struct CLIOptions {
    bool verbose = false;
    bool trace_explicit = false;
    bool world_specified = false;
    bool use_default_world = false;
    uint16_t host_port = 0;
    bool host_port_explicit = false;
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
    std::string world_dir;
};

CLIOptions ParseCLIOptions(int argc, char** argv);

} // namespace bz3::server

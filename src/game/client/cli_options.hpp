#pragma once

#include <cstdint>
#include <string>

namespace bz3::client {

struct CLIOptions {
    std::string player_name;
    std::string connect_addr;
    uint16_t connect_port = 0;
    std::string language;
    std::string data_dir;
    std::string user_config_path;
    std::string backend_render;
    std::string backend_ui;
    std::string backend_platform;

    bool name_explicit = false;
    bool addr_explicit = false;
    bool port_explicit = false;
    bool language_explicit = false;
    bool data_dir_explicit = false;
    bool user_config_explicit = false;
    bool backend_render_explicit = false;
    bool backend_ui_explicit = false;
    bool backend_platform_explicit = false;

    bool dev_quick_start = false;
    bool strict_config = true;

    bool verbose = false;
    bool trace_explicit = false;
    std::string trace_channels;
    bool timestamp_logging = false;
};

CLIOptions ParseCLIOptions(int argc, char** argv);

} // namespace bz3::client

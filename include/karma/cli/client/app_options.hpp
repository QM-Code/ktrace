#pragma once

#include "karma/cli/shared/parse.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace karma::cli::client {

struct AppOptions {
    std::string app_name{};
    std::string username{};
    std::string password{};
    std::string server{};
    std::string data_dir{};
    std::string user_config_path{};
    std::string backend_render{};
    std::string backend_ui{};
    std::string backend_physics{};
    std::string backend_audio{};
    std::string backend_window{};

    bool username_explicit = false;
    bool password_explicit = false;
    bool server_explicit = false;
    bool data_dir_explicit = false;
    bool user_config_explicit = false;
    bool backend_render_explicit = false;
    bool backend_ui_explicit = false;
    bool backend_physics_explicit = false;
    bool backend_audio_explicit = false;
    bool backend_window_explicit = false;

    bool strict_config = true;

    bool trace_explicit = false;
    std::string trace_channels{};
    bool timestamp_logging = false;
};

AppOptions ParseAppOptions(
    int argc,
    char** argv,
    std::string_view fallback_app_name = "app",
    const std::vector<shared::RegisteredOption>& extra_options = {});

} // namespace karma::cli::client

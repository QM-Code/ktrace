#include "client/bootstrap.hpp"
#include "client/cli_options.hpp"

#include "karma/app/engine_app.hpp"
#include "karma/common/config_helpers.hpp"

#include "game.hpp"

int main(int argc, char** argv) {
    const bz3::client::CLIOptions options = bz3::client::ParseCLIOptions(argc, argv);
    bz3::client::ConfigureLogging(options);
    bz3::client::ConfigureDataAndConfig(argc, argv);

    karma::app::EngineConfig config;
    config.window.title = karma::config::ReadRequiredStringConfig("platform.WindowTitle");
    config.window.width = karma::config::ReadRequiredUInt16Config("platform.WindowWidth");
    config.window.height = karma::config::ReadRequiredUInt16Config("platform.WindowHeight");
    config.window.preferredVideoDriver = karma::config::ReadRequiredStringConfig("platform.SdlVideoDriver");
    config.window.fullscreen = karma::config::ReadRequiredBoolConfig("platform.Fullscreen");
    config.window.wayland_libdecor = karma::config::ReadRequiredBoolConfig("platform.WaylandLibdecor");
    config.vsync = karma::config::ReadRequiredBoolConfig("platform.VSync");

    karma::app::EngineApp app;
    bz3::Game game;
    app.start(game, config);
    while (app.isRunning()) {
        app.tick();
    }
    return 0;
}

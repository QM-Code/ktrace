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
    config.default_camera.position.x = karma::config::ReadRequiredFloatConfig("render.camera.PositionX");
    config.default_camera.position.y = karma::config::ReadRequiredFloatConfig("render.camera.PositionY");
    config.default_camera.position.z = karma::config::ReadRequiredFloatConfig("render.camera.PositionZ");
    config.default_camera.target.x = karma::config::ReadRequiredFloatConfig("render.camera.TargetX");
    config.default_camera.target.y = karma::config::ReadRequiredFloatConfig("render.camera.TargetY");
    config.default_camera.target.z = karma::config::ReadRequiredFloatConfig("render.camera.TargetZ");
    config.default_camera.fov_y_degrees = karma::config::ReadRequiredFloatConfig("render.camera.FovYDegrees");
    config.default_camera.near_clip = karma::config::ReadRequiredFloatConfig("render.camera.NearClip");
    config.default_camera.far_clip = karma::config::ReadRequiredFloatConfig("render.camera.FarClip");
    config.default_light.direction.x = karma::config::ReadRequiredFloatConfig("render.light.DirectionX");
    config.default_light.direction.y = karma::config::ReadRequiredFloatConfig("render.light.DirectionY");
    config.default_light.direction.z = karma::config::ReadRequiredFloatConfig("render.light.DirectionZ");
    config.default_light.color.r = karma::config::ReadRequiredFloatConfig("render.light.ColorR");
    config.default_light.color.g = karma::config::ReadRequiredFloatConfig("render.light.ColorG");
    config.default_light.color.b = karma::config::ReadRequiredFloatConfig("render.light.ColorB");
    config.default_light.color.a = karma::config::ReadRequiredFloatConfig("render.light.ColorA");
    config.default_light.ambient.r = karma::config::ReadRequiredFloatConfig("render.light.AmbientR");
    config.default_light.ambient.g = karma::config::ReadRequiredFloatConfig("render.light.AmbientG");
    config.default_light.ambient.b = karma::config::ReadRequiredFloatConfig("render.light.AmbientB");
    config.default_light.ambient.a = karma::config::ReadRequiredFloatConfig("render.light.AmbientA");
    config.default_light.unlit = karma::config::ReadRequiredFloatConfig("render.light.Unlit");
    const std::string model_key = karma::config::ReadRequiredStringConfig("render.model");

    karma::app::EngineApp app;
    bz3::Game game(model_key);
    app.start(game, config);
    while (app.isRunning()) {
        app.tick();
    }
    return 0;
}

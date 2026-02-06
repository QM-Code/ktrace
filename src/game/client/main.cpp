#include "client/bootstrap.hpp"
#include "client/cli_options.hpp"

#include "karma/app/engine_app.hpp"
#include "karma/common/config_helpers.hpp"

#include "game.hpp"

#include <spdlog/spdlog.h>

#include <exception>
#include <stdexcept>

namespace {

glm::vec3 ReadRequiredVec3(const char* path) {
    const auto values = karma::config::ReadRequiredFloatArrayConfig(path);
    if (values.size() != 3) {
        throw std::runtime_error(std::string("Config '") + path + "' must have 3 elements");
    }
    return {values[0], values[1], values[2]};
}

glm::vec4 ReadRequiredColor(const char* path) {
    const auto values = karma::config::ReadRequiredFloatArrayConfig(path);
    if (values.size() == 3) {
        return {values[0], values[1], values[2], 1.0f};
    }
    if (values.size() == 4) {
        return {values[0], values[1], values[2], values[3]};
    }
    throw std::runtime_error(std::string("Config '") + path + "' must have 3 or 4 elements");
}

bz3::GameStartupOptions ResolveGameStartupOptions(const bz3::client::CLIOptions& options) {
    bz3::GameStartupOptions startup{};
    startup.player_name = options.name_explicit
        ? options.player_name
        : karma::config::ReadStringConfig({"userDefaults.username"}, "Player");

    const bool connect_requested = options.addr_explicit || options.port_explicit;
    if (!connect_requested) {
        return startup;
    }

    startup.connect_addr = options.addr_explicit ? options.connect_addr : std::string("localhost");
    startup.connect_port = options.port_explicit
        ? options.connect_port
        : karma::config::ReadUInt16Config({"network.ServerPort"}, static_cast<uint16_t>(11899));

    if (startup.connect_addr.empty()) {
        throw std::runtime_error("Client connect requested but address is empty.");
    }
    if (startup.connect_port == 0) {
        throw std::runtime_error("Client connect requested but port is 0.");
    }

    startup.connect_on_start = true;
    return startup;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const bz3::client::CLIOptions options = bz3::client::ParseCLIOptions(argc, argv);
        bz3::client::ConfigureLogging(options);
        bz3::client::ConfigureDataAndConfig(argc, argv);
        bz3::client::ApplyRuntimeOptionOverrides(options);

        karma::app::EngineConfig config;
        config.window.title = karma::config::ReadRequiredStringConfig("platform.WindowTitle");
        config.window.width = karma::config::ReadRequiredUInt16Config("platform.WindowWidth");
        config.window.height = karma::config::ReadRequiredUInt16Config("platform.WindowHeight");
        config.window.preferredVideoDriver = karma::config::ReadRequiredStringConfig("platform.SdlVideoDriver");
        config.window.fullscreen = karma::config::ReadRequiredBoolConfig("platform.Fullscreen");
        config.window.wayland_libdecor = karma::config::ReadRequiredBoolConfig("platform.WaylandLibdecor");
        config.vsync = karma::config::ReadRequiredBoolConfig("platform.VSync");
        config.default_camera.position = ReadRequiredVec3("camera.default.position");
        config.default_camera.target = ReadRequiredVec3("camera.default.target");
        config.default_camera.fov_y_degrees = karma::config::ReadRequiredFloatConfig("camera.default.fovYDegrees");
        config.default_camera.near_clip = karma::config::ReadRequiredFloatConfig("camera.default.nearClip");
        config.default_camera.far_clip = karma::config::ReadRequiredFloatConfig("camera.default.farClip");
        config.default_light.direction = ReadRequiredVec3("graphics.lighting.sunDirection");
        config.default_light.color = ReadRequiredColor("graphics.lighting.sunColor");
        config.default_light.ambient = ReadRequiredColor("graphics.lighting.ambientColor");
        config.default_light.unlit = karma::config::ReadFloatConfig({"graphics.lighting.unlit"}, 0.0f);

        const bz3::GameStartupOptions startup = ResolveGameStartupOptions(options);

        karma::app::EngineApp app;
        bz3::Game game{startup};
        app.start(game, config);
        while (app.isRunning()) {
            app.tick();
        }
        return 0;
    } catch (const std::exception& ex) {
        spdlog::error("bz3: {}", ex.what());
        return 1;
    }
}

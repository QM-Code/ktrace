#include "client/bootstrap.hpp"
#include "client/cli_options.hpp"
#include "client/community_server_list.hpp"

#include "karma/app/backend_resolution.hpp"
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
        if (options.community_list_active_explicit) {
            bz3::client::CommunityActiveList list{};
            std::string error{};
            if (!bz3::client::FetchCommunityActiveList(options.community_list_active, &list, &error)) {
                spdlog::error("bz3: failed to fetch active community servers: {}", error);
                return 1;
            }
            bz3::client::PrintCommunityActiveList(list);
            return 0;
        }

        bz3::client::ConfigureDataAndConfig(argc, argv);
        bz3::client::ApplyRuntimeOptionOverrides(options);

        karma::app::EngineConfig config;
        config.window.title = karma::config::ReadRequiredStringConfig("platform.WindowTitle");
        config.window.width = karma::config::ReadRequiredUInt16Config("platform.WindowWidth");
        config.window.height = karma::config::ReadRequiredUInt16Config("platform.WindowHeight");
        config.window.preferredVideoDriver = karma::app::ReadPreferredVideoDriverFromConfig();
        config.window.fullscreen = karma::config::ReadRequiredBoolConfig("platform.Fullscreen");
        config.window.wayland_libdecor = karma::config::ReadRequiredBoolConfig("platform.WaylandLibdecor");
        config.vsync = karma::config::ReadRequiredBoolConfig("platform.VSync");
        config.default_camera.position = ReadRequiredVec3("roamingMode.camera.default.position");
        config.default_camera.target = ReadRequiredVec3("roamingMode.camera.default.target");
        config.default_camera.fov_y_degrees =
            karma::config::ReadRequiredFloatConfig("roamingMode.camera.default.fovYDegrees");
        config.default_camera.near_clip =
            karma::config::ReadRequiredFloatConfig("roamingMode.camera.default.nearClip");
        config.default_camera.far_clip =
            karma::config::ReadRequiredFloatConfig("roamingMode.camera.default.farClip");
        config.default_light.direction = ReadRequiredVec3("roamingMode.graphics.lighting.sunDirection");
        config.default_light.color = ReadRequiredColor("roamingMode.graphics.lighting.sunColor");
        config.default_light.ambient = ReadRequiredColor("roamingMode.graphics.lighting.ambientColor");
        config.default_light.unlit =
            karma::config::ReadFloatConfig({"roamingMode.graphics.lighting.unlit"}, 0.0f);
        config.default_light.shadow.enabled = karma::config::ReadBoolConfig(
            {"roamingMode.graphics.lighting.shadows.enabled"},
            config.default_light.shadow.enabled);
        config.default_light.shadow.strength = karma::config::ReadFloatConfig(
            {"roamingMode.graphics.lighting.shadows.strength"},
            config.default_light.shadow.strength);
        config.default_light.shadow.bias = karma::config::ReadFloatConfig(
            {"roamingMode.graphics.lighting.shadows.bias"},
            config.default_light.shadow.bias);
        config.default_light.shadow.extent = karma::config::ReadFloatConfig(
            {"roamingMode.graphics.lighting.shadows.extent"},
            config.default_light.shadow.extent);
        config.default_light.shadow.map_size = static_cast<int>(karma::config::ReadUInt16Config(
            {"roamingMode.graphics.lighting.shadows.mapSize"},
            static_cast<uint16_t>(config.default_light.shadow.map_size)));
        config.default_light.shadow.pcf_radius = static_cast<int>(karma::config::ReadUInt16Config(
            {"roamingMode.graphics.lighting.shadows.pcfRadius"},
            static_cast<uint16_t>(config.default_light.shadow.pcf_radius)));
        config.default_light.shadow.update_every_frames = static_cast<int>(karma::config::ReadUInt16Config(
            {"roamingMode.graphics.lighting.shadows.updateEveryFrames"},
            static_cast<uint16_t>(config.default_light.shadow.update_every_frames)));
        config.render_backend =
            karma::app::ResolveRenderBackendFromOption(options.backend_render, options.backend_render_explicit);
        config.physics_backend =
            karma::app::ResolvePhysicsBackendFromOption(options.backend_physics, options.backend_physics_explicit);
        config.audio_backend =
            karma::app::ResolveAudioBackendFromOption(options.backend_audio, options.backend_audio_explicit);
        config.enable_audio = karma::config::ReadBoolConfig({"audio.enabled"}, true);
        config.simulation_fixed_hz = karma::config::ReadFloatConfig({"simulation.fixedHz"}, 60.0f);
        config.simulation_max_frame_dt =
            karma::config::ReadFloatConfig({"simulation.maxFrameDeltaTime"}, 0.25f);
        config.simulation_max_steps =
            static_cast<int>(karma::config::ReadUInt16Config({"simulation.maxSubsteps"}, 4));
        config.ui_backend_override =
            karma::app::ResolveUiBackendOverrideFromOption(options.backend_ui, options.backend_ui_explicit);

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

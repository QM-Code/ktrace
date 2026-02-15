#include "client/bootstrap.hpp"
#include "client/cli_options.hpp"
#include "client/community_server_list.hpp"

#include "karma/app/backend_resolution.hpp"
#include "karma/app/engine_app.hpp"
#include "karma/common/config_helpers.hpp"

#include "game.hpp"

#include <spdlog/spdlog.h>

#include <exception>
#include <limits>
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

    const bool connect_requested = options.addr_explicit;
    if (!connect_requested) {
        return startup;
    }

    const std::string endpoint = options.connect_addr;
    const std::size_t split = endpoint.rfind(':');
    if (split == std::string::npos || split == 0 || split + 1 >= endpoint.size()) {
        throw std::runtime_error("Client --addr must be formatted as <host:port>.");
    }
    startup.connect_addr = endpoint.substr(0, split);
    const std::string port_text = endpoint.substr(split + 1);
    uint16_t parsed_port = 0;
    try {
        const unsigned long raw = std::stoul(port_text);
        if (raw == 0 || raw > std::numeric_limits<uint16_t>::max()) {
            throw std::runtime_error("out of range");
        }
        parsed_port = static_cast<uint16_t>(raw);
    } catch (...) {
        throw std::runtime_error("Client --addr port must be an integer in range 1..65535.");
    }
    startup.connect_port = parsed_port;

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
        config.default_light.shadow.receiver_bias_scale = karma::config::ReadFloatConfig(
            {"roamingMode.graphics.lighting.shadows.receiverBiasScale"},
            config.default_light.shadow.receiver_bias_scale);
        config.default_light.shadow.normal_bias_scale = karma::config::ReadFloatConfig(
            {"roamingMode.graphics.lighting.shadows.normalBiasScale"},
            config.default_light.shadow.normal_bias_scale);
        config.default_light.shadow.raster_depth_bias = karma::config::ReadFloatConfig(
            {"roamingMode.graphics.lighting.shadows.rasterDepthBias"},
            config.default_light.shadow.raster_depth_bias);
        config.default_light.shadow.raster_slope_bias = karma::config::ReadFloatConfig(
            {"roamingMode.graphics.lighting.shadows.rasterSlopeBias"},
            config.default_light.shadow.raster_slope_bias);
        config.default_light.shadow.extent = karma::config::ReadFloatConfig(
            {"roamingMode.graphics.lighting.shadows.extent"},
            config.default_light.shadow.extent);
        config.default_light.shadow.map_size = static_cast<int>(karma::config::ReadUInt16Config(
            {"roamingMode.graphics.lighting.shadows.mapSize"},
            static_cast<uint16_t>(config.default_light.shadow.map_size)));
        config.default_light.shadow.pcf_radius = static_cast<int>(karma::config::ReadUInt16Config(
            {"roamingMode.graphics.lighting.shadows.pcfRadius"},
            static_cast<uint16_t>(config.default_light.shadow.pcf_radius)));
        config.default_light.shadow.triangle_budget = static_cast<int>(karma::config::ReadUInt16Config(
            {"roamingMode.graphics.lighting.shadows.triangleBudget"},
            static_cast<uint16_t>(config.default_light.shadow.triangle_budget)));
        config.default_light.shadow.update_every_frames = static_cast<int>(karma::config::ReadUInt16Config(
            {"roamingMode.graphics.lighting.shadows.updateEveryFrames"},
            static_cast<uint16_t>(config.default_light.shadow.update_every_frames)));
        const std::string shadow_execution_mode_raw = karma::config::ReadStringConfig(
            {"roamingMode.graphics.lighting.shadows.executionMode"},
            karma::renderer::DirectionalLightData::ShadowExecutionModeToken(
                config.default_light.shadow.execution_mode));
        karma::renderer::DirectionalLightData::ShadowExecutionMode shadow_execution_mode =
            config.default_light.shadow.execution_mode;
        if (!karma::renderer::DirectionalLightData::TryParseShadowExecutionMode(
                shadow_execution_mode_raw,
                &shadow_execution_mode)) {
            spdlog::warn(
                "bz3: invalid roamingMode.graphics.lighting.shadows.executionMode='{}'; using '{}'",
                shadow_execution_mode_raw,
                karma::renderer::DirectionalLightData::ShadowExecutionModeToken(
                    config.default_light.shadow.execution_mode));
        } else {
            config.default_light.shadow.execution_mode = shadow_execution_mode;
        }
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

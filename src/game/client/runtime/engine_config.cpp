#include "client/runtime/internal.hpp"

#include "karma/app/client/backend_resolution.hpp"
#include "karma/app/shared/backend_resolution.hpp"
#include "karma/common/config/helpers.hpp"

#include <stdexcept>
#include <string>

#include <spdlog/spdlog.h>

namespace bz3::client::runtime::detail {

glm::vec3 ReadRequiredVec3(const char* path) {
    const auto values = karma::common::config::ReadRequiredFloatArrayConfig(path);
    if (values.size() != 3) {
        throw std::runtime_error(std::string("Config '") + path + "' must have 3 elements");
    }
    return {values[0], values[1], values[2]};
}

glm::vec4 ReadRequiredColor(const char* path) {
    const auto values = karma::common::config::ReadRequiredFloatArrayConfig(path);
    if (values.size() == 3) {
        return {values[0], values[1], values[2], 1.0f};
    }
    if (values.size() == 4) {
        return {values[0], values[1], values[2], values[3]};
    }
    throw std::runtime_error(std::string("Config '") + path + "' must have 3 or 4 elements");
}

karma::app::client::EngineConfig BuildEngineConfig(const karma::cli::client::AppOptions& options) {
    karma::app::client::EngineConfig config;
    config.window.title = karma::common::config::ReadRequiredStringConfig("window.WindowTitle");
    config.window.width = karma::common::config::ReadRequiredUInt16Config("window.WindowWidth");
    config.window.height = karma::common::config::ReadRequiredUInt16Config("window.WindowHeight");
    config.window.preferredVideoDriver = karma::app::client::ReadPreferredVideoDriverFromConfig();
    config.window.fullscreen = karma::common::config::ReadRequiredBoolConfig("window.Fullscreen");
    config.window.wayland_libdecor = karma::common::config::ReadRequiredBoolConfig("window.WaylandLibdecor");
    config.vsync = karma::common::config::ReadRequiredBoolConfig("window.VSync");
    config.default_camera.position = ReadRequiredVec3("roamingMode.camera.default.position");
    config.default_camera.target = ReadRequiredVec3("roamingMode.camera.default.target");
    config.default_camera.fov_y_degrees =
        karma::common::config::ReadRequiredFloatConfig("roamingMode.camera.default.fovYDegrees");
    config.default_camera.near_clip = karma::common::config::ReadRequiredFloatConfig("roamingMode.camera.default.nearClip");
    config.default_camera.far_clip = karma::common::config::ReadRequiredFloatConfig("roamingMode.camera.default.farClip");
    config.default_light.direction = ReadRequiredVec3("roamingMode.graphics.lighting.sunDirection");
    config.default_light.color = ReadRequiredColor("roamingMode.graphics.lighting.sunColor");
    config.default_light.ambient = ReadRequiredColor("roamingMode.graphics.lighting.ambientColor");
    config.default_light.unlit = karma::common::config::ReadFloatConfig({"roamingMode.graphics.lighting.unlit"}, 0.0f);
    config.default_light.shadow.enabled = karma::common::config::ReadBoolConfig(
        {"roamingMode.graphics.lighting.shadows.enabled"},
        config.default_light.shadow.enabled);
    config.default_light.shadow.strength = karma::common::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.strength"},
        config.default_light.shadow.strength);
    config.default_light.shadow.bias = karma::common::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.bias"},
        config.default_light.shadow.bias);
    config.default_light.shadow.receiver_bias_scale = karma::common::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.receiverBiasScale"},
        config.default_light.shadow.receiver_bias_scale);
    config.default_light.shadow.normal_bias_scale = karma::common::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.normalBiasScale"},
        config.default_light.shadow.normal_bias_scale);
    config.default_light.shadow.raster_depth_bias = karma::common::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.rasterDepthBias"},
        config.default_light.shadow.raster_depth_bias);
    config.default_light.shadow.raster_slope_bias = karma::common::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.rasterSlopeBias"},
        config.default_light.shadow.raster_slope_bias);
    config.default_light.shadow.extent = karma::common::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.extent"},
        config.default_light.shadow.extent);
    config.default_light.shadow.map_size = static_cast<int>(karma::common::config::ReadUInt16Config(
        {"roamingMode.graphics.lighting.shadows.mapSize"},
        static_cast<uint16_t>(config.default_light.shadow.map_size)));
    config.default_light.shadow.pcf_radius = static_cast<int>(karma::common::config::ReadUInt16Config(
        {"roamingMode.graphics.lighting.shadows.pcfRadius"},
        static_cast<uint16_t>(config.default_light.shadow.pcf_radius)));
    config.default_light.shadow.triangle_budget = static_cast<int>(karma::common::config::ReadUInt16Config(
        {"roamingMode.graphics.lighting.shadows.triangleBudget"},
        static_cast<uint16_t>(config.default_light.shadow.triangle_budget)));
    config.default_light.shadow.update_every_frames = static_cast<int>(karma::common::config::ReadUInt16Config(
        {"roamingMode.graphics.lighting.shadows.updateEveryFrames"},
        static_cast<uint16_t>(config.default_light.shadow.update_every_frames)));
    config.default_light.shadow.point_map_size = static_cast<int>(karma::common::config::ReadUInt16Config(
        {"roamingMode.graphics.lighting.shadows.pointMapSize"},
        static_cast<uint16_t>(config.default_light.shadow.point_map_size)));
    config.default_light.shadow.point_max_shadow_lights = static_cast<int>(karma::common::config::ReadUInt16Config(
        {"roamingMode.graphics.lighting.shadows.pointMaxShadowLights"},
        static_cast<uint16_t>(config.default_light.shadow.point_max_shadow_lights)));
    config.default_light.shadow.point_faces_per_frame_budget = static_cast<int>(karma::common::config::ReadUInt16Config(
        {"roamingMode.graphics.lighting.shadows.pointFacesPerFrameBudget"},
        static_cast<uint16_t>(config.default_light.shadow.point_faces_per_frame_budget)));
    config.default_light.shadow.point_constant_bias = karma::common::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.pointConstantBias"},
        config.default_light.shadow.point_constant_bias);
    config.default_light.shadow.point_slope_bias_scale = karma::common::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.pointSlopeBiasScale"},
        config.default_light.shadow.point_slope_bias_scale);
    config.default_light.shadow.point_normal_bias_scale = karma::common::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.pointNormalBiasScale"},
        config.default_light.shadow.point_normal_bias_scale);
    config.default_light.shadow.point_receiver_bias_scale = karma::common::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.pointReceiverBiasScale"},
        config.default_light.shadow.point_receiver_bias_scale);
    config.default_light.shadow.local_light_distance_damping = karma::common::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.localLightDistanceDamping"},
        config.default_light.shadow.local_light_distance_damping);
    config.default_light.shadow.local_light_range_falloff_exponent = karma::common::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.localLightRangeFalloffExponent"},
        config.default_light.shadow.local_light_range_falloff_exponent);
    config.default_light.shadow.ao_affects_local_lights = karma::common::config::ReadBoolConfig(
        {"roamingMode.graphics.lighting.shadows.aoAffectsLocalLights"},
        config.default_light.shadow.ao_affects_local_lights);
    config.default_light.shadow.local_light_directional_shadow_lift_strength = karma::common::config::ReadFloatConfig(
        {"roamingMode.graphics.lighting.shadows.localLightDirectionalShadowLiftStrength"},
        config.default_light.shadow.local_light_directional_shadow_lift_strength);
    const std::string shadow_execution_mode_raw = karma::common::config::ReadStringConfig(
        {"roamingMode.graphics.lighting.shadows.executionMode"},
        karma::renderer::DirectionalLightData::ShadowExecutionModeToken(
            config.default_light.shadow.execution_mode));
    karma::renderer::DirectionalLightData::ShadowExecutionMode shadow_execution_mode =
        config.default_light.shadow.execution_mode;
    if (!karma::renderer::DirectionalLightData::TryParseShadowExecutionMode(
            shadow_execution_mode_raw,
            &shadow_execution_mode)) {
        spdlog::warn(
            "{}: invalid roamingMode.graphics.lighting.shadows.executionMode='{}'; using '{}'",
            options.app_name,
            shadow_execution_mode_raw,
            karma::renderer::DirectionalLightData::ShadowExecutionModeToken(
                config.default_light.shadow.execution_mode));
    } else {
        config.default_light.shadow.execution_mode = shadow_execution_mode;
    }
    config.render_backend =
        karma::app::client::ResolveRenderBackendFromOption(options.backend_render, options.backend_render_explicit);
    config.physics_backend =
        karma::app::shared::ResolvePhysicsBackendFromOption(options.backend_physics, options.backend_physics_explicit);
    config.audio_backend =
        karma::app::shared::ResolveAudioBackendFromOption(options.backend_audio, options.backend_audio_explicit);
    config.enable_audio = karma::common::config::ReadBoolConfig({"audio.enabled"}, true);
    config.simulation_fixed_hz = karma::common::config::ReadFloatConfig({"simulation.fixedHz"}, 60.0f);
    config.simulation_max_frame_dt = karma::common::config::ReadFloatConfig({"simulation.maxFrameDeltaTime"}, 0.25f);
    config.simulation_max_steps =
        static_cast<int>(karma::common::config::ReadUInt16Config({"simulation.maxSubsteps"}, 4));
    config.ui_backend_override =
        karma::app::client::ResolveUiBackendOverrideFromOption(options.backend_ui, options.backend_ui_explicit);
    return config;
}

} // namespace bz3::client::runtime::detail

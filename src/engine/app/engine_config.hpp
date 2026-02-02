#pragma once

#include "karma/platform/window.hpp"

namespace karma::app {

struct EngineConfig {
    platform::WindowConfig window{};
    bool cursor_visible = true;
    bool enable_anisotropy = false;
    int anisotropy_level = 1;
    bool generate_mipmaps = false;
    int shadow_map_size = 1024;
    int shadow_pcf_radius = 1;
    bool enable_ecs_render_sync = false;
    bool enable_ecs_physics_sync = false;
    bool enable_ecs_audio_sync = false;
    bool enable_ecs_camera_sync = false;
    bool enable_fixed_update = true;
    float fixed_timestep = 1.0f / 60.0f;
};

} // namespace karma::app

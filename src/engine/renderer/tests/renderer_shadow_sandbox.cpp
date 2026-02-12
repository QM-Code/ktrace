#include "karma/common/logging.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "karma/ecs/world.hpp"
#include "karma/platform/window.hpp"
#include "karma/renderer/backend.hpp"
#include "karma/renderer/device.hpp"
#include "karma/renderer/render_system.hpp"
#include "karma/scene/components.hpp"
#include "karma/scene/scene.hpp"
#include "renderer/backends/directional_shadow_internal.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct SandboxOptions {
    karma::renderer_backend::BackendKind backend = karma::renderer_backend::BackendKind::Auto;
    int width = 1280;
    int height = 720;
    float duration_seconds = 0.0f;
    int ground_tiles = 1;
    float ground_extent = 20.0f;
    glm::vec3 sun_direction = glm::normalize(glm::vec3(0.35f, -1.0f, -0.25f));
    float shadow_strength = 0.85f;
    float shadow_extent = 22.0f;
    int shadow_map_size = 1024;
    int shadow_pcf_radius = 2;
    float shadow_bias = 0.0008f;
    bool verbose = false;
    std::string trace_channels{};
    std::string preferred_video_driver{};
};

struct OrbitCameraState {
    float yaw_degrees = 35.0f;
    float pitch_degrees = 22.0f;
    float radius = 22.0f;
    glm::vec3 target{0.0f, 0.8f, 0.0f};
};

struct ShadowEntity {
    karma::ecs::Entity entity{};
    const karma::renderer::MeshData* mesh = nullptr;
    glm::vec3 sample_center{0.0f, 0.0f, 0.0f};
    bool is_ground = false;
};

struct ShadowGridStats {
    float min_factor = 1.0f;
    float max_factor = 0.0f;
    float average_factor = 1.0f;
    int sample_count = 0;
};

void PrintUsage(const char* argv0) {
    std::cout
        << "Usage: " << argv0 << " [options]\n"
        << "Options:\n"
        << "  --backend-render <auto|bgfx|diligent>\n"
        << "  --duration-sec <seconds>      Auto-exit after N seconds (0 = run until quit)\n"
        << "  --width <pixels>              Window width (default 1280)\n"
        << "  --height <pixels>             Window height (default 720)\n"
        << "  --ground-tiles <N>            Ground draw subdivisions per axis (default 1)\n"
        << "  --ground-extent <meters>      Half-extent of ground area (default 20)\n"
        << "  --sun-dir <x,y,z>             Directional light vector (default 0.35,-1,-0.25)\n"
        << "  --shadow-map-size <64..2048>  Directional shadow map size (default 1024)\n"
        << "  --shadow-strength <0..1>      Shadow strength (default 0.85)\n"
        << "  --shadow-extent <2..512>      Shadow projection extent (default 22)\n"
        << "  --shadow-bias <0..0.02>       Shadow bias (default 0.0008)\n"
        << "  --shadow-pcf <0..4>           PCF radius (default 2)\n"
        << "  --video-driver <name>         SDL video driver override (e.g. wayland, x11)\n"
        << "  -v, --verbose                 Enable debug logging\n"
        << "  -t, --trace <channels>        Enable KARMA trace channels\n"
        << "  --help                        Show this help\n"
        << "\nRuntime controls:\n"
        << "  Esc: quit\n"
        << "  Arrow left/right: orbit camera yaw\n"
        << "  Arrow up/down: orbit camera pitch\n"
        << "  PageUp/PageDown: zoom camera\n"
        << "  A/D: rotate sun azimuth\n"
        << "  W/S: adjust sun elevation\n";
}

bool ParseVec3(std::string_view text, glm::vec3& out_vec) {
    std::array<float, 3> values{};
    std::size_t start = 0;
    for (std::size_t i = 0; i < values.size(); ++i) {
        const std::size_t comma = text.find(',', start);
        const bool last = (i + 1u) == values.size();
        if (last && comma != std::string_view::npos) {
            return false;
        }
        if (!last && comma == std::string_view::npos) {
            return false;
        }
        const std::string token(
            text.substr(start, last ? std::string_view::npos : (comma - start)));
        try {
            values[i] = std::stof(token);
        } catch (...) {
            return false;
        }
        if (!last) {
            start = comma + 1u;
        }
    }
    out_vec = glm::vec3(values[0], values[1], values[2]);
    return std::isfinite(out_vec.x) && std::isfinite(out_vec.y) && std::isfinite(out_vec.z);
}

float ClampPositive(float value, float fallback) {
    if (!std::isfinite(value) || value <= 0.0f) {
        return fallback;
    }
    return value;
}

SandboxOptions ParseOptions(int argc, char** argv) {
    SandboxOptions options{};
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto require_value = [&](const char* name) -> std::string {
            if ((i + 1) >= argc) {
                throw std::runtime_error(std::string("Missing value for ") + name);
            }
            return std::string(argv[++i]);
        };

        if (arg == "--help") {
            PrintUsage(argv[0]);
            std::exit(0);
        } else if (arg == "--backend-render") {
            const std::string value = require_value("--backend-render");
            const auto parsed = karma::renderer_backend::ParseBackendKind(value);
            if (!parsed) {
                throw std::runtime_error(
                    "Invalid --backend-render value '" + value + "' (expected auto|bgfx|diligent)");
            }
            options.backend = *parsed;
        } else if (arg == "--duration-sec") {
            options.duration_seconds =
                std::max(0.0f, std::stof(require_value("--duration-sec")));
        } else if (arg == "--width") {
            options.width = std::max(320, std::stoi(require_value("--width")));
        } else if (arg == "--height") {
            options.height = std::max(240, std::stoi(require_value("--height")));
        } else if (arg == "--ground-tiles") {
            options.ground_tiles = std::clamp(std::stoi(require_value("--ground-tiles")), 1, 64);
        } else if (arg == "--ground-extent") {
            options.ground_extent = ClampPositive(std::stof(require_value("--ground-extent")), 20.0f);
        } else if (arg == "--sun-dir") {
            glm::vec3 direction{};
            if (!ParseVec3(require_value("--sun-dir"), direction) || glm::length(direction) <= 1e-6f) {
                throw std::runtime_error("Invalid --sun-dir vector; expected finite x,y,z values");
            }
            options.sun_direction = glm::normalize(direction);
        } else if (arg == "--shadow-map-size") {
            options.shadow_map_size =
                std::clamp(std::stoi(require_value("--shadow-map-size")), 64, 2048);
        } else if (arg == "--shadow-strength") {
            options.shadow_strength =
                std::clamp(std::stof(require_value("--shadow-strength")), 0.0f, 1.0f);
        } else if (arg == "--shadow-extent") {
            options.shadow_extent = std::clamp(
                std::stof(require_value("--shadow-extent")), 2.0f, 512.0f);
        } else if (arg == "--shadow-bias") {
            options.shadow_bias = std::clamp(
                std::stof(require_value("--shadow-bias")), 0.0f, 0.02f);
        } else if (arg == "--shadow-pcf") {
            options.shadow_pcf_radius =
                std::clamp(std::stoi(require_value("--shadow-pcf")), 0, 4);
        } else if (arg == "--video-driver") {
            options.preferred_video_driver = require_value("--video-driver");
        } else if (arg == "-v" || arg == "--verbose") {
            options.verbose = true;
        } else if (arg == "-t" || arg == "--trace") {
            options.trace_channels = require_value("--trace");
        } else {
            throw std::runtime_error("Unknown option: " + arg);
        }
    }
    return options;
}

glm::mat4 BuildTransform(const glm::vec3& position, const glm::vec3& scale) {
    return glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), scale);
}

karma::renderer::MeshData BuildUnitPlaneMesh() {
    karma::renderer::MeshData mesh{};
    mesh.positions = {
        {-0.5f, 0.0f, -0.5f},
        {0.5f, 0.0f, -0.5f},
        {0.5f, 0.0f, 0.5f},
        {-0.5f, 0.0f, 0.5f},
    };
    mesh.normals = {
        {0.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
    };
    mesh.uvs = {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f},
    };
    // Winding is CCW when viewed from +Y so top faces are front-facing.
    mesh.indices = {0, 2, 1, 0, 3, 2};
    return mesh;
}

karma::renderer::MeshData BuildUnitCubeMesh() {
    karma::renderer::MeshData mesh{};
    // 24 vertices (4 per face) for stable flat-face normals.
    const std::array<glm::vec3, 24> positions = {
        // +X
        glm::vec3(0.5f, -0.5f, -0.5f), glm::vec3(0.5f, -0.5f, 0.5f),
        glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(0.5f, 0.5f, -0.5f),
        // -X
        glm::vec3(-0.5f, -0.5f, 0.5f), glm::vec3(-0.5f, -0.5f, -0.5f),
        glm::vec3(-0.5f, 0.5f, -0.5f), glm::vec3(-0.5f, 0.5f, 0.5f),
        // +Y
        glm::vec3(-0.5f, 0.5f, -0.5f), glm::vec3(0.5f, 0.5f, -0.5f),
        glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(-0.5f, 0.5f, 0.5f),
        // -Y
        glm::vec3(-0.5f, -0.5f, 0.5f), glm::vec3(0.5f, -0.5f, 0.5f),
        glm::vec3(0.5f, -0.5f, -0.5f), glm::vec3(-0.5f, -0.5f, -0.5f),
        // +Z
        glm::vec3(0.5f, -0.5f, 0.5f), glm::vec3(-0.5f, -0.5f, 0.5f),
        glm::vec3(-0.5f, 0.5f, 0.5f), glm::vec3(0.5f, 0.5f, 0.5f),
        // -Z
        glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(0.5f, -0.5f, -0.5f),
        glm::vec3(0.5f, 0.5f, -0.5f), glm::vec3(-0.5f, 0.5f, -0.5f),
    };
    const std::array<glm::vec3, 24> normals = {
        glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f),
        glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
        glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 0.0f, -1.0f),
        glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 0.0f, -1.0f),
    };
    const std::array<glm::vec2, 24> uvs = {
        glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f),
        glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f),
        glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f),
        glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f),
        glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f),
        glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f),
    };
    mesh.positions.assign(positions.begin(), positions.end());
    mesh.normals.assign(normals.begin(), normals.end());
    mesh.uvs.assign(uvs.begin(), uvs.end());
    // Winding is CCW when viewed from outside so culling produces solid closed cubes.
    mesh.indices = {
        0, 2, 1, 0, 3, 2,       // +X
        4, 6, 5, 4, 7, 6,       // -X
        8, 10, 9, 8, 11, 10,    // +Y
        12, 14, 13, 12, 15, 14, // -Y
        16, 18, 17, 16, 19, 18, // +Z
        20, 22, 21, 20, 23, 22, // -Z
    };
    return mesh;
}

glm::vec3 ComputeMeshSampleCenter(const karma::renderer::MeshData& mesh) {
    if (mesh.positions.empty()) {
        return glm::vec3(0.0f);
    }
    glm::vec3 min_pos = mesh.positions.front();
    glm::vec3 max_pos = mesh.positions.front();
    for (const glm::vec3& position : mesh.positions) {
        min_pos = glm::min(min_pos, position);
        max_pos = glm::max(max_pos, position);
    }
    return 0.5f * (min_pos + max_pos);
}

karma::renderer::MaterialDesc MakeMaterial(const glm::vec4& color,
                                           float roughness,
                                           float metallic) {
    karma::renderer::MaterialDesc material{};
    material.base_color = color;
    material.roughness_factor = roughness;
    material.metallic_factor = metallic;
    material.occlusion_strength = 1.0f;
    material.normal_scale = 1.0f;
    material.double_sided = false;
    return material;
}

void AddRenderableEntity(karma::ecs::World& world,
                         std::vector<ShadowEntity>& shadow_entities,
                         const karma::renderer::MeshData& mesh_data,
                         karma::renderer::MeshId mesh_id,
                         karma::renderer::MaterialId material_id,
                         const glm::mat4& transform,
                         bool is_ground,
                         bool casts_shadow) {
    const karma::ecs::Entity entity = world.createEntity();
    world.add(entity, karma::scene::TransformComponent{transform, transform});
    world.add(entity, karma::scene::RenderComponent{mesh_id, material_id, 0u, true, casts_shadow});
    shadow_entities.push_back(ShadowEntity{
        entity,
        &mesh_data,
        ComputeMeshSampleCenter(mesh_data),
        is_ground,
    });
}

glm::vec3 DirectionFromAngles(float azimuth_degrees, float elevation_degrees) {
    const float azimuth = glm::radians(azimuth_degrees);
    const float elevation = glm::radians(elevation_degrees);
    const float cos_elevation = std::cos(elevation);
    const glm::vec3 direction{
        cos_elevation * std::cos(azimuth),
        std::sin(elevation),
        cos_elevation * std::sin(azimuth),
    };
    if (glm::length(direction) <= 1e-6f) {
        return glm::vec3(0.35f, -1.0f, -0.25f);
    }
    return glm::normalize(direction);
}

void AnglesFromDirection(const glm::vec3& direction,
                         float& out_azimuth_degrees,
                         float& out_elevation_degrees) {
    const glm::vec3 normalized =
        (glm::length(direction) <= 1e-6f) ? glm::vec3(0.35f, -1.0f, -0.25f) : glm::normalize(direction);
    out_elevation_degrees = glm::degrees(std::asin(std::clamp(normalized.y, -1.0f, 1.0f)));
    out_azimuth_degrees = glm::degrees(std::atan2(normalized.z, normalized.x));
}

void UpdateInteractiveControls(const karma::platform::Window& window,
                               float dt,
                               OrbitCameraState& camera,
                               float& sun_azimuth_degrees,
                               float& sun_elevation_degrees) {
    const float camera_yaw_speed = 75.0f;
    const float camera_pitch_speed = 60.0f;
    const float camera_zoom_speed = 18.0f;
    const float sun_azimuth_speed = 60.0f;
    const float sun_elevation_speed = 50.0f;

    if (window.isKeyDown(karma::platform::Key::Left)) {
        camera.yaw_degrees -= camera_yaw_speed * dt;
    }
    if (window.isKeyDown(karma::platform::Key::Right)) {
        camera.yaw_degrees += camera_yaw_speed * dt;
    }
    if (window.isKeyDown(karma::platform::Key::Up)) {
        camera.pitch_degrees += camera_pitch_speed * dt;
    }
    if (window.isKeyDown(karma::platform::Key::Down)) {
        camera.pitch_degrees -= camera_pitch_speed * dt;
    }
    if (window.isKeyDown(karma::platform::Key::PageUp)) {
        camera.radius -= camera_zoom_speed * dt;
    }
    if (window.isKeyDown(karma::platform::Key::PageDown)) {
        camera.radius += camera_zoom_speed * dt;
    }

    if (window.isKeyDown(karma::platform::Key::A)) {
        sun_azimuth_degrees -= sun_azimuth_speed * dt;
    }
    if (window.isKeyDown(karma::platform::Key::D)) {
        sun_azimuth_degrees += sun_azimuth_speed * dt;
    }
    if (window.isKeyDown(karma::platform::Key::W)) {
        sun_elevation_degrees += sun_elevation_speed * dt;
    }
    if (window.isKeyDown(karma::platform::Key::S)) {
        sun_elevation_degrees -= sun_elevation_speed * dt;
    }

    camera.pitch_degrees = std::clamp(camera.pitch_degrees, -80.0f, 80.0f);
    camera.radius = std::clamp(camera.radius, 5.0f, 80.0f);
    sun_elevation_degrees = std::clamp(sun_elevation_degrees, -85.0f, -5.0f);
}

karma::renderer::CameraData BuildCamera(const OrbitCameraState& state) {
    const float yaw = glm::radians(state.yaw_degrees);
    const float pitch = glm::radians(state.pitch_degrees);
    const float cp = std::cos(pitch);
    const glm::vec3 offset{
        state.radius * cp * std::cos(yaw),
        state.radius * std::sin(pitch),
        state.radius * cp * std::sin(yaw),
    };

    karma::renderer::CameraData camera{};
    camera.position = state.target + offset;
    camera.target = state.target;
    camera.fov_y_degrees = 60.0f;
    camera.near_clip = 0.1f;
    camera.far_clip = 500.0f;
    return camera;
}

ShadowGridStats SampleGroundGridFactors(
    const karma::renderer_backend::detail::DirectionalShadowMap& shadow_map,
    float ground_extent,
    int samples_per_axis) {
    ShadowGridStats stats{};
    stats.min_factor = 1.0f;
    stats.max_factor = 0.0f;
    stats.average_factor = 1.0f;
    stats.sample_count = 0;

    if (samples_per_axis <= 1) {
        return stats;
    }

    float sum = 0.0f;
    for (int z = 0; z < samples_per_axis; ++z) {
        for (int x = 0; x < samples_per_axis; ++x) {
            const float tx =
                static_cast<float>(x) / static_cast<float>(samples_per_axis - 1);
            const float tz =
                static_cast<float>(z) / static_cast<float>(samples_per_axis - 1);
            const glm::vec3 world_point{
                glm::mix(-ground_extent, ground_extent, tx),
                0.0f,
                glm::mix(-ground_extent, ground_extent, tz),
            };
            const float visibility =
                karma::renderer_backend::detail::SampleDirectionalShadowVisibility(shadow_map, world_point);
            const float factor =
                karma::renderer_backend::detail::ComputeDirectionalShadowFactor(shadow_map, visibility);
            stats.min_factor = std::min(stats.min_factor, factor);
            stats.max_factor = std::max(stats.max_factor, factor);
            sum += factor;
            ++stats.sample_count;
        }
    }
    if (stats.sample_count > 0) {
        stats.average_factor = sum / static_cast<float>(stats.sample_count);
    }
    return stats;
}

void LogShadowDiagnostics(const char* backend_name,
                          const karma::renderer::DirectionalLightData& light,
                          const std::vector<ShadowEntity>& shadow_entities,
                          const karma::ecs::World& world,
                          float ground_extent,
                          float frame_dt_avg,
                          float frame_dt_max) {
    std::vector<karma::renderer_backend::detail::DirectionalShadowCaster> casters{};
    casters.reserve(shadow_entities.size());

    for (const ShadowEntity& shadow_entity : shadow_entities) {
        const auto* transform =
            world.tryGet<karma::scene::TransformComponent>(shadow_entity.entity);
        if (!transform || !shadow_entity.mesh) {
            continue;
        }

        karma::renderer_backend::detail::DirectionalShadowCaster caster{};
        caster.transform = transform->world;
        caster.positions = &shadow_entity.mesh->positions;
        caster.indices = &shadow_entity.mesh->indices;
        caster.sample_center = shadow_entity.sample_center;
        caster.casts_shadow = !shadow_entity.is_ground;
        casters.push_back(caster);
    }

    const auto semantics =
        karma::renderer_backend::detail::ResolveDirectionalShadowSemantics(light);
    const auto shadow_map = karma::renderer_backend::detail::BuildDirectionalShadowMap(
        semantics, light.direction, casters);

    float ground_draw_min = 1.0f;
    float ground_draw_max = 0.0f;
    float ground_draw_sum = 0.0f;
    int ground_draw_count = 0;

    for (const ShadowEntity& shadow_entity : shadow_entities) {
        if (!shadow_entity.is_ground) {
            continue;
        }
        const auto* transform =
            world.tryGet<karma::scene::TransformComponent>(shadow_entity.entity);
        if (!transform || !shadow_entity.mesh) {
            continue;
        }
        const float visibility =
            karma::renderer_backend::detail::ComputeDirectionalShadowVisibilityForReceiver(
                shadow_map,
                transform->world,
                &shadow_entity.mesh->positions,
                &shadow_entity.mesh->indices,
                shadow_entity.sample_center);
        const float factor =
            karma::renderer_backend::detail::ComputeDirectionalShadowFactor(shadow_map, visibility);
        ground_draw_min = std::min(ground_draw_min, factor);
        ground_draw_max = std::max(ground_draw_max, factor);
        ground_draw_sum += factor;
        ++ground_draw_count;
    }

    const ShadowGridStats grid_stats = SampleGroundGridFactors(shadow_map, ground_extent, 25);
    const float ground_draw_avg =
        (ground_draw_count > 0) ? (ground_draw_sum / static_cast<float>(ground_draw_count)) : 1.0f;
    const float flattening_gap = std::max(0.0f, ground_draw_min - grid_stats.min_factor);

    spdlog::info(
        "[sandbox] backend={} map_ready={} draws(ground={}) "
        "ground_draw_factor[min={:.3f} avg={:.3f} max={:.3f}] "
        "ground_grid_factor[min={:.3f} avg={:.3f} max={:.3f}] "
        "flattening_gap={:.3f} "
        "sun_dir=({:.2f},{:.2f},{:.2f}) "
        "dt_raw(avg={:.4f} max={:.4f})",
        backend_name ? backend_name : "unknown",
        shadow_map.ready ? 1 : 0,
        ground_draw_count,
        ground_draw_min,
        ground_draw_avg,
        ground_draw_max,
        grid_stats.min_factor,
        grid_stats.average_factor,
        grid_stats.max_factor,
        flattening_gap,
        light.direction.x,
        light.direction.y,
        light.direction.z,
        frame_dt_avg,
        frame_dt_max);
}

bool ShouldQuitFromEvents(const std::vector<karma::platform::Event>& events) {
    for (const auto& event : events) {
        if (event.type == karma::platform::EventType::Quit ||
            event.type == karma::platform::EventType::WindowClose) {
            return true;
        }
        if (event.type == karma::platform::EventType::KeyDown &&
            event.key == karma::platform::Key::Escape) {
            return true;
        }
    }
    return false;
}

void SubmitLightDebugLine(karma::renderer::RenderSystem& render,
                          const karma::renderer::DirectionalLightData& light,
                          const glm::vec3& center) {
    karma::renderer::DebugLineItem line{};
    line.start = center;
    line.end = center - (glm::normalize(light.direction) * 4.0f);
    line.color = glm::vec4(1.0f, 0.95f, 0.1f, 1.0f);
    line.layer = 0;
    render.submitDebugLine(line);
}

} // namespace

int main(int argc, char** argv) {
    try {
        const SandboxOptions options = ParseOptions(argc, argv);
        karma::logging::ConfigureLogPatterns(false);
        spdlog::set_level(options.verbose ? spdlog::level::debug : spdlog::level::info);
        if (!options.trace_channels.empty()) {
            karma::logging::EnableTraceChannels(options.trace_channels);
        }

        karma::data::DataPathSpec data_spec{};
        data_spec.appName = "bz3";
        data_spec.dataDirEnvVar = "BZ3_DATA_DIR";
        data_spec.requiredDataMarker = "common/config.json";
        karma::data::SetDataPathSpec(data_spec);
        const std::filesystem::path repo_data_root = std::filesystem::current_path() / "data";
        if (std::filesystem::exists(repo_data_root / "common/config.json")) {
            karma::data::SetDataRootOverride(repo_data_root);
        }

        karma::platform::WindowConfig window_config{};
        window_config.title = "Renderer Shadow Sandbox";
        window_config.width = options.width;
        window_config.height = options.height;
        window_config.resizable = true;
        window_config.preferredVideoDriver = options.preferred_video_driver;

        std::unique_ptr<karma::platform::Window> window = karma::platform::CreateWindow(window_config);
        if (!window) {
            throw std::runtime_error("Failed to create sandbox window");
        }
        window->setVsync(true);

        std::unique_ptr<karma::renderer::GraphicsDevice> graphics =
            std::make_unique<karma::renderer::GraphicsDevice>(*window, options.backend);
        if (!graphics || !graphics->isValid()) {
            throw std::runtime_error("Failed to create graphics device for shadow sandbox");
        }

        std::unique_ptr<karma::renderer::RenderSystem> render =
            std::make_unique<karma::renderer::RenderSystem>(*graphics);

        karma::ecs::World world{};
        karma::scene::Scene scene(world);
        render->setWorld(&world);

        const karma::renderer::MeshData plane_mesh = BuildUnitPlaneMesh();
        const karma::renderer::MeshData cube_mesh = BuildUnitCubeMesh();
        const karma::renderer::MeshId plane_mesh_id = graphics->createMesh(plane_mesh);
        const karma::renderer::MeshId cube_mesh_id = graphics->createMesh(cube_mesh);
        const karma::renderer::MaterialId ground_material_id = graphics->createMaterial(
            MakeMaterial(glm::vec4(0.24f, 0.50f, 0.26f, 1.0f), 0.92f, 0.0f));
        const karma::renderer::MaterialId cube_material_id = graphics->createMaterial(
            MakeMaterial(glm::vec4(0.72f, 0.52f, 0.40f, 1.0f), 0.75f, 0.0f));
        if (plane_mesh_id == karma::renderer::kInvalidMesh ||
            cube_mesh_id == karma::renderer::kInvalidMesh ||
            ground_material_id == karma::renderer::kInvalidMaterial ||
            cube_material_id == karma::renderer::kInvalidMaterial) {
            throw std::runtime_error("Failed to create sandbox mesh/material resources");
        }

        std::vector<ShadowEntity> shadow_entities{};
        shadow_entities.reserve(static_cast<std::size_t>(options.ground_tiles * options.ground_tiles) + 8u);

        const int tiles = std::max(1, options.ground_tiles);
        const float full_width = options.ground_extent * 2.0f;
        const float tile_width = full_width / static_cast<float>(tiles);
        for (int z = 0; z < tiles; ++z) {
            for (int x = 0; x < tiles; ++x) {
                const float world_x = -options.ground_extent + (tile_width * (static_cast<float>(x) + 0.5f));
                const float world_z = -options.ground_extent + (tile_width * (static_cast<float>(z) + 0.5f));
                AddRenderableEntity(
                    world,
                    shadow_entities,
                    plane_mesh,
                    plane_mesh_id,
                    ground_material_id,
                    BuildTransform(glm::vec3(world_x, 0.0f, world_z), glm::vec3(tile_width, 1.0f, tile_width)),
                    true,
                    false);
            }
        }

        AddRenderableEntity(
            world,
            shadow_entities,
            cube_mesh,
            cube_mesh_id,
            cube_material_id,
            BuildTransform(glm::vec3(-4.0f, 1.0f, -2.5f), glm::vec3(2.0f, 2.0f, 2.0f)),
            false,
            true);
        AddRenderableEntity(
            world,
            shadow_entities,
            cube_mesh,
            cube_mesh_id,
            cube_material_id,
            BuildTransform(glm::vec3(0.0f, 1.25f, 0.0f), glm::vec3(2.2f, 2.5f, 2.2f)),
            false,
            true);
        AddRenderableEntity(
            world,
            shadow_entities,
            cube_mesh,
            cube_mesh_id,
            cube_material_id,
            BuildTransform(glm::vec3(4.0f, 1.0f, 3.0f), glm::vec3(2.0f, 2.0f, 2.0f)),
            false,
            true);
        scene.updateWorldTransforms();

        karma::renderer::DirectionalLightData light{};
        light.direction = glm::normalize(options.sun_direction);
        light.color = glm::vec4(1.0f, 0.97f, 0.92f, 1.0f);
        light.ambient = glm::vec4(0.03f, 0.03f, 0.035f, 1.0f);
        light.unlit = 0.0f;
        light.shadow.enabled = true;
        light.shadow.map_size = options.shadow_map_size;
        light.shadow.strength = options.shadow_strength;
        light.shadow.extent = options.shadow_extent;
        light.shadow.pcf_radius = options.shadow_pcf_radius;
        light.shadow.bias = options.shadow_bias;

        karma::renderer::EnvironmentLightingData environment{};
        environment.enabled = true;
        environment.sky_color = glm::vec4(0.58f, 0.66f, 0.78f, 1.0f);
        environment.ground_color = glm::vec4(0.08f, 0.10f, 0.08f, 1.0f);
        environment.diffuse_strength = 0.25f;
        environment.specular_strength = 0.05f;
        environment.skybox_exposure = 0.70f;
        render->setEnvironmentLighting(environment);

        OrbitCameraState orbit{};
        float sun_azimuth = 0.0f;
        float sun_elevation = -45.0f;
        AnglesFromDirection(light.direction, sun_azimuth, sun_elevation);

        const Clock::time_point started_at = Clock::now();
        Clock::time_point previous_tick = started_at;
        Clock::time_point last_diagnostics = started_at;
        int frame_count_since_diag = 0;
        float frame_dt_sum_since_diag = 0.0f;
        float frame_dt_max_since_diag = 0.0f;

        spdlog::info(
            "[sandbox] backend={} ground_tiles={} ground_extent={} shadow_map={} pcf={} strength={:.2f} bias={:.4f}",
            graphics->backendName(),
            options.ground_tiles,
            options.ground_extent,
            light.shadow.map_size,
            light.shadow.pcf_radius,
            light.shadow.strength,
            light.shadow.bias);
        if (!options.preferred_video_driver.empty()) {
            spdlog::info("[sandbox] preferred_video_driver={}", options.preferred_video_driver);
        }
        spdlog::info(
            "[sandbox] controls: arrows orbit, pageup/pagedown zoom, a/d sun azimuth, w/s sun elevation, esc quit");

        bool running = true;
        while (running && !window->shouldClose()) {
            const Clock::time_point now = Clock::now();
            const float dt = std::chrono::duration<float>(now - previous_tick).count();
            previous_tick = now;

            window->pollEvents();
            if (ShouldQuitFromEvents(window->events())) {
                running = false;
            }

            UpdateInteractiveControls(*window, dt, orbit, sun_azimuth, sun_elevation);
            light.direction = DirectionFromAngles(sun_azimuth, sun_elevation);

            const karma::renderer::CameraData camera = BuildCamera(orbit);
            render->setCamera(camera);
            render->setDirectionalLight(light);

            int fb_width = 0;
            int fb_height = 0;
            window->getFramebufferSize(fb_width, fb_height);
            scene.updateWorldTransforms();
            render->beginFrame(fb_width, fb_height, dt);
            SubmitLightDebugLine(*render, light, orbit.target);
            render->renderFrame();
            render->endFrame();
            window->clearEvents();

            ++frame_count_since_diag;
            frame_dt_sum_since_diag += dt;
            frame_dt_max_since_diag = std::max(frame_dt_max_since_diag, dt);

            if ((now - last_diagnostics) >= std::chrono::seconds(1)) {
                const float frame_dt_avg =
                    (frame_count_since_diag > 0)
                        ? (frame_dt_sum_since_diag / static_cast<float>(frame_count_since_diag))
                        : 0.0f;
                LogShadowDiagnostics(
                    graphics->backendName(),
                    light,
                    shadow_entities,
                    world,
                    options.ground_extent,
                    frame_dt_avg,
                    frame_dt_max_since_diag);
                frame_count_since_diag = 0;
                frame_dt_sum_since_diag = 0.0f;
                frame_dt_max_since_diag = 0.0f;
                last_diagnostics = now;
            }

            if (options.duration_seconds > 0.0f) {
                const float elapsed = std::chrono::duration<float>(now - started_at).count();
                if (elapsed >= options.duration_seconds) {
                    running = false;
                }
            }
        }

        graphics->destroyMaterial(cube_material_id);
        graphics->destroyMaterial(ground_material_id);
        graphics->destroyMesh(cube_mesh_id);
        graphics->destroyMesh(plane_mesh_id);
        return 0;
    } catch (const std::exception& ex) {
        spdlog::error("renderer_shadow_sandbox: {}", ex.what());
        return 1;
    }
}

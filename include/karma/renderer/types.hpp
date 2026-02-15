#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>
#include <glm/glm.hpp>

namespace karma::renderer {

using MeshId = uint32_t;
using MaterialId = uint32_t;
using TextureId = uint32_t;
using RenderTargetId = uint32_t;
using LayerId = uint32_t;

constexpr MeshId kInvalidMesh = 0;
constexpr MaterialId kInvalidMaterial = 0;
constexpr TextureId kInvalidTexture = 0;
constexpr RenderTargetId kDefaultRenderTarget = 0;

enum class MaterialAlphaMode : uint8_t {
    Opaque = 0,
    Mask = 1,
    Blend = 2,
};

struct MeshData {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;
    std::vector<uint32_t> indices;
    struct TextureData {
        int width = 0;
        int height = 0;
        int channels = 0;
        std::vector<uint8_t> pixels;
    };
    std::optional<TextureData> albedo;
};

struct MaterialDesc {
    glm::vec4 base_color{1.0f, 1.0f, 1.0f, 1.0f};
    std::optional<MeshData::TextureData> albedo;
    float metallic_factor = 0.0f;
    float roughness_factor = 1.0f;
    float normal_scale = 1.0f;
    float occlusion_strength = 1.0f;
    glm::vec3 emissive_color{0.0f, 0.0f, 0.0f};
    MaterialAlphaMode alpha_mode = MaterialAlphaMode::Opaque;
    float alpha_cutoff = 0.5f;
    bool double_sided = false;
    std::optional<MeshData::TextureData> metallic_roughness;
    std::optional<MeshData::TextureData> emissive;
    std::optional<MeshData::TextureData> normal;
    std::optional<MeshData::TextureData> occlusion;
};

struct CameraData {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 target{0.0f, 0.0f, -1.0f};
    float fov_y_degrees = 60.0f;
    float near_clip = 0.1f;
    float far_clip = 2000.0f;
};

struct DirectionalLightData {
    enum class ShadowExecutionMode : uint8_t {
        CpuReference = 0,
        GpuDefault = 1,
    };

    static constexpr const char* ShadowExecutionModeToken(ShadowExecutionMode mode) {
        switch (mode) {
            case ShadowExecutionMode::CpuReference:
                return "cpu_reference";
            case ShadowExecutionMode::GpuDefault:
                return "gpu_default";
            default:
                return "cpu_reference";
        }
    }

    static constexpr bool TryParseShadowExecutionMode(std::string_view token, ShadowExecutionMode* out) {
        if (!out) {
            return false;
        }
        if (token == "cpu_reference") {
            *out = ShadowExecutionMode::CpuReference;
            return true;
        }
        if (token == "gpu_default") {
            *out = ShadowExecutionMode::GpuDefault;
            return true;
        }
        return false;
    }

    struct ShadowDesc {
        bool enabled = true;
        float strength = 0.65f;
        float bias = 0.0015f;
        float receiver_bias_scale = 0.08f;
        float normal_bias_scale = 0.35f;
        float raster_depth_bias = 0.0f;
        float raster_slope_bias = 0.0f;
        float extent = 24.0f;
        int map_size = 256;
        int pcf_radius = 1;
        int triangle_budget = 4096;
        int update_every_frames = 1;
        ShadowExecutionMode execution_mode = ShadowExecutionMode::CpuReference;
    };

    glm::vec3 direction{0.3f, 0.7f, -0.5f};
    glm::vec4 color{0.8f, 0.8f, 0.8f, 1.0f};
    glm::vec4 ambient{0.25f, 0.25f, 0.25f, 1.0f};
    float unlit = 1.0f;
    ShadowDesc shadow{};
};

struct EnvironmentLightingData {
    bool enabled = true;
    glm::vec4 sky_color{0.56f, 0.66f, 0.88f, 1.0f};
    glm::vec4 ground_color{0.14f, 0.14f, 0.16f, 1.0f};
    float diffuse_strength = 0.75f;
    float specular_strength = 0.20f;
    float skybox_exposure = 1.0f;
};

struct DrawItem {
    MeshId mesh = kInvalidMesh;
    MaterialId material = kInvalidMaterial;
    glm::mat4 transform{1.0f};
    LayerId layer = 0;
    bool casts_shadow = true;
};

struct DebugLineItem {
    glm::vec3 start{0.0f, 0.0f, 0.0f};
    glm::vec3 end{0.0f, 0.0f, 0.0f};
    glm::vec4 color{1.0f, 0.0f, 0.0f, 1.0f};
    LayerId layer = 0;
};

} // namespace karma::renderer

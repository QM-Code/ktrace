#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
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
};

struct CameraData {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 target{0.0f, 0.0f, -1.0f};
    float fov_y_degrees = 60.0f;
    float near_clip = 0.1f;
    float far_clip = 2000.0f;
};

struct DrawItem {
    MeshId mesh = kInvalidMesh;
    MaterialId material = kInvalidMaterial;
    glm::mat4 transform{1.0f};
    LayerId layer = 0;
};

} // namespace karma::renderer

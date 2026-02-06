#include "karma/ui/ui_system.hpp"

#include "karma/common/config_helpers.hpp"
#include "karma/common/logging.hpp"
#include "karma/renderer/device.hpp"
#include "karma/renderer/layers.hpp"
#include "karma/renderer/render_system.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <string>

namespace karma::ui {

namespace {

renderer::MeshData::TextureData BuildOverlayTexture(int width, int height) {
    renderer::MeshData::TextureData tex{};
    tex.width = width;
    tex.height = height;
    tex.channels = 4;
    tex.pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool checker = ((x / 16) + (y / 16)) % 2 == 0;
            const uint8_t grain = static_cast<uint8_t>((x * 13 + y * 7) & 0xff);
            const uint8_t r = checker ? static_cast<uint8_t>(220u - (grain / 4u)) : static_cast<uint8_t>(40u + (grain / 3u));
            const uint8_t g = checker ? static_cast<uint8_t>(80u + (grain / 3u)) : static_cast<uint8_t>(170u - (grain / 4u));
            const uint8_t b = checker ? static_cast<uint8_t>(40u + (grain / 5u)) : static_cast<uint8_t>(220u - (grain / 6u));
            const size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
            tex.pixels[idx + 0] = r;
            tex.pixels[idx + 1] = g;
            tex.pixels[idx + 2] = b;
            tex.pixels[idx + 3] = 255;
        }
    }

    return tex;
}

renderer::MeshData BuildOverlayQuadMesh() {
    renderer::MeshData mesh{};
    mesh.positions = {
        {-0.5f,  0.5f, 0.0f},
        { 0.5f,  0.5f, 0.0f},
        { 0.5f, -0.5f, 0.0f},
        {-0.5f, -0.5f, 0.0f},
    };
    mesh.normals = {
        {0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 1.0f},
    };
    mesh.uvs = {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f},
    };
    // Include both winding orders so the overlay stays visible across backend cull conventions.
    mesh.indices = {0, 1, 2, 0, 2, 3, 2, 1, 0, 3, 2, 0};
    return mesh;
}

} // namespace

void UiSystem::init(renderer::GraphicsDevice& graphics) {
    if (initialized_) {
        return;
    }
    initialized_ = true;

    overlay_test_enabled_ = config::ReadBoolConfig({"ui.overlayTest.Enabled"}, true);
    overlay_distance_ = config::ReadFloatConfig({"ui.overlayTest.Distance"}, 0.75f);
    overlay_width_ = config::ReadFloatConfig({"ui.overlayTest.Width"}, 1.2f);
    overlay_height_ = config::ReadFloatConfig({"ui.overlayTest.Height"}, 0.7f);

    if (!overlay_test_enabled_) {
        KARMA_TRACE("ui.system", "UiSystem: overlay test disabled");
        return;
    }

    renderer::MaterialDesc material{};
    material.base_color = {1.0f, 1.0f, 1.0f, 1.0f};
    material.albedo = BuildOverlayTexture(128, 128);
    overlay_material_ = graphics.createMaterial(material);

    renderer::MeshData quad = BuildOverlayQuadMesh();
    overlay_mesh_ = graphics.createMesh(quad);

    if (overlay_mesh_ == renderer::kInvalidMesh || overlay_material_ == renderer::kInvalidMaterial) {
        KARMA_TRACE("ui.system",
                    "UiSystem: overlay test resource creation failed mesh={} material={}",
                    overlay_mesh_,
                    overlay_material_);
        overlay_test_enabled_ = false;
        if (overlay_mesh_ != renderer::kInvalidMesh) {
            graphics.destroyMesh(overlay_mesh_);
            overlay_mesh_ = renderer::kInvalidMesh;
        }
        if (overlay_material_ != renderer::kInvalidMaterial) {
            graphics.destroyMaterial(overlay_material_);
            overlay_material_ = renderer::kInvalidMaterial;
        }
        return;
    }

    KARMA_TRACE("ui.system",
                "UiSystem: overlay test ready mesh={} material={} size=({:.2f},{:.2f}) distance={:.2f}",
                overlay_mesh_,
                overlay_material_,
                overlay_width_,
                overlay_height_,
                overlay_distance_);
}

void UiSystem::shutdown(renderer::GraphicsDevice& graphics) {
    if (overlay_mesh_ != renderer::kInvalidMesh) {
        graphics.destroyMesh(overlay_mesh_);
        overlay_mesh_ = renderer::kInvalidMesh;
    }
    if (overlay_material_ != renderer::kInvalidMaterial) {
        graphics.destroyMaterial(overlay_material_);
        overlay_material_ = renderer::kInvalidMaterial;
    }
    initialized_ = false;
}

void UiSystem::beginFrame(float dt) {
    frame_dt_ = dt;
}

void UiSystem::update(renderer::RenderSystem& render) {
    (void)frame_dt_;

    if (!initialized_ ||
        !overlay_test_enabled_ ||
        overlay_mesh_ == renderer::kInvalidMesh ||
        overlay_material_ == renderer::kInvalidMaterial) {
        return;
    }

    const renderer::CameraData& camera = render.camera();
    glm::vec3 forward = camera.target - camera.position;
    const float forward_len = glm::length(forward);
    if (forward_len <= 1e-5f) {
        return;
    }
    forward /= forward_len;

    glm::vec3 up_ref{0.0f, 1.0f, 0.0f};
    if (std::abs(glm::dot(forward, up_ref)) > 0.99f) {
        up_ref = {1.0f, 0.0f, 0.0f};
    }
    const glm::vec3 right = glm::normalize(glm::cross(forward, up_ref));
    const glm::vec3 up = glm::normalize(glm::cross(right, forward));
    const glm::vec3 center = camera.position + (forward * overlay_distance_);

    glm::mat4 transform{1.0f};
    transform[0] = glm::vec4(right * overlay_width_, 0.0f);
    transform[1] = glm::vec4(up * overlay_height_, 0.0f);
    transform[2] = glm::vec4(forward, 0.0f);
    transform[3] = glm::vec4(center, 1.0f);

    renderer::DrawItem overlay{};
    overlay.mesh = overlay_mesh_;
    overlay.material = overlay_material_;
    overlay.layer = renderer::kLayerUI;
    overlay.transform = transform;
    render.submit(overlay);

    KARMA_TRACE_CHANGED("ui.system",
                        std::to_string(overlay_mesh_) + ":" + std::to_string(overlay_material_),
                        "UiSystem: overlay submit mesh={} material={} layer={}",
                        overlay_mesh_,
                        overlay_material_,
                        renderer::kLayerUI);
}

void UiSystem::endFrame() {
}

} // namespace karma::ui

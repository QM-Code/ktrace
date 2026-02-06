#pragma once

#include "karma/renderer/types.hpp"

namespace karma::renderer {
class GraphicsDevice;
class RenderSystem;
}

namespace karma::ui {

enum class Backend {
    ImGui,
    RmlUi
};

class UiSystem {
 public:
    void setBackend(Backend backend) { backend_ = backend; }
    Backend backend() const { return backend_; }

    void init(renderer::GraphicsDevice& graphics);
    void shutdown(renderer::GraphicsDevice& graphics);
    void beginFrame(float dt);
    void update(renderer::RenderSystem& render);
    void endFrame();

 private:
    renderer::MeshId overlay_mesh_ = renderer::kInvalidMesh;
    renderer::MaterialId overlay_material_ = renderer::kInvalidMaterial;
    float overlay_distance_ = 1.0f;
    float overlay_width_ = 1.2f;
    float overlay_height_ = 0.7f;
    float frame_dt_ = 0.0f;
    Backend backend_ = Backend::RmlUi;
    bool initialized_ = false;
    bool overlay_test_enabled_ = true;
};

} // namespace karma::ui

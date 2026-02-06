#pragma once

#include "karma/renderer/types.hpp"
#include "ui/ui_backend.hpp"

#include <cstdint>
#include <memory>
#include <string>

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
    UiSystem() = default;
    ~UiSystem();

    void setBackend(Backend backend) { backend_ = backend; }
    Backend backend() const { return backend_; }

    void init(renderer::GraphicsDevice& graphics);
    void shutdown(renderer::GraphicsDevice& graphics);
    void beginFrame(float dt, const std::vector<platform::Event>& events);
    void update(renderer::RenderSystem& render);
    void endFrame();
    bool wantsMouseCapture() const { return wants_mouse_capture_; }
    bool wantsKeyboardCapture() const { return wants_keyboard_capture_; }

 private:
    renderer::GraphicsDevice* graphics_ = nullptr;
    std::unique_ptr<UiBackend> backend_impl_{};
    renderer::MeshId overlay_mesh_ = renderer::kInvalidMesh;
    renderer::MaterialId overlay_material_ = renderer::kInvalidMaterial;
    uint64_t overlay_texture_revision_ = 0;
    float overlay_distance_ = 1.0f;
    float overlay_width_ = 1.2f;
    float overlay_height_ = 0.7f;
    float frame_dt_ = 0.0f;
    Backend backend_ = Backend::RmlUi;
    bool initialized_ = false;
    bool overlay_fallback_enabled_ = true;
    renderer::MeshData::TextureData fallback_texture_{};
    uint64_t fallback_texture_revision_ = 1;
    bool fallback_texture_ready_ = false;
    std::string overlay_source_name_ = "none";
    bool wants_mouse_capture_ = false;
    bool wants_keyboard_capture_ = false;
};

} // namespace karma::ui

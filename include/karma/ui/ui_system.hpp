#pragma once

#include "karma/platform/events.hpp"
#include "karma/renderer/types.hpp"
#include "karma/ui/ui_draw_context.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace karma::renderer {
class GraphicsDevice;
class RenderSystem;
}

namespace karma::ui {

class UiBackend;
struct UiBackendDeleter {
    void operator()(UiBackend* backend) const;
};

enum class Backend {
    ImGui,
    RmlUi
};

class UiSystem : public UiDrawContext {
 public:
    UiSystem() = default;
    ~UiSystem();

    void setBackend(Backend backend);
    Backend backend() const { return backend_; }

    void init(renderer::GraphicsDevice& graphics);
    void shutdown(renderer::GraphicsDevice& graphics);
    void beginFrame(float dt, const std::vector<platform::Event>& events);
    void update(renderer::RenderSystem& render);
    void endFrame();
    UiDrawContext& drawContext() { return *this; }
    bool wantsMouseCapture() const { return wants_mouse_capture_; }
    bool wantsKeyboardCapture() const { return wants_keyboard_capture_; }

    UiBackendKind backendKind() const override;
    void addImGuiDraw(ImGuiDrawCallback callback) override;
    void addRmlUiDraw(RmlUiDrawCallback callback) override;
    void addTextPanel(TextPanel panel) override;
    size_t imguiDrawCount() const override { return imgui_draw_callbacks_.size(); }
    size_t rmluiDrawCount() const override { return rmlui_draw_callbacks_.size(); }
    size_t textPanelCount() const override { return text_panels_.size(); }

 private:
    renderer::GraphicsDevice* graphics_ = nullptr;
    std::unique_ptr<UiBackend, UiBackendDeleter> backend_impl_{};
    renderer::MeshId overlay_mesh_ = renderer::kInvalidMesh;
    renderer::MaterialId overlay_material_ = renderer::kInvalidMaterial;
    uint64_t overlay_texture_revision_ = 0;
    float overlay_distance_ = 1.0f;
    float overlay_width_ = 1.2f;
    float overlay_height_ = 0.7f;
    float frame_dt_ = 0.0f;
    Backend backend_ = Backend::RmlUi;
    bool backend_forced_ = false;
    bool initialized_ = false;
    bool capture_input_enabled_ = false;
    bool overlay_fallback_enabled_ = true;
    renderer::MeshData::TextureData fallback_texture_{};
    uint64_t fallback_texture_revision_ = 1;
    bool fallback_texture_ready_ = false;
    std::string overlay_source_name_ = "none";
    bool wants_mouse_capture_ = false;
    bool wants_keyboard_capture_ = false;
    std::vector<ImGuiDrawCallback> imgui_draw_callbacks_{};
    std::vector<RmlUiDrawCallback> rmlui_draw_callbacks_{};
    std::vector<TextPanel> text_panels_{};
};

} // namespace karma::ui

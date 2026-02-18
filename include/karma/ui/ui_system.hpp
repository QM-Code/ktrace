#pragma once

#include "karma/ui/backend.hpp"
#include "karma/ui/ui_draw_context.hpp"
#include "karma/window/events.hpp"

#include <memory>
#include <vector>

namespace karma::renderer {
class GraphicsDevice;
class RenderSystem;
}

namespace karma::ui {

class UiSystem : public UiDrawContext {
 public:
    UiSystem();
    ~UiSystem();

    void setBackend(backend::BackendKind backend);
    backend::BackendKind backend() const;

    void init(renderer::GraphicsDevice& graphics);
    void shutdown(renderer::GraphicsDevice& graphics);
    void beginFrame(float dt, const std::vector<window::Event>& events);
    void update(renderer::RenderSystem& render);
    void endFrame();
    UiDrawContext& drawContext() { return *this; }
    bool wantsMouseCapture() const;
    bool wantsKeyboardCapture() const;

    UiBackendKind backendKind() const override;
    void addImGuiDraw(ImGuiDrawCallback callback) override;
    void addRmlUiDraw(RmlUiDrawCallback callback) override;
    void addTextPanel(TextPanel panel) override;
    size_t imguiDrawCount() const override;
    size_t rmluiDrawCount() const override;
    size_t textPanelCount() const override;

 private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};

} // namespace karma::ui

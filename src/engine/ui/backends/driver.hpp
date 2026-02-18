#pragma once

#include "karma/renderer/types.hpp"
#include "karma/ui/backend.hpp"
#include "karma/ui/ui_draw_context.hpp"
#include "karma/window/events.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace karma::ui::backend {

struct OverlayFrame {
    const renderer::MeshData::TextureData* texture = nullptr;
    uint64_t texture_revision = 0;
    float distance = 0.75f;
    float width = 1.2f;
    float height = 0.7f;
    bool wants_mouse_capture = false;
    bool wants_keyboard_capture = false;
    bool allow_fallback = true;
};

class BackendDriver {
 public:
    virtual ~BackendDriver() = default;
    virtual const char* name() const = 0;
    virtual bool init() = 0;
    virtual void shutdown() = 0;
    virtual void beginFrame(float dt, const std::vector<window::Event>& events) = 0;
    virtual void build(const std::vector<UiDrawContext::ImGuiDrawCallback>& imgui_draw_callbacks,
                       const std::vector<UiDrawContext::RmlUiDrawCallback>& rmlui_draw_callbacks,
                       const std::vector<UiDrawContext::TextPanel>& text_panels,
                       OverlayFrame& out) = 0;
};

std::unique_ptr<BackendDriver> CreateBackend(BackendKind preferred = BackendKind::Auto,
                                             BackendKind* out_selected = nullptr);

} // namespace karma::ui::backend

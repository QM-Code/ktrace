#pragma once

#include "karma/platform/events.hpp"
#include "karma/renderer/types.hpp"
#include "karma/ui/ui_draw_context.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace karma::ui {

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
    virtual void beginFrame(float dt, const std::vector<platform::Event>& events) = 0;
    virtual void build(const std::vector<UiDrawContext::ImGuiDrawCallback>& imgui_draw_callbacks,
                       const std::vector<UiDrawContext::RmlUiDrawCallback>& rmlui_draw_callbacks,
                       const std::vector<UiDrawContext::TextPanel>& text_panels,
                       OverlayFrame& out) = 0;
};

std::unique_ptr<BackendDriver> CreateSoftwareBackend();
std::unique_ptr<BackendDriver> CreateImGuiBackend();
std::unique_ptr<BackendDriver> CreateRmlUiBackend();

} // namespace karma::ui

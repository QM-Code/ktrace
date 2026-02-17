#include "ui/backends/imgui/internal.hpp"

#include "karma/common/logging.hpp"

namespace karma::ui::imgui {
namespace {

class StubBackend final : public BackendDriver {
 public:
    const char* name() const override {
        return "imgui";
    }

    bool init() override {
        KARMA_TRACE("ui.system.imgui", "Backend[{}]: imgui support not compiled", name());
        return false;
    }

    void shutdown() override {}

    void beginFrame(float dt, const std::vector<platform::Event>& events) override {
        (void)dt;
        (void)events;
    }

    void build(const std::vector<UiDrawContext::ImGuiDrawCallback>& imgui_draw_callbacks,
               const std::vector<UiDrawContext::RmlUiDrawCallback>& rmlui_draw_callbacks,
               const std::vector<UiDrawContext::TextPanel>& text_panels,
               OverlayFrame& out) override {
        (void)imgui_draw_callbacks;
        (void)rmlui_draw_callbacks;
        (void)text_panels;
        (void)out;
    }
};

} // namespace

std::unique_ptr<BackendDriver> CreateStubBackend() {
    return std::make_unique<StubBackend>();
}

} // namespace karma::ui::imgui

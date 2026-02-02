#include "ui/core/backend.hpp"

#include "spdlog/spdlog.h"
#include "ui/console/null_console.hpp"

#if defined(KARMA_UI_BACKEND_IMGUI)
#include "ui/frontends/imgui/backend.hpp"
#elif defined(KARMA_UI_BACKEND_RMLUI)
#include "ui/frontends/rmlui/backend.hpp"
#else
#error "BZ3 UI backend not set. Define KARMA_UI_BACKEND_IMGUI or KARMA_UI_BACKEND_RMLUI."
#endif

namespace ui_backend {

namespace {

class NullBackend final : public Backend {
public:
    ui::ConsoleInterface &console() override { return consoleImpl; }
    const ui::ConsoleInterface &console() const override { return consoleImpl; }
    void handleEvents(const std::vector<platform::Event> &events) override { (void)events; }
    void update() override {}
    void reloadFonts() override {}
    void setHudModel(const ui::HudModel &model) override { (void)model; }
    void addConsoleLine(const std::string &playerName, const std::string &line) override {
        (void)playerName;
        (void)line;
    }
    std::string getChatInputBuffer() const override { return {}; }
    void clearChatInputBuffer() override {}
    void focusChatInput() override {}
    bool getChatInputFocus() const override { return false; }
    bool consumeKeybindingsReloadRequest() override { return false; }
    std::optional<ui::QuickMenuAction> consumeQuickMenuAction() override { return std::nullopt; }
    void setRendererBridge(const ui::RendererBridge *bridge) override { (void)bridge; }
    ui::RenderOutput getRenderOutput() const override { return {}; }
    float getRenderBrightness() const override { return 1.0f; }
    bool isUiInputEnabled() const override { return false; }
    const char *name() const override { return "null"; }
    ui::HudRenderState getHudRenderState() const override { return {}; }

private:
    ui::NullConsole consoleImpl;
};

} // namespace

std::unique_ptr<Backend> CreateUiBackend(platform::Window &window) {
    if (const char* noUi = std::getenv("KARMA_NO_UI"); noUi && noUi[0] != '\0') {
        spdlog::warn("UiSystem: UI disabled via KARMA_NO_UI");
        return std::make_unique<NullBackend>();
    }
#if defined(KARMA_UI_BACKEND_IMGUI)
    return std::make_unique<ImGuiBackend>(window);
#elif defined(KARMA_UI_BACKEND_RMLUI)
    return std::make_unique<RmlUiBackend>(window);
#endif
}

} // namespace ui_backend

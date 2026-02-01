#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ui/core/backend.hpp"
#include "ui/frontends/rmlui/console/panels/panel_settings.hpp"

namespace platform {
class Window;
}

namespace Rml {
class Event;
}

namespace ui {
class RmlUiHud;
class RmlUiConsole;
}

namespace ui_backend {

class RmlUiBackend final : public Backend {
public:
    explicit RmlUiBackend(platform::Window &window);
    ~RmlUiBackend() override;

    ui::ConsoleInterface &console() override;
    const ui::ConsoleInterface &console() const override;
    void handleEvents(const std::vector<platform::Event> &events) override;
    void update() override;
    void reloadFonts() override;
    bool buildDrawData(karma::app::UIContext &ctx) override;

    void setHudModel(const ui::HudModel &model) override;
    void addConsoleLine(const std::string &playerName, const std::string &line) override;
    std::string getChatInputBuffer() const override;
    void clearChatInputBuffer() override;
    void focusChatInput() override;
    bool getChatInputFocus() const override;
    bool consumeKeybindingsReloadRequest() override;
    std::optional<ui::QuickMenuAction> consumeQuickMenuAction() override;
    void setRendererBridge(const ui::RendererBridge *bridge) override;
    ui::RenderOutput getRenderOutput() const override;
    float getRenderBrightness() const override;
    bool isRenderBrightnessDragActive() const override;
    void setActiveTab(const std::string &tabKey);
    bool isUiInputEnabled() const override;
    const char *name() const override { return "rmlui"; }
    ui::HudRenderState getHudRenderState() const override { return lastHudRenderState; }

private:
    platform::Window *windowRef = nullptr;
    struct RmlUiState;
    std::unique_ptr<RmlUiState> state;
    std::unique_ptr<ui::RmlUiConsole> consoleView;
    ui::HudModel hudModel;
    const ui::RendererBridge *rendererBridge = nullptr;
    ui::RmlUiPanelSettings *settingsPanel = nullptr;
    ui::HudRenderState lastHudRenderState{};
    void loadConfiguredFonts(const std::string &language);
    void loadConsoleDocument();
    void loadHudDocument();
    const std::string &cachedTwemojiMarkup(const std::string &text);
};

} // namespace ui_backend
